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

typedef enum ValueType {
  TYPE_NONE,
  TYPE_STRING,
  TYPE_INTEGER,
  TYPE_FLOAT
} ValueType;

typedef struct ParseItem {
  char *key;
  char *value;
  ValueType type;
} ParseItem;

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

  /** Measurement identifier, pointer into the line buffer */
  const char *metric;

  /** Timestamp as a string */
  const char *timestamp;

  /** List of items that represent tags */
  List *tags;

  /** List of items that represent fields */
  List *fields;
} ParseState;

void ParseStateInit(ParseState *state, char *line);
bool ReadNextLine(ParseState *state);

#endif /* PARSER_H_ */
