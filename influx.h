#ifndef INFLUX_H_
#define INFLUX_H_

#include <postgres.h>

#include <access/tupdesc.h>
#include <utils/builtins.h>
#include <utils/jsonb.h>

#include <stdbool.h>

#include "parser.h"

ParseState *ParseInfluxSetup(char *buffer);
bool ParseInfluxCollect(ParseState *state, TupleDesc tupdesc, Oid *argtypes,
                        Datum *values, bool *nulls);
Jsonb *BuildJsonObject(List *items);
#endif /* INFLUX_H_ */
