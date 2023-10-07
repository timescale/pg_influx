CREATE SCHEMA db_create;
CREATE EXTENSION influx WITH SCHEMA db_create;
SELECT pg_sleep(1) FROM db_create.launch_udp_worker(4711::text);

-- Should create the table
\d db_create.*
CALL db_create.send_udp_packet('system,host=fury uptime=607641i 1574753954000000000', 4711::text);
SELECT pg_sleep(1);
\d db_create.*
DROP TABLE db_create.system;

-- Replace the function with something else
CREATE OR REPLACE FUNCTION db_create._create("metric" name, "tags" name[], "fields" name[])
RETURNS regclass AS $$
BEGIN
   EXECUTE format('CREATE TABLE db_create.%I (_time timestamp, _fields json)', metric);
   RETURN format('%I.%I', 'db_create', metric)::regclass;
END;
$$ LANGUAGE plpgsql;

\d db_create.*
CALL db_create.send_udp_packet('disk,device=nvme0n1p1 free=527806464i 1574753954000000000', 4711::text);
SELECT pg_sleep(1);
\d db_create.*
DROP TABLE db_create.disk;

-- Drop the function to see that the tables are not auto-created
ALTER EXTENSION influx DROP FUNCTION db_create._create;
DROP FUNCTION db_create._create;

\d db_create.*
CALL db_create.send_udp_packet('cpu,host=fury uptime=607641i 1574753954000000000', 4711::text);
SELECT pg_sleep(1);
\d db_create.*

SELECT pg_terminate_backend(pid) FROM pg_stat_activity WHERE backend_type like '%Influx%';

DROP EXTENSION influx;
DROP SCHEMA db_create;
