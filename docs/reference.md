# Reference Manual for `pg_influx`

## Function `worker_launch`

### Parameters

|      Name | Type           | Description                         |
|----------:|:---------------|:------------------------------------|
| namespace | `regnamespace` | Namespace for the metrics.          |
|   service | `text`         | Service for the worker to listen on |

The service is looked up using `getaddrinfo` so it is possible to
either provide a port number of service name that will be translated
to a corresponding port number. Both IPv4 and IPv6 addresses are
supported through `getaddrinfo`.

The function will bind to the first address suitable for receiving UDP
traffic (using `AI_PASSIVE` and a NULL node name), which means that it
is not possible to select the IP address to listen on.

### Return value

Integer indicating the PID of the started worker. This can be used
with [`pg_terminate_backend`][1] to terminate the backend.

[1]: https://www.postgresql.org/docs/current/functions-admin.html#FUNCTIONS-ADMIN-SIGNAL-TABLE

## Procedure `send_packet`

### Parameters

|     Name | Type   | Description                                   |
|---------:|:-------|:----------------------------------------------|
|   packet | `text` | String to send.                               |
|  service | `text` | Service for the worker to listen on           |
| hostname | `text` | Hostname to send to. Defaults to `localhost`. |

The text string provided will be sent as an UDP packet to the host and
service given by `service` and `hostname` parameters. Service and
hostname lookup is done using `getaddrinfo` so it accepts both
numerical IP addresses (both IPv4 and IPv6) as well as service names
and also handles hostname resolution.

