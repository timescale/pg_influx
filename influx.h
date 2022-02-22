#ifndef INFLUX_H_
#define INFLUX_H_

#include <postgres.h>

#include <access/tupdesc.h>
#include <utils/builtins.h>

#include <stdbool.h>

#include "parser.h"

ParseState *ParseInfluxSetup(char *buffer);
void ParseInfluxCollect(ParseState *state, TupleDesc tupdesc, Datum *values,
                        bool *nulls);

#endif /* INFLUX_H_ */
