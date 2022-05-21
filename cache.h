/* Copyright 2022
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License. */

#ifndef CACHE_H_
#define CACHE_H_

#include <postgres.h>

#include <executor/spi.h>

/**
 * Structure for prepared inserts.
 */
typedef struct PreparedInsertData {
  Oid relid;
  SPIPlanPtr pplan;
} PreparedInsertData;

typedef PreparedInsertData *PreparedInsert;

extern bool FindOrAllocEntry(Relation rel, PreparedInsert *precord);
extern void InitInsertCache(void);
extern void InsertCacheInvalCallback(Datum arg, Oid relid);

#endif /* CACHE_H_ */
