#include <postgres.h>
#include <fmgr.h>

#include <funcapi.h>
#include <utils/builtins.h>

#include <stdbool.h>
#include <string.h>

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(split_pair);
Datum split_pair(PG_FUNCTION_ARGS) {
  TupleDesc tupdesc;
  char *key, *val;

  if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                    errmsg("function returning record called in context "
                           "that cannot accept type record")));
  key = text_to_cstring(PG_GETARG_TEXT_PP(0));
  val = strchr(key, '=');
  if (val) {
    Datum values[2];
    bool nulls[2] = {0};
    HeapTuple tuple;

    *val++ = '\0';
    values[0] = CStringGetTextDatum(key);
    values[1] = CStringGetTextDatum(val);
    tuple = heap_form_tuple(tupdesc, values, nulls);
    PG_RETURN_DATUM(HeapTupleGetDatum(tuple));
  }
  PG_RETURN_NULL();
}
