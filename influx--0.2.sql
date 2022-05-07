-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION influx" to load this file. \quit

-- Launch a new worker that listens on a port and writes to a namespace
CREATE FUNCTION worker_launch(ns regnamespace, service text)
RETURNS integer
LANGUAGE C AS 'MODULE_PATHNAME';

-- Send a packet over UDP to a host and service
CREATE PROCEDURE send_packet(packet text, service text, hostname text = 'localhost')
LANGUAGE C AS 'MODULE_PATHNAME';

-- Parse InfluxDB Line Protocol packet
CREATE FUNCTION parse_influx(text)
RETURNS TABLE (_metric text, _time timestamp, _tags jsonb, _fields jsonb)
LANGUAGE C AS 'MODULE_PATHNAME';
