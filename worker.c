/*
 * Copyright 2022 Timescale Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "worker.h"

#include <postgres.h>
#include <fmgr.h>

#include <access/table.h>
#include <access/xact.h>
#include <catalog/namespace.h>
#include <commands/dbcommands.h>
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
#if PG_VERSION_NUM < 150000
#include <utils/int8.h>
#endif
#include <utils/jsonb.h>
#include <utils/lsyscache.h>
#include <utils/rel.h>
#include <utils/snapmgr.h>

#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include "cache.h"
#include "influx.h"
#include "network.h"

PG_FUNCTION_INFO_V1(worker_launch);

/* MTU for the wireless interface. We don't bother about digging up
 * the actual MTU here and just pick something that is common. */
#define MTU 1500

static volatile sig_atomic_t ReloadConfig = false;
static volatile sig_atomic_t ShutdownWorker = false;

/* Check that sizeof(WorkerArgs) > BGW_EXTRALEN */
static char c1[BGW_EXTRALEN - sizeof(WorkerArgs)] pg_attribute_unused();

/**
 * Process one packet of lines.
 */
static void ProcessPacket(char *buffer, size_t bytes, Oid nspid) {
  IngestState *state;

  buffer[bytes] = '\0';
  state = ParseInfluxSetup(buffer);

  while (true) {
    bool result;
    PG_TRY();
    { result = IngestReadNextLine(state); }
    PG_CATCH();
    { result = false; }
    PG_END_TRY();
    if (!result)
      return;
    MetricInsert(&state->metric, nspid);
  }
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
void InfluxWorkerInit(BackgroundWorker *worker, WorkerArgs *args) {
  memset(worker, 0, sizeof(*worker));
  /* Shared memory access is necessary to connect to the database. */
  worker->bgw_flags =
      BGWORKER_SHMEM_ACCESS | BGWORKER_BACKEND_DATABASE_CONNECTION;
  worker->bgw_start_time = BgWorkerStart_RecoveryFinished;
  worker->bgw_restart_time = BGW_NEVER_RESTART;
  sprintf(worker->bgw_library_name, INFLUX_LIBRARY_NAME);
  sprintf(worker->bgw_function_name, INFLUX_FUNCTION_NAME);
  snprintf(worker->bgw_name, BGW_MAXLEN, "Influx listener for schema %s",
           args->namespace);
  snprintf(worker->bgw_type, BGW_MAXLEN, "Influx line protocol listener");
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
 * - The database identifier is passed as a parameter to this
 *   function.
 */
void InfluxWorkerMain(Datum arg) {
  int sfd;
  char buffer[MTU];
  WorkerArgs *args = (WorkerArgs *)&MyBgworkerEntry->bgw_extra;
  Oid namespace_id;
  struct sockaddr_storage sockaddr;

  /* Establish signal handlers; once that's done, unblock signals. */
  pqsignal(SIGTERM, WorkerSigterm);
  pqsignal(SIGHUP, WorkerSighup);
  BackgroundWorkerUnblockSignals();
  BackgroundWorkerInitializeConnection(args->database, args->role, 0);

  pgstat_report_activity(STATE_RUNNING, "initializing worker");

  CacheInit();

  sfd = CreateSocket(NULL, args->service, &UdpRecvSocket,
                     (struct sockaddr *)&sockaddr, sizeof(sockaddr));
  if (sfd == -1) {
    ereport(LOG, (errcode_for_socket_access(),
                  errmsg("could not create socket for service '%s': %m",
                         args->service)));
    proc_exit(1);
  }

  /* We need to start a transaction first because none is started and
     SPI_connect_ext might use TopTransactionContext, which is set by
     this function. The SPI_commit below will automatically start a
     new one. */
  StartTransactionCommand();

  /* It is necessary to start a transaction before calling functions
     like `get_namespace_oid`. Calling them before this will cause a
     crash. */
  namespace_id = get_namespace_oid(args->namespace, false);

  ereport(
      LOG,
      (errmsg("worker listening on port %d",
              SocketPort((struct sockaddr *)&sockaddr, sizeof(sockaddr))),
       errdetail(
           "Connected to database %s as user %s. Metrics written to schema %s.",
           args->database, args->role, args->namespace)));

  pgstat_report_activity(STATE_RUNNING, "reading events");

  /* This is the same approach as used in pgstats.c: We read the
     packets in two loops. One outer that will block when there is no
     data received, and one inner loop that will read packets as long
     as possible in non-blocking mode. */
  while (true) {
    int wait_result;
    int err;

    ResetLatch(MyLatch);
    if (ShutdownWorker)
      break;

    /* We start a transaction to insert all packets that we're
       reading. This is not optimal if we are constantly inserting
       data since we will build a very large snapshot, but it is good
       enough for a first implementation.

       We need to open a non-atomic (that is, transactional) context
       since we are opening a table for the metric and we need to make
       sure that it does not change while we are updating it. */
    if ((err = SPI_connect_ext(SPI_OPT_NONATOMIC)) != SPI_OK_CONNECT)
      elog(ERROR, "SPI_connect failed: %s", SPI_result_code_string(err));
    PushActiveSnapshot(GetTransactionSnapshot());

    pgstat_report_activity(STATE_RUNNING, "processing incoming packets");

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

      ProcessPacket(buffer, bytes, namespace_id);
    }

    PopActiveSnapshot();
    SPI_commit();
    if ((err = SPI_finish()) != SPI_OK_FINISH)
      elog(ERROR, "SPI_finish failed: %s", SPI_result_code_string(err));
    pgstat_report_stat(false);
    pgstat_report_activity(STATE_IDLE, NULL);

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
  Oid nspid = PG_NARGS() == 1 ? get_func_namespace(fcinfo->flinfo->fn_oid)
                              : PG_GETARG_OID(0);
  char *service = text_to_cstring(PG_GETARG_TEXT_P(PG_NARGS() == 1 ? 0 : 1));
  BackgroundWorker worker;
  BackgroundWorkerHandle *handle;
  BgwHandleStatus status;
  pid_t pid;
  WorkerArgs args = {0};

  /* Check that we have a valid namespace id */
  if (get_namespace_name(nspid) == NULL)
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_SCHEMA),
                    errmsg("schema with OID %d does not exist", nspid)));

  strncpy(args.role, GetUserNameFromId(GetUserId(), true), sizeof(args.role));
  strncpy(args.service, service, sizeof(args.service));
  strncpy(args.namespace, get_namespace_name(nspid), sizeof(args.namespace));
  strncpy(args.database, get_database_name(MyDatabaseId),
          sizeof(args.database));

  InfluxWorkerInit(&worker, &args);

  /* set bgw_notify_pid so that we can use WaitForBackgroundWorkerStartup */
  worker.bgw_notify_pid = MyProcPid;

  if (!RegisterDynamicBackgroundWorker(&worker, &handle))
    PG_RETURN_NULL();

  /* TODO: This only waits for the worker process to actually
     start. It would simplify testing significantly if we wait for it
     to be ready to accept input on the port since we can then start
     sending rows without fear of them being lost, but that requires
     more work that it's worth right now. */
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

  ereport(LOG, (errmsg("background worker started"), errdetail("pid=%d", pid)));

  Assert(status == BGWH_STARTED);
  PG_RETURN_INT32(pid);
}
