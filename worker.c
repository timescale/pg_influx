#include "worker.h"

#include <postgres.h>
#include <fmgr.h>

#include <executor/spi.h>
#include <funcapi.h>
#include <miscadmin.h>
#include <pgstat.h>
#include <postmaster/bgworker.h>
#include <storage/ipc.h>
#include <storage/latch.h>
#include <utils/builtins.h>
#include <utils/elog.h>
#include <utils/guc.h>
#include <utils/lsyscache.h>

#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include "network.h"

void WorkerMain(Datum dbid) pg_attribute_noreturn();

typedef struct WorkerArgs {
  Oid namespace_id;
  char service[NI_MAXSERV];
} WorkerArgs;

/* Check that sizeof(WorkerArgs) < BGW_EXTRALEN */
static char c1[BGW_EXTRALEN - sizeof(WorkerArgs)] pg_attribute_unused();

static volatile sig_atomic_t ReloadConfig = false;
static volatile sig_atomic_t ShutdownWorker = false;

PG_FUNCTION_INFO_V1(worker_launch);

/**
 * Process one packet of lines.
 */
static void ProcessPacket(char *buffer, size_t bytes, Oid nspid) {
  buffer[bytes] = '\0';
  ereport(LOG, (errmsg("received %ld bytes", bytes),
                errdetail("packet: %s", buffer)));
}

/* Signal handler for SIGTERM */
static void WorkerSigterm(SIGNAL_ARGS) {
  int save_errno = errno;
  ShutdownWorker = true;
  SetLatch(MyLatch);
  errno = save_errno;
}

/* Signal handler for SIGHUP */
static void WorkerSighup(SIGNAL_ARGS) {
  int save_errno = errno;
  ReloadConfig = true;
  SetLatch(MyLatch);
  errno = save_errno;
}

/**
 * Initalize a worker before registering it.
 */
static void WorkerInit(BackgroundWorker *worker, WorkerArgs *args) {
  memset(worker, 0, sizeof(*worker));
  worker->bgw_start_time = BgWorkerStart_RecoveryFinished;
  worker->bgw_restart_time = BGW_NEVER_RESTART;
  sprintf(worker->bgw_library_name, BGW_LIBRARY_NAME);
  sprintf(worker->bgw_function_name, BGW_FUNCTION_NAME);
  snprintf(worker->bgw_name, BGW_MAXLEN, "Influx listener for schema %s",
           get_namespace_name(args->namespace_id));
  snprintf(worker->bgw_type, BGW_MAXLEN, "Influx line protocol listener");
  worker->bgw_main_arg = MyDatabaseId;
  memcpy(worker->bgw_extra, args, sizeof(*args));
}

/**
 * Main worker function.
 *
 * The worker will read from a socket and write to a tables in a the
 * given schema.
 *
 * From `MyBgworkerEntry` we expect the following fields to be set up:
 *
 * - Field `bgw_extra` will contain the worker arguments in a
 *   WorkerArgs structure.
 *
 * - The handle for the shared memory segment is passed as a parameter
 *   to this function.
 */
void WorkerMain(Datum arg) {
  /* MTU for the wireless interface. We don't bother about digging up
     the actual MTU here and just pick something that is common. */
  const Oid database_id = DatumGetInt32(arg);
  int sfd;
  char buffer[1500];
  WorkerArgs *args = (WorkerArgs *)&MyBgworkerEntry->bgw_extra;

  /* Establish signal handlers; once that's done, unblock signals. */
  pqsignal(SIGTERM, WorkerSigterm);
  pqsignal(SIGHUP, WorkerSighup);
  BackgroundWorkerUnblockSignals();

  sfd = CreateSocket(NULL, args->service, &UdpRecvSocket, NULL, 0);
  if (sfd == -1) {
    ereport(LOG, (errcode_for_socket_access(),
                  errmsg("could not create socket for service '%s': %m",
                         args->service)));
    proc_exit(1);
  }

  pgstat_report_activity(STATE_RUNNING, "worker initialized");

  ereport(LOG, (errmsg("worker initialized"),
                errdetail("service=%s, database_id=%u, namespace_id=%u",
                          args->service, database_id, args->namespace_id)));

  /* This is the same approach as used in pgstats.c: We read the
     packets in two loops. One outer that will block when there is no
     data received, and one inner loop that will read packets as long
     as possible in non-blocking mode. */
  while (true) {
    int wait_result;

    ResetLatch(MyLatch);
    if (ShutdownWorker)
      break;

    while (!ShutdownWorker) {
      int bytes;

      if (ReloadConfig) {
        ReloadConfig = false;
        ProcessConfigFile(PGC_SIGHUP);
        elog(LOG, "configuration file reloaded");
      }

      /* Try to read one batch of rows from the socket. Note that the
         socket is in noblock mode, so this might fail immediately and
         we will then exit to the outer loop. */
      bytes = recv(sfd, &buffer, sizeof(buffer), 0);
      if (bytes < 0) {
        /* Leave the inner loop if there either was no data to receive
         * or if the call was interrupted by a signal. */
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
          break;
        ereport(ERROR, (errcode_for_socket_access(),
                        errmsg("could not read lines: %m")));
      }

      ProcessPacket(buffer, bytes, args->namespace_id);
    }

    /* Here we block and wait until there is anything to read from the socket,
     * or the postmaster shuts down. */
    wait_result = WaitLatchOrSocket(
        MyLatch, WL_LATCH_SET | WL_POSTMASTER_DEATH | WL_SOCKET_READABLE, sfd,
        -1L, PG_WAIT_EXTENSION);
    if (wait_result & WL_POSTMASTER_DEATH)
      break; /* Abort the worker */
  }

  proc_exit(0);
}

/**
 * Dynamically launch a worker.
 *
 * This is used to start a worker in, for example, tests. The worker
 * is automatically associated with the current database.
 *
 * @param schema Schema name where tables for metrics are stored.
 */
Datum worker_launch(PG_FUNCTION_ARGS) {
  Oid nspid = PG_GETARG_OID(0);
  char *service = text_to_cstring(PG_GETARG_TEXT_P(1));
  BackgroundWorker worker;
  BackgroundWorkerHandle *handle;
  BgwHandleStatus status;
  pid_t pid;
  WorkerArgs args;

  /* Check that we have a valid namespace id */
  if (get_namespace_name(nspid) == NULL)
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_SCHEMA),
                    errmsg("schema with OID %d does not exist", nspid)));

  /* Set up arguments to worker */
  memset(&args, 0, sizeof(args));
  strncpy(args.service, service, sizeof(args.service));
  args.namespace_id = nspid;

  WorkerInit(&worker, &args);

  /* set bgw_notify_pid so that we can use WaitForBackgroundWorkerStartup */
  worker.bgw_notify_pid = MyProcPid;

  if (!RegisterDynamicBackgroundWorker(&worker, &handle))
    PG_RETURN_NULL();

  status = WaitForBackgroundWorkerStartup(handle, &pid);

  if (status == BGWH_STOPPED)
    ereport(ERROR,
            (errcode(ERRCODE_INSUFFICIENT_RESOURCES),
             errmsg("could not start background process"),
             errhint("More details may be available in the server log.")));

  if (status == BGWH_POSTMASTER_DIED)
    ereport(ERROR,
            (errcode(ERRCODE_INSUFFICIENT_RESOURCES),
             errmsg("cannot start background processes without postmaster"),
             errhint("Kill all remaining database processes and restart the "
                     "database.")));

  ereport(NOTICE,
          (errmsg("background worker started"), errdetail("pid=%d", pid)));

  Assert(status == BGWH_STARTED);
  PG_RETURN_INT32(pid);
}
