#include "cache.h"

#include <postgres.h>

#include <utils/inval.h>
#include <utils/lsyscache.h>
#include <utils/rel.h>

/**
 * Hash table with prepared inserts for relations.
 *
 * There is one entry for each relation that has been inserted into.
 */
static HTAB *InsertCache = NULL;

/**
 * Initialize the cache.
 */
void InitInsertCache(void) {
  HASHCTL hash_ctl;

  memset(&hash_ctl, 0, sizeof(hash_ctl));

  hash_ctl.keysize = sizeof(Oid);
  hash_ctl.entrysize = sizeof(PreparedInsertData);

  /* The initial size was arbitrarily picked. Most other usage of
   * hash_create in the PostgreSQL code uses 128, so we do the same
   * here. */
  InsertCache =
      hash_create("Prepared Inserts", 128, &hash_ctl, HASH_ELEM | HASH_BLOBS);
}

/**
 * Find an existing entry for the given relation, or insert a new one.
 *
 * @param rel[in] Relation for the entry.
 * @param precord[out] Pointer to variable for record.
 * @retval true Existing entry was found.
 * @retval false Inserted a new entry.
 */
bool FindOrAllocEntry(Relation rel, PreparedInsert *precord) {
  const Oid relid = RelationGetRelid(rel);
  bool found;

  if (!InsertCache)
    InitInsertCache();

  *precord = hash_search(InsertCache, &relid, HASH_ENTER, &found);
  return found;
}

/**
 * Invalidate an insert cache entry.
 *
 * The cache entry is invalidated by just removing it. It will be
 * re-created when it is again requested since it will not be found in
 * the cache.
 *
 * @param arg[in] Not used
 * @param relid[in] Relation id for relation that was invalidated
 */
void InsertCacheInvalCallback(Datum arg, Oid relid) {
  if (InsertCache) {
    bool found;
    PreparedInsert record =
        hash_search(InsertCache, &relid, HASH_REMOVE, &found);
    if (found)
      SPI_freeplan(record->pplan);
  }
}

void CacheInit(void) {
  CacheRegisterRelcacheCallback(InsertCacheInvalCallback, 0);
}
