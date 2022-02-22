#include "influx.h"

#include <postgres.h>
#include <fmgr.h>

#include <executor/spi.h>
#include <funcapi.h>
#include <utils/builtins.h>
#include <utils/jsonb.h>

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
static Jsonb *BuildJsonObject(List *items) {
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
 * Parse one line of data from the state and compute values arrays.
 */
void ParseInfluxCollect(ParseState *state, TupleDesc tupdesc, Datum *values,
                        bool *nulls) {
  values[1] = CStringGetTextDatum(state->timestamp);
  values[2] = JsonbPGetDatum(BuildJsonObject(state->tags));
  values[3] = JsonbPGetDatum(BuildJsonObject(state->fields));
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
static HeapTuple ParseInfluxNextTuple(ParseState *state, TupleDesc tupdesc) {
  Datum *values;
  bool *nulls;

  if (!ReadNextLine(state))
    return NULL;

  /* No values are null, so we can just zero the memory for the nulls
     array. */
  nulls = (bool *)palloc0(tupdesc->natts * sizeof(bool));
  values = (Datum *)palloc(tupdesc->natts * sizeof(Datum));

  values[0] = CStringGetTextDatum(state->metric);
  ParseInfluxCollect(state, tupdesc, values, nulls);

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
    funcctx->tuple_desc = tupdesc;
    funcctx->user_fctx =
        ParseInfluxSetup(text_to_cstring(PG_GETARG_TEXT_PP(0)));

    MemoryContextSwitchTo(oldcontext);
  }

  funcctx = SRF_PERCALL_SETUP();
  state = funcctx->user_fctx;

  tuple = ParseInfluxNextTuple(state, funcctx->tuple_desc);
  if (tuple == NULL)
    SRF_RETURN_DONE(funcctx);

  SRF_RETURN_NEXT(funcctx, PointerGetDatum(HeapTupleGetDatum(tuple)));
}
