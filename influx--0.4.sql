-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION influx" to load this file. \quit

-- Launch a new worker that listens on a port and writes to a namespace
CREATE FUNCTION launch_udp_worker(ns regnamespace, service text)
RETURNS integer
LANGUAGE C AS '$libdir/influx.so';

CREATE FUNCTION launch_udp_worker(service text)
RETURNS integer
LANGUAGE C AS '$libdir/influx.so';

-- Send a packet over UDP to a host and service
CREATE PROCEDURE send_udp_packet(packet text, service text, hostname text = 'localhost')
LANGUAGE C AS '$libdir/influx.so';

-- Parse InfluxDB Line Protocol packet
CREATE FUNCTION parse_influx(text)
RETURNS TABLE (_metric text, _time timestamp, _tags jsonb, _fields jsonb)
LANGUAGE C AS '$libdir/influx.so';

CREATE FUNCTION _create("metric" name, "tags" name[], "fields" name[])
RETURNS regclass
LANGUAGE C AS '$libdir/influx.so', 'default_create';
