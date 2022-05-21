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

#include <catalog/pg_type.h>
#include <executor/spi.h>
#include <funcapi.h>
#include <utils/builtins.h>
#include <utils/int8.h>
#include <utils/jsonb.h>
#include <utils/timestamp.h>

#include <stdbool.h>
#include <string.h>

#include "parser.h"

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(parse_influx);

/**
 * Add key and value pair to JSONB parser state.
 *
 * @param state JSON parser state
 * @param key Key as a C string.
 * @param value Value as a C string.
 */
static void AddJsonField(JsonbParseState **state, char *key, char *value) {
  JsonbValue jb_key, jb_val;
  jb_key.type = jbvString;
  jb_key.val.string.val = pstrdup(key);
  jb_key.val.string.len = strlen(key);

  (void)pushJsonbValue(state, WJB_KEY, &jb_key);

  jb_val.type = jbvString;
  jb_val.val.string.val = pstrdup(value);
  jb_val.val.string.len = strlen(value);
  (void)pushJsonbValue(state, WJB_VALUE, &jb_val);
}

/**
 * Build JSON object from list of key-value items.
 *
 * @param items List of `ParseItem`
 * @returns JSONB object with the key-value pairs.
 */
Jsonb *BuildJsonObject(List *items) {
  ListCell *cell;
  JsonbParseState *state = NULL;
  JsonbValue *value;

  pushJsonbValue(&state, WJB_BEGIN_OBJECT, NULL);
  foreach (cell, items) {
    ParseItem *item = (ParseItem *)lfirst(cell);
    AddJsonField(&state, item->key, item->value);
  }
  value = pushJsonbValue(&state, WJB_END_OBJECT, NULL);

  return JsonbValueToJsonb(value);
}

/**
 * Check if the Oid is a "timestamp type".
 *
 * Timestamp types here are any types that accept an integer. We allow
 * normal integers as well since there is no harm in it, not because
 * it is required for some reason.
 *
 * @param argtype OID of type to check
 * @returns True if this is a timestamp type.
 */
static bool is_timestamp_type(Oid argtype) {
  return (argtype == TIMESTAMPOID || argtype == TIMESTAMPTZOID ||
          argtype == INT8OID);
}

static void BuildFromCString(AttInMetadata *attinmeta, char *value, int attnum,
                             Datum *values, bool *nulls) {
  values[attnum - 1] = InputFunctionCall(
      &attinmeta->attinfuncs[attnum - 1], value,
      attinmeta->attioparams[attnum - 1], attinmeta->atttypmods[attnum - 1]);
  nulls[attnum - 1] = (value == NULL);
}

/**
 * Insert items into values array.
 *
 *
 */
static void InsertItems(List **pitems, AttInMetadata *attinmeta, Datum *values,
                        bool *nulls) {
  ListCell *cell;
  TupleDesc tupdesc = attinmeta->tupdesc;
  List *items = *pitems;

  foreach (cell, items) {
    const ParseItem *item = (ParseItem *)lfirst(cell);
    const int attnum = SPI_fnumber(tupdesc, item->key);
    if (attnum > 0) {
      BuildFromCString(attinmeta, item->value, attnum, values, nulls);
      items = foreach_delete_current(items, cell);
    }
  }
  *pitems = items;
}

/**
 * Parse one line of data from the state and compute values arrays.
 */
bool ParseInfluxCollect(ParseState *state, AttInMetadata *attinmeta,
                        Oid *argtypes, Datum *values, bool *nulls) {
  TupleDesc tupdesc = attinmeta->tupdesc;
  int time_attnum, tags_attnum, fields_attnum, i;

  /* Set default values for nulls array and fill in the type id. */
  for (i = 0; i < tupdesc->natts; ++i) {
    nulls[i] = true;
    argtypes[i] = SPI_gettypeid(tupdesc, i + 1);
  }

  time_attnum = SPI_fnumber(tupdesc, "_time");
  if (time_attnum > 0) {
    /* If the type is not a timestamp type, we skip the line. Also if
       we cannot parse the timestamp as an integer. */
    int64 value;
    if (!is_timestamp_type(argtypes[time_attnum - 1]) ||
        !scanint8(state->timestamp, true, &value))
      return false;
    /* The timestamp is timestamp in nanosecond since UNIX epoch, so we convert
       it to PostgreSQL timestamp in microseconds since PostgreSQL epoch. */
    value /= 1000;
    value -= USECS_PER_SEC *
             ((POSTGRES_EPOCH_JDATE - UNIX_EPOCH_JDATE) * SECS_PER_DAY);
    values[time_attnum - 1] = TimestampGetDatum(value);
    nulls[time_attnum - 1] = false;
  }

  InsertItems(&state->tags, attinmeta, values, nulls);
  InsertItems(&state->fields, attinmeta, values, nulls);

  tags_attnum = SPI_fnumber(tupdesc, "_tags");
  if (tags_attnum > 0) {
    if (argtypes[tags_attnum] != JSONBOID)
      return false;
    values[tags_attnum - 1] = JsonbPGetDatum(BuildJsonObject(state->tags));
    nulls[tags_attnum - 1] = false;
  }

  fields_attnum = SPI_fnumber(tupdesc, "_fields");
  if (fields_attnum > 0) {
    if (argtypes[tags_attnum] != JSONBOID)
      return false;
    values[fields_attnum - 1] = JsonbPGetDatum(BuildJsonObject(state->fields));
    nulls[fields_attnum - 1] = false;
  }
  return true;
}

/**
 * Parser state setup.
 */
ParseState *ParseInfluxSetup(char *buffer) {
  ParseState *state = palloc(sizeof(ParseState));
  ParseStateInit(state, buffer);
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
static HeapTuple ParseInfluxNextTuple(ParseState *state,
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
    if (!ReadNextLine(state))
      return NULL;
  } while (!ParseInfluxCollect(state, attinmeta, argtypes, values, nulls));

  /* This assumes that the metric is a text column. We should probably add a
   * check here, or call the input function for the column type. */
  metric_attnum = SPI_fnumber(tupdesc, "_metric");
  if (metric_attnum > 0) {
    values[metric_attnum - 1] = CStringGetTextDatum(state->metric);
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
  ParseState *state;

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

  SRF_RETURN_NEXT(funcctx, PointerGetDatum(HeapTupleGetDatum(tuple)));
}
