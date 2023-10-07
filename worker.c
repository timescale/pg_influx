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

#include <access/xact.h>
#include <catalog/namespace.h>
#include <pgstat.h>
#include <storage/latch.h>
#include <utils/elog.h>
#if PG_VERSION_NUM < 150000
#include <utils/int8.h>
#endif

#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include "cache.h"
#include "influx.h"

volatile sig_atomic_t ReloadConfig = false;
volatile sig_atomic_t ShutdownWorker = false;

/**
 * Namespace OID for schema to write metrics to. This is shared by all
 * kinds of protocols.
 */
Oid NamespaceOid;

/* Check that sizeof(WorkerArgs) > BGW_EXTRALEN */
static char c1[BGW_EXTRALEN - sizeof(WorkerArgs)] pg_attribute_unused();

/**
 * Process one packet of lines.
 *
 * The packet consists of zero or more lines of Influx line protocol
 * lines. These will be read and inserted into the database under the
 * apropriate metric.
 */
void InfluxProcessPacket(char *buffer, size_t bytes) {
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
    MetricInsert(&state->metric);
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

void InfluxWorkerSetup(WorkerArgs *args) {
  /* Establish signal handlers; once that's done, unblock signals. */
  pqsignal(SIGTERM, WorkerSigterm);
  pqsignal(SIGHUP, WorkerSighup);
  BackgroundWorkerUnblockSignals();
  BackgroundWorkerInitializeConnection(args->database, args->role, 0);

  pgstat_report_activity(STATE_RUNNING, "initializing worker");

  CacheInit();

  /* We need to start a transaction first because none is started and
     SPI_connect_ext might use TopTransactionContext, which is set by
     this function. The SPI_commit below will automatically start a
     new one. */
  StartTransactionCommand();

  /* It is necessary to start a transaction before calling functions
     like `get_namespace_oid`. Calling them before this will cause a
     crash. */
  NamespaceOid = get_namespace_oid(args->namespace, false);
}
