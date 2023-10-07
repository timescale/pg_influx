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

#include "influx.h"

#include <postgres.h>
#include <fmgr.h>

#include <catalog/namespace.h>
#include <catalog/pg_type.h>
#include <executor/spi.h>
#include <funcapi.h>
#include <miscadmin.h>
#include <postmaster/bgworker.h>
#include <utils/acl.h>
#include <utils/builtins.h>
#include <utils/guc.h>
#include <utils/jsonb.h>
#include <utils/timestamp.h>

#include <stdbool.h>
#include <string.h>

#include "guc.h"
#include "ingest.h"
#include "udp.h"
#include "worker.h"

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(parse_influx);

void PGDLLEXPORT _PG_init(void);

/** Parser state setup. */
IngestState *ParseInfluxSetup(char *buffer) {
  IngestState *state = palloc(sizeof(IngestState));
  IngestStateInit(state, buffer);
  return state;
}

/**
 * Parse one line of data from the parser state and store it as a heap tuple
 * with the columns:
 *
 * 1. The metric
 * 2. The timestamp
 * 3. The tags as a JSONB value
 * 4. The fields as a JSONB value
 *
 * @param state Parser state, with line information.
 * @param tupdesc Tuple descriptor for the data.
 * @returns tuple Heap tuple
 */
static HeapTuple ParseInfluxNextTuple(IngestState *state,
                                      AttInMetadata *attinmeta) {
  int metric_attnum;
  Datum *values;
  bool *nulls;
  Oid *argtypes;
  TupleDesc tupdesc = attinmeta->tupdesc;

  nulls = (bool *)palloc0(tupdesc->natts * sizeof(bool));
  values = (Datum *)palloc0(tupdesc->natts * sizeof(Datum));
  argtypes = (Oid *)palloc0(tupdesc->natts * sizeof(Oid));

  /* Read lines until we find one that can be used. If none are found, we're
   * done. */
  do {
    if (!IngestReadNextLine(state))
      return NULL;
  } while (!CollectValues(&state->metric, attinmeta, argtypes, values, nulls));

  /* This assumes that the metric is a text column. We should probably add a
   * check here, or call the input function for the column type. */
  metric_attnum = SPI_fnumber(tupdesc, "_metric");
  if (metric_attnum > 0) {
    values[metric_attnum - 1] = CStringGetTextDatum(state->metric.name);
    nulls[metric_attnum - 1] = false;
  }

  return heap_form_tuple(tupdesc, values, nulls);
}

/**
 * Parse an influx packet.
 */
Datum parse_influx(PG_FUNCTION_ARGS) {
  HeapTuple tuple;
  FuncCallContext *funcctx;
  IngestState *state;

  if (SRF_IS_FIRSTCALL()) {
    TupleDesc tupdesc;
    MemoryContext oldcontext;

    funcctx = SRF_FIRSTCALL_INIT();
    oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

    if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                      errmsg("function returning record called in context "
                             "that cannot accept type record")));
    funcctx->attinmeta = TupleDescGetAttInMetadata(tupdesc);
    funcctx->user_fctx =
        ParseInfluxSetup(text_to_cstring(PG_GETARG_TEXT_PP(0)));

    MemoryContextSwitchTo(oldcontext);
  }

  funcctx = SRF_PERCALL_SETUP();
  state = funcctx->user_fctx;

  tuple = ParseInfluxNextTuple(state, funcctx->attinmeta);
  if (tuple == NULL)
    SRF_RETURN_DONE(funcctx);

  SRF_RETURN_NEXT(funcctx, HeapTupleGetDatum(tuple));
}

static void StartBackgroundWorkers(const char *database_name,
                                   const char *schema_name,
                                   const char *role_name,
                                   const char *service_name) {
  MemoryContext oldcontext = MemoryContextSwitchTo(TopMemoryContext);
  WorkerArgs args = {0};

  if (schema_name)
    strncpy(args.namespace, schema_name, sizeof(args.namespace));
  if (database_name)
    strncpy(args.database, database_name, sizeof(args.database));
  if (role_name)
    strncpy(args.role, role_name, sizeof(args.role));
  if (service_name)
    strncpy(args.service, service_name, sizeof(args.service));

  StartUdpBackgroundWorkers(&args);
  MemoryContextSwitchTo(oldcontext);
}

void _PG_init(void) {
  InfluxGucInitDynamic();

  if (!process_shared_preload_libraries_in_progress)
    return;

  InfluxGucInitStatic();
  StartBackgroundWorkers(InfluxDatabaseName, InfluxSchemaName, InfluxRoleName,
                         InfluxServiceName);
}
