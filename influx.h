#ifndef INFLUX_H_
#define INFLUX_H_

#include <postgres.h>
#include <fmgr.h>

#include <access/tupdesc.h>
#include <funcapi.h>
#include <utils/builtins.h>
#include <utils/jsonb.h>

#include <stdbool.h>

#include "parser.h"

ParseState *ParseInfluxSetup(char *buffer);
bool ParseInfluxCollect(ParseState *state, AttInMetadata *attinmeta,
                        Oid *argtypes, Datum *values, bool *nulls);
Jsonb *BuildJsonObject(List *items);

#endif /* INFLUX_H_ */
