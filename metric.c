#include "metric.h"

#include <postgres.h>
#include <fmgr.h>

#include <access/table.h>
#include <catalog/pg_type.h>
#include <commands/tablecmds.h>
#include <executor/spi.h>
#include <funcapi.h>
#include <miscadmin.h>
#include <nodes/makefuncs.h>
#include <parser/parse_func.h>
#include <utils/builtins.h>
#if PG_VERSION_NUM < 150000
#include <utils/int8.h>
#endif
#include <utils/inval.h>
#include <utils/jsonb.h>
#include <utils/lsyscache.h>
#include <utils/rel.h>
#include <utils/syscache.h>
#include <utils/timestamp.h>

#include "cache.h"

PG_FUNCTION_INFO_V1(default_create);

static void BuildFromCString(AttInMetadata *attinmeta, char *value, int attnum,
                             Datum *values, bool *nulls) {
  values[attnum - 1] = InputFunctionCall(
      &attinmeta->attinfuncs[attnum - 1], value,
      attinmeta->attioparams[attnum - 1], attinmeta->atttypmods[attnum - 1]);
  nulls[attnum - 1] = (value == NULL);
}

/**
 * Check if the Oid is a "timestamp type".
 *
 * Timestamp types here are any types that accept an integer. We allow
 * normal integers as well since there is no harm in it, not because
 * it is required for some reason.
 *
 * @param argtype OID of type to check
 * @returns True if this is a timestamp type.
 */
static bool is_timestamp_type(Oid argtype) {
  return (argtype == TIMESTAMPOID || argtype == TIMESTAMPTZOID ||
          argtype == INT8OID);
}

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
    KVItem *item = (KVItem *)lfirst(cell);
    AddJsonField(&state, item->key, item->value);
  }
  value = pushJsonbValue(&state, WJB_END_OBJECT, NULL);

  return JsonbValueToJsonb(value);
}

/**
 * Insert items into values array.
 */
static void InsertItems(List **pitems, AttInMetadata *attinmeta, Datum *values,
                        bool *nulls) {
  ListCell *cell;
  TupleDesc tupdesc = attinmeta->tupdesc;
  List *items = *pitems;

  foreach (cell, items) {
    const KVItem *item = (KVItem *)lfirst(cell);
    const int attnum = SPI_fnumber(tupdesc, item->key);
    if (attnum > 0) {
      BuildFromCString(attinmeta, item->value, attnum, values, nulls);
      items = foreach_delete_current(items, cell);
    }
  }
  *pitems = items;
}

/**
 * Prepare a record for the relation.
 *
 * Memory for the record need to be already allocated, but it will be
 * filled in.
 *
 * @param[in] Relation to prepare record for.
 * @param[in] Array of parameter types.
 * @param[out] Pointer to variable for prepared insert statement.
 */
static void PrepareRecord(Relation rel, Oid *argtypes, PreparedInsert record) {
  TupleDesc tupdesc = RelationGetDescr(rel);
  StringInfoData stmt;
  SPIPlanPtr plan;
  const Oid relid = RelationGetRelid(rel);
  int i;

  /* Using the tuple descriptor and the parsed package, build the
   * insert statement and collect the null array for the prepare
   * call. */
  initStringInfo(&stmt);
  appendStringInfo(&stmt, "INSERT INTO %s.%s VALUES (",
                   quote_identifier(SPI_getnspname(rel)),
                   quote_identifier(SPI_getrelname(rel)));
  for (i = 0; i < tupdesc->natts; ++i)
    appendStringInfo(&stmt, "$%d%s", i + 1,
                     (i < tupdesc->natts - 1 ? ", " : ""));
  appendStringInfoString(&stmt, ")");

  plan = SPI_prepare(stmt.data, tupdesc->natts, argtypes);
  if (!plan)
    elog(ERROR, "SPI_prepare for relation %s failed: %s", SPI_getrelname(rel),
         SPI_result_code_string(SPI_result));
  if (SPI_keepplan(plan))
    elog(ERROR, "SPI_keepplan failed for relation %s", SPI_getrelname(rel));

  record->relid = relid;
  record->pplan = plan;
}

/**
 * Compute values arrays and nulls from a metric.
 */
bool CollectValues(Metric *metric, AttInMetadata *attinmeta, Oid *argtypes,
                   Datum *values, bool *nulls) {
  TupleDesc tupdesc = attinmeta->tupdesc;
  int time_attnum, tags_attnum, fields_attnum, i;

  /* Set default values for nulls array and fill in the type id. */
  for (i = 0; i < tupdesc->natts; ++i) {
    nulls[i] = true;
    argtypes[i] = SPI_gettypeid(tupdesc, i + 1);
  }

  time_attnum = SPI_fnumber(tupdesc, "_time");
  if (time_attnum > 0) {
    /* If the type is not a timestamp type, we skip the line. Also if
       we cannot parse the timestamp as an integer. */
    int64 value;

    if (!is_timestamp_type(argtypes[time_attnum - 1]))
      return false;
#if PG_VERSION_NUM < 150000
    if (!scanint8(metric->timestamp, true, &value))
      return false;
#else
    {
      char *endptr;
      errno = 0;
      value = strtoi64(metric->timestamp, &endptr, 10);
      if (errno != 0 || *endptr != '\0')
        return false;
    }
#endif

    /* The timestamp is timestamp in nanosecond since UNIX epoch, so we
       convert it to PostgreSQL timestamp in microseconds since
       PostgreSQL epoch. */
    value /= 1000;
    value -= USECS_PER_SEC *
             ((POSTGRES_EPOCH_JDATE - UNIX_EPOCH_JDATE) * SECS_PER_DAY);
    values[time_attnum - 1] = TimestampGetDatum(value);
    nulls[time_attnum - 1] = false;
  }

  InsertItems(&metric->tags, attinmeta, values, nulls);
  InsertItems(&metric->fields, attinmeta, values, nulls);

  tags_attnum = SPI_fnumber(tupdesc, "_tags");
  if (tags_attnum > 0) {
    if (argtypes[tags_attnum] != JSONBOID)
      return false;
    values[tags_attnum - 1] = JsonbPGetDatum(BuildJsonObject(metric->tags));
    nulls[tags_attnum - 1] = false;
  }

  fields_attnum = SPI_fnumber(tupdesc, "_fields");
  if (fields_attnum > 0) {
    if (argtypes[tags_attnum] != JSONBOID)
      return false;
    values[fields_attnum - 1] = JsonbPGetDatum(BuildJsonObject(metric->fields));
    nulls[fields_attnum - 1] = false;
  }
  return true;
}

static ArrayType *MakeArrayFromCStringList(List *elems) {
  ListCell *cell;
  Datum *datum = palloc(sizeof(Datum) * list_length(elems));

  foreach (cell, elems) {
    const char *elem = (const char *)lfirst(cell);
    Name name = palloc(NAMEDATALEN);
    namestrcpy(name, elem);
    datum[foreach_current_index(cell)] = NameGetDatum(name);
  }

  return construct_array(datum, list_length(elems), NAMEOID, NAMEDATALEN, false,
                         TYPALIGN_CHAR);
}

/**
 * Optionally create the metric table.
 *
 * The function looks for a procedure named `_create` in the same
 * schema as the metrics and which accepts three parameters:
 *
 * 1. The name of the metric.
 * 2. An array of tag names of the measurement just received.
 * 3. An array of field names just received.
 *
 * @returns OID of created table, or InvalidOid if the table was not
 * created.
 */
Oid MetricCreate(Metric *metric, Oid nspid) {
  /*
   * Fetch the function from the system table. We are looking for a
   * function that accepts three parameters:
   * 1. A Name, which is the metric name.
   * 2. Array with tag names
   * 3. Array with field names
   */
  Name metric_name;
  Oid createoid, result;
  ArrayType *tags_array, *fields_array;
  Oid args[] = {NAMEOID, NAMEARRAYOID, NAMEARRAYOID};
  char *namespace = get_namespace_name(nspid);
  List *create_func = list_make2(makeString(namespace), makeString("_create"));

  createoid =
      LookupFuncName(create_func, sizeof(args) / sizeof(*args), args, true);

  if (!OidIsValid(createoid))
    return InvalidOid;

  elog(DEBUG1, "found metric creation function \"%s\" with OID %d",
       NameListToString(create_func), createoid);

  if (get_func_rettype(createoid) != REGCLASSOID)
    ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION),
                    errmsg("type input function \"%s\" must return type \"%s\"",
                           NameListToString(create_func),
                           format_type_be(REGCLASSOID))));

  metric_name = palloc(NAMEDATALEN);
  namestrcpy(metric_name, metric->name);
  tags_array = MakeArrayFromCStringList(metric->tags);
  fields_array = MakeArrayFromCStringList(metric->fields);
  PG_TRY();
  {
    result = DatumGetObjectId(OidFunctionCall3(
        createoid, NameGetDatum(metric_name), PointerGetDatum(tags_array),
        PointerGetDatum(fields_array)));
  }
  PG_CATCH();
  { result = InvalidOid; }
  PG_END_TRY();
  return result;
}

/*
 * Insert a row in the metric table.
 *
 * If there is no table with the same name as the metric, an attempt
 * will be made to create such a table.
 */
void MetricInsert(Metric *metric, Oid nspid) {
  Relation rel;
  Oid relid;
  Datum *values;
  bool *nulls;
  Oid *argtypes;
  AttInMetadata *attinmeta;
  int err, i, natts;

  /* Try to fetch the table. */
  relid = get_relname_relid(metric->name, nspid);

  /* If the table does not exist, we try to create the table. */
  if (!OidIsValid(relid))
    relid = MetricCreate(metric, nspid);

  /* If that fails, we skip the line. */
  if (!OidIsValid(relid))
    return;

  /* Get tuple descriptor for metric table and parse all the data
   * into values array. To do this, we open the table for access and
   * keep it open until the end of the transaction. Otherwise, the
   * table definition can change before we've had a chance to insert
   * the data. */
  rel = table_open(relid, AccessShareLock);
  attinmeta = TupleDescGetAttInMetadata(RelationGetDescr(rel));
  natts = attinmeta->tupdesc->natts;

  values = palloc0(natts * sizeof(Datum));
  nulls = palloc0(natts * sizeof(bool));
  argtypes = palloc(natts * sizeof(Oid));

  if (CollectValues(metric, attinmeta, argtypes, values, nulls)) {
    PreparedInsert record;
    char *cnulls;

    /* Find the hashed entry, or prepare a statement and fill in the
       entry. Filling in the entry will also cache it. */
    if (!FindOrAllocEntry(rel, &record))
      PrepareRecord(rel, argtypes, record);

    cnulls = palloc(natts * sizeof(char));
    for (i = 0; i < natts; ++i)
      cnulls[i] = (nulls[i]) ? 'n' : ' ';

    err = SPI_execute_plan(record->pplan, values, cnulls, false, 0);
    if (err != SPI_OK_INSERT)
      elog(LOG, "SPI_execute_plan failed executing: %s",
           SPI_result_code_string(err));
  }

  table_close(rel, NoLock);
}

Datum default_create(PG_FUNCTION_ARGS) {
  Name metric = PG_GETARG_NAME(0);
  Oid nspoid = get_func_namespace(fcinfo->flinfo->fn_oid);
  CreateStmt *create = makeNode(CreateStmt);
  ObjectAddress address;

  create->relation =
      makeRangeVar(get_namespace_name(nspoid), NameStr(*metric), -1);
  create->tableElts =
      list_make3(makeColumnDef("_time", TIMESTAMPTZOID, -1, InvalidOid),
                 makeColumnDef("_tags", JSONBOID, -1, InvalidOid),
                 makeColumnDef("_fields", JSONBOID, -1, InvalidOid));
  address = DefineRelation(create, RELKIND_RELATION, GetUserId(), NULL, NULL);
  return address.objectId;
}
