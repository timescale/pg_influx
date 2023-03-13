/**
 * Module to insert metrics into the database.
 *
 * The module will translate metrics to insert statements and execute
 * them to insert the data. The module will use the prepared statement
 * cache to avoid re-preparing statements multiple times.
 */

#ifndef METRIC_H_
#define METRIC_H_

#include <postgres.h>

#include <funcapi.h>
#include <nodes/pg_list.h>

typedef enum Type { TYPE_NONE, TYPE_STRING, TYPE_INTEGER, TYPE_FLOAT } Type;

typedef struct KVItem {
  char *key;
  char *value;
  Type type;
} KVItem;

/**
 * A metric.
 *
 * A metric consists of:
 * - A name
 * - A timestamp
 * - A set of zero or more tags
 * - A set of one or more values
 */
typedef struct Metric {
  /** Measurement identifier, pointer into the line buffer */
  const char *name;

  /** Timestamp as a string */
  const char *timestamp;

  /** List of items that represent tags */
  List *tags;

  /** List of items that represent fields */
  List *fields;
} Metric;

Oid MetricCreate(Metric *metric, Oid nspid);
void MetricInsert(Metric *metric, Oid nspid);
bool CollectValues(Metric *metric, AttInMetadata *attinmeta, Oid *argtypes,
                   Datum *values, bool *nulls);

#endif /* METRIC_H_ */
