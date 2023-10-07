# Getting Started with the Influx Listener

The easiest way to get started with the Influx Listener is to install
it in a schema and launch a worker process for that schema.

```sql
CREATE EXTENSION influx WITH SCHEMA metrics;
SELECT metrics.launch_udp_worker('8089');
```

This will spawn a single worker that will listen to socket 8089. The
`launch_udp_worker` procedure will assume that the worker is for that
schema.

> **NOTE:** PostgreSQL does not allow installing an extension multiple
> times in the same database, so you cannot use this method to launch
> workers for different schema. If you want to do that, you need to
> use the [2-argument version of `launch_udp_worker`][1]. If you do
> that, however, you will not be able to start workers automatically
> in the background since you can (currently) only give one schema for
> the workers and not have several workers writing to different
> schema.

[1]: procedures.md#function-launch_udp_worker

## Automatically starting listeners

To automatically start listeners in the background, you need to add
the extension to the `shared_preload_libraries`, and set the
parameters [`influx.schema`](options.md#influx.schema) and
[`influx.database`](options.md#influx.database) to useful values.

```ini
shared_preload_libraries = 'influx'
influx.schema = metrics
influx.database = my_database
influx.role = influx       # defaults to superuser
influx.workers = 10        # defaults to 4
```

> **NOTE:** When pre-loading the library, it will not create the
> schema nor add functions (such as [`_create`][2]) to the schema, so
> before editing the configuration file, you should create the
> extension in the database and schema that you want to use as
> explained above.

[2]: procedures.md#function-_create
