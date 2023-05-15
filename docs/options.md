# Configuration Options Reference for `pg_influx`

To load the extension on startup, add it to `shared_preload_libraries`
in the configuration file. In addition, the following options are
supported.

<dl>
  <dt id="influx.workers"><code>influx.workers</code></dt>
  <dd>Number of workers to spawn when starting up. Defaults to 4.</dd>

  <dt id="influx.service"><code>influx.service</code></dt>
  <dd>Service or port to listen on. If it is a service name, it will
  be looked up in services. Defaults to 8089, which is the default
  InfluxDB port for UDP.</dd>
  
  <dt id="influx.database"><code>influx.database</code></dt>
  <dd>Database name for the worker to connect to.</dd>

  <dt id="influx.role"><code>influx.role</code></dt>
  <dd>Role name for the worker to connect to the database as. Defaults
  to use the superuser.</dd>

  <dt id="influx.schema"><code>influx.schema</code></dt>
  <dd>Schema where the metric tables are located.</dd>
</dl>
