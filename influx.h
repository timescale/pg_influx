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
Jsonb *BuildJsonObject(List *items);

#endif /* INFLUX_H_ */
