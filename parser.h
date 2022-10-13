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

#ifndef PARSER_H_
#define PARSER_H_

#include <postgres.h>

#include <nodes/pg_list.h>

#include <setjmp.h>
#include <stdbool.h>
#include <stdlib.h>

#include "metric.h"

/**
 * Parser state.
 *
 * @note All pointers in the parser state will point into the line
 * buffer.
 */
typedef struct ParseState {
  char *start; /**< Start of line buffer */

  /** Current read position of line buffer  */
  char *current;

  /** Metric resulting from the parse. */
  Metric metric;
} ParseState;

void ParseStateInit(ParseState *state, char *line);
bool ReadNextLine(ParseState *state);

#endif /* PARSER_H_ */
