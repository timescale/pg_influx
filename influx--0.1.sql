-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION influx" to load this file. \quit

CREATE TYPE pair AS (key text, value text);

CREATE FUNCTION split_pair(text)
RETURNS pair LANGUAGE C AS 'MODULE_PATHNAME';
