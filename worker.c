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
#include <utils/int8.h>
#include <utils/inval.h>
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

void WorkerMain(Datum dbid) pg_attribute_noreturn();
PG_FUNCTION_INFO_V1(worker_launch);

typedef struct WorkerArgs {
  Oid namespace_id;
  char service[NI_MAXSERV];
} WorkerArgs;

/* Check that sizeof(WorkerArgs) > BGW_EXTRALEN */
static char c1[BGW_EXTRALEN - sizeof(WorkerArgs)] pg_attribute_unused();

static volatile sig_atomic_t ReloadConfig = false;
static volatile sig_atomic_t ShutdownWorker = false;

/**
 * Prepare a record for the relation.
 *
 * Memory for the record need to be already allocated, but it will be
 * filled in.
 *
 * @param[in] Relation to prepare record for.
 * @param[in] Array of parameter types.
 * @param[out] Pointer to variable for prepared insert statement.
 */
static void PrepareRecord(Relation rel, Oid *argtypes, PreparedInsert record) {
  TupleDesc tupdesc = RelationGetDescr(rel);
  StringInfoData stmt;
  SPIPlanPtr plan;
  const Oid relid = RelationGetRelid(rel);
  int i;

  elog(NOTICE, "preparing statement for %s", SPI_getrelname(rel));

  /* Using the tuple descriptor and the parsed package, build the
   * insert statement and collect the null array for the prepare
   * call. */
  initStringInfo(&stmt);
  appendStringInfo(&stmt, "INSERT INTO %s.%s VALUES (",
                   quote_identifier(SPI_getnspname(rel)),
                   quote_identifier(SPI_getrelname(rel)));
  for (i = 0; i < tupdesc->natts; ++i)
    appendStringInfo(&stmt, "$%d%s", i + 1,
                     (i < tupdesc->natts - 1 ? ", " : ""));
  appendStringInfoString(&stmt, ")");

  plan = SPI_prepare(stmt.data, tupdesc->natts, argtypes);
  if (!plan)
    elog(ERROR, "SPI_prepare for relation %s failed: %s", SPI_getrelname(rel),
         SPI_result_code_string(SPI_result));
  if (SPI_keepplan(plan))
    elog(ERROR, "SPI_keepplan failed for relation %s", SPI_getrelname(rel));

  record->relid = relid;
  record->pplan = plan;
}

/**
 * Process one packet of lines.
 */
static void ProcessPacket(char *buffer, size_t bytes, Oid nspid) {
  ParseState *state;

  buffer[bytes] = '\0';
  state = ParseInfluxSetup(buffer);

  while (true) {
    Relation rel;
    Oid relid;
    Datum *values;
    bool *nulls;
    Oid *argtypes;
    AttInMetadata *attinmeta;
    int err, i, natts;

    if (!ReadNextLine(state))
      return;

    /* Get tuple descriptor for metric table and parse all the data
     * into values array. To do this, we open the table for access and
     * keep it open until the end of the transaction. Otherwise, the
     * table definition can change before we've had a chance to insert
     * the data. */
    relid = get_relname_relid(state->metric, nspid);

    /* If the table does not exist, we just skip the line silently. */
    relid = get_relname_relid(state->metric, nspid);
    if (relid == InvalidOid)
      continue;

    rel = table_open(relid, AccessShareLock);
    attinmeta = TupleDescGetAttInMetadata(RelationGetDescr(rel));
    natts = attinmeta->tupdesc->natts;

    values = (Datum *)palloc0(natts * sizeof(Datum));
    nulls = (bool *)palloc0(natts * sizeof(bool));
    argtypes = (Oid *)palloc(natts * sizeof(Oid));

    if (ParseInfluxCollect(state, attinmeta, argtypes, values, nulls)) {
      PreparedInsert record;
      char *cnulls;

      /* Find the hashed entry, or prepare a statement and fill in the
         entry. Filling in the entry will also cache it. */
      if (!FindOrAllocEntry(rel, &record))
        PrepareRecord(rel, argtypes, record);

      cnulls = (char *)palloc(natts * sizeof(char));
      for (i = 0; i < natts; ++i)
        cnulls[i] = (nulls[i]) ? 'n' : ' ';

      err = SPI_execute_plan(record->pplan, values, cnulls, false, 0);
      if (err != SPI_OK_INSERT)
        elog(LOG, "SPI_execute_plan failed executing: %s",
             SPI_result_code_string(err));
    }

    table_close(rel, NoLock);
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
static void WorkerInit(BackgroundWorker *worker, WorkerArgs *args) {
  memset(worker, 0, sizeof(*worker));
  /* Shared memory access is necessary to connect to the database. */
  worker->bgw_flags =
      BGWORKER_SHMEM_ACCESS | BGWORKER_BACKEND_DATABASE_CONNECTION;
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

#define GET_FIELD(PTR, FLD) ((PTR) ? (PTR)->FLD : "<NULL>")

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

  BackgroundWorkerInitializeConnectionByOid(database_id, InvalidOid, 0);

  pgstat_report_activity(STATE_RUNNING, "initializing worker");

  CacheRegisterRelcacheCallback(InsertCacheInvalCallback, 0);

  sfd = CreateSocket(NULL, args->service, &UdpRecvSocket, NULL, 0);
  if (sfd == -1) {
    ereport(LOG, (errcode_for_socket_access(),
                  errmsg("could not create socket for service '%s': %m",
                         args->service)));
    proc_exit(1);
  }

  ereport(LOG, (errmsg("worker initialized"),
                errdetail("service=%s, database_id=%u, namespace_id=%u",
                          args->service, database_id, args->namespace_id)));

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
    SPI_start_transaction();
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

      ProcessPacket(buffer, bytes, args->namespace_id);
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

  ereport(NOTICE,
          (errmsg("background worker started"), errdetail("pid=%d", pid)));

  Assert(status == BGWH_STARTED);
  PG_RETURN_INT32(pid);
}
