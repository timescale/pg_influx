# Getting Started with the Influx Listener

The easiest way to get started with the Influx Listener is to install
it in a schema and launch a worker process.

```sql
CREATE EXTENSION influx WITH SCHEMA metrics;
SELECT worker_launch('metrics', '8089');
```

This will spawn a single worker that will listen to socket 8089.

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
