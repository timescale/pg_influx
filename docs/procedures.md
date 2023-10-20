# Reference Manual for `pg_influx`

## Table of Contents

1. [Procedure `send_udp_packet`](#procedure-send_udp_packet)
2. [Function `launch_udp_worker`](#function-launch_udp_worker)
3. [Function `_create`](#function-_create)

## Function `launch_udp_worker`

Launch a new UDP background worker.

The service is looked up using `getaddrinfo` so it is possible to
either provide a port number of service name that will be translated
to a corresponding port number. Both IPv4 and IPv6 addresses are
supported through `getaddrinfo`.

The function will bind to the first address suitable for receiving UDP
traffic (using `AI_PASSIVE` and a NULL node name), which means that it
is not possible to select the IP address to listen on.

### Parameters

|      Name | Type           | Description                                                                     |
|----------:|:---------------|:--------------------------------------------------------------------------------|
| namespace | `regnamespace` | Namespace for the metrics. Optional. Defaults to the namespace of the function. |
|   service | `text`         | Service for the worker to listen on                                             |

### Returns

An integer being the PID of the started worker. This can be used with
[`pg_terminate_backend`][1] to terminate the backend.

[1]: https://www.postgresql.org/docs/current/functions-admin.html#FUNCTIONS-ADMIN-SIGNAL-TABLE

### Examples

If you have installed the extension in the schema `metrics` you can
spawn a worker listening on socket 8089 using:

```sql
SELECT metrics.launch_udp_worker('8089');
```

If you have installed the extension in the `public` schema (which is
the default if you do not give a schema when using [`CREATE
EXTENSION`]()) you can start a worker using:

```sql
SELECT launch_udp_worker('metrics', '8089');
```

If you want to terminate a previously started worker, you can save the
PID in a `psql` variable:

```sql
SELECT metrics.launch_udp_worker('8089') as pid \gset
   .
   .
   .
SELECT pg_terminate_backend(:pid);
```

## Procedure `send_udp_packet`

Send a UDP packet to a network address.

The text string provided will be sent as an UDP packet to the host and
service given by `service` and `hostname` parameters. Service and
hostname lookup is done using `getaddrinfo` so it accepts both
numerical IP addresses (both IPv4 and IPv6) as well as service names
and also handles hostname resolution.

### Parameters

|     Name | Type   | Description                                   |
|---------:|:-------|:----------------------------------------------|
|   packet | `text` | String to send.                               |
|  service | `text` | Service for the worker to listen on           |
| hostname | `text` | Hostname to send to. Defaults to `localhost`. |

## Function `_create`

Create a table for a metric.

This function is not intended to be called directly, even though it is
possible. It is used by the workers to create a table for the metric
when a table for the metric can't be found.

When installing the extension, a default function is created in the
schema where the extension is installed. The default function will
create a table in the schema where the function is installed and with
the following definition:

|  Column | Type          | Description              |
|--------:|:--------------|:-------------------------|
|   _time | `timestamptz` | Timestamp of the metric. |
|   _tags | `jsonb`       | Array of tag names.      |
| _fields | `jsonb`       | Array of field names.    |

> **NOTE:** If you replace this function you need to make sure that a
> table with the same name as the metric is created. If you do not,
> this function will be called again for each received metric.

### Parameters

|   Name | Type     | Description                                                                 |
|-------:|:---------|:----------------------------------------------------------------------------|
| metric | `name`   | Name of metric to create. The function should create a table with this name |
|   tags | `name[]` | Array of tag names. Note that the values of the tags are not given.         |
| fields | `name[]` | Array of field names. Note that the values of the fields are not given.     |

### Returns

A OID of type `regclass`, which is the OID of the created table. This
OID will then be used when writing the metric.

### Examples

All the examples below assume that you have created a schema `metrics`
for the metrics and installed the extension there:

```sql
CREATE SCHEMA metrics;
CREATE EXTENSION influx WITH SCHEMA metrics;
```

To not create tables by default, you have to drop the function, but to
do that, you first need to remove it from the extension using `ALTER
TABLE`:

```sql
ALTER EXTENSION influx DROP FUNCTION metric._create;
DROP FUNCTION metric._create;
```

To use a function that creates a table that just creates the `_fields`
column as a JSON (not JSONB) you can use the following definition:

```sql
CREATE OR REPLACE FUNCTION metrics._create(metric name, tags name[], fields name[])
RETURNS regclass AS $$
BEGIN
   EXECUTE format('CREATE TABLE metric.%I (_time timestamp, _fields json)', metric);
   RETURN format('metric.%I', metric)::regclass;
END;
$$ LANGUAGE plpgsql;
```
