#include "guc.h"

#include <postgres.h>

#include <utils/guc.h>

/** Number of Influx protocol workers. */
int InfluxWorkersCount;

/** Service name to listen on. */
char *InfluxServiceName;

/** Schema name to use for metrics. */
char *InfluxSchemaName;

/** Database name to work with. */
char *InfluxDatabaseName;

/** Role name to use when connecting to the database. */
char *InfluxRoleName;

void InfluxGucInitDynamic(void) {
  DefineCustomIntVariable("influx.workers",     /* option name */
                          "Number of workers.", /* short descriptor */
                          "Number of workers to spawn when starting up"
                          "the server.",       /* long description */
                          &InfluxWorkersCount, /* value address */
                          4,                   /* boot value */
                          0,                   /* min value */
                          20,                  /* max value */
                          PGC_SIGHUP,          /* option context */
                          0,                   /* option flags */
                          NULL,                /* check hook */
                          NULL,                /* assign hook */
                          NULL);               /* show hook */
}

void InfluxGucInitStatic(void) {
  /* Right now we have only UDP supported, so we use 8089. If we decide to
   * support more protocols, we should dynamically pick the default based on the
   * protocol. */
  DefineCustomStringVariable(
      "influx.service", "Service name.",
      "Service name to listen on, or port number. If it is a service name, it"
      " will be looked up.If it is a port, it will be used as it is.",
      &InfluxServiceName, "8089", PGC_POSTMASTER, 0, NULL, NULL, NULL);
  DefineCustomStringVariable(
      "influx.database", "Database name.", "Database name to connect to.",
      &InfluxDatabaseName, NULL, PGC_POSTMASTER, 0, NULL, NULL, NULL);
  DefineCustomStringVariable(
      "influx.role", "Role name.",
      "Role name to use when connecting to the database. Default is to"
      " connect as superuser.",
      &InfluxRoleName, NULL, PGC_POSTMASTER, 0, NULL, NULL, NULL);
  DefineCustomStringVariable(
      "influx.schema", "Schema name.",
      "Schema name to use for the workers. This is where the measurement"
      " tables should be placed.",
      &InfluxSchemaName, NULL, PGC_POSTMASTER, 0, NULL, NULL, NULL);

  elog(LOG,
       "InfluxDatabaseName: %s, InfluxSchemaName: %s, InfluxServiceName: %s, "
       "InfluxRoleName: %s, InfluxWorkersCount: %d",
       InfluxDatabaseName, InfluxSchemaName, InfluxServiceName, InfluxRoleName,
       InfluxWorkersCount);
}
