CREATE SCHEMA db_worker;
CREATE TABLE db_worker.cpu(_time timestamptz, _tags jsonb, _fields jsonb);

\set VERBOSITY terse
\x on
SELECT pg_sleep(1) FROM worker_launch('db_worker', 4711::text);
CALL send_packet('cpu,cpu=cpu0,host=fury usage_system=2.04,usage_user=5.40 1574753954000000000', 4711::text);
SELECT pg_sleep(1);
SELECT * FROM db_worker.cpu;
ALTER TABLE db_worker.cpu ADD COLUMN cpu text;
CALL send_packet('cpu,cpu=cpu0,host=fury usage_system=2.04,usage_user=5.40 1574753955000000000', 4711::text);
SELECT pg_sleep(1);
SELECT * FROM db_worker.cpu;
ALTER TABLE db_worker.cpu ADD COLUMN host text;
CALL send_packet('cpu,cpu=cpu0,host=fury usage_system=2.04,usage_user=5.40 1574753956000000000', 4711::text);
SELECT pg_sleep(1);
SELECT * FROM db_worker.cpu;
SELECT pg_terminate_backend(pid) FROM pg_stat_activity WHERE backend_type like '%Influx%';

DROP TABLE db_worker.cpu;
DROP SCHEMA db_worker;
