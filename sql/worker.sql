CREATE SCHEMA db_worker;
CREATE TABLE db_worker.disk(_time timestamptz, host text, device text, _tags jsonb, _fields jsonb);
CREATE TABLE db_worker.system(_time timestamp, host text, uptime int, _tags jsonb, _fields jsonb);

CREATE EXTENSION influx WITH SCHEMA db_worker;

\set VERBOSITY terse
\x on
SELECT pg_sleep(1), pid AS worker_pid FROM db_worker.worker_launch('db_worker', 4711::text) AS pid \gset
CALL db_worker.send_packet('cpu,cpu=cpu0,host=fury usage_system=2.0408163264927324,usage_user=2.0408163264927324 1574753954000000000', 4711::text);
CALL db_worker.send_packet('cpu,cpu=cpu1,host=fury usage_system=3.921568627286635,usage_user=3.9215686274649673 1574753954000000000', 4711::text);
CALL db_worker.send_packet('disk,device=nvme0n1p2,fstype=ext4,host=fury,mode=rw,path=/ free=912578965504i,total=1006530654208i,used=42751348736i,used_percent=4.475033200428718 1574753954000000000', 4711::text);
CALL db_worker.send_packet('disk,device=nvme0n1p1,fstype=vfat,host=fury,mode=rw,path=/boot/efi free=527806464i,total=535805952i,used=7999488i,used_percent=1.4929822952022749 1574753954000000000', 4711::text);
CALL db_worker.send_packet('system,host=fury load1=2.13,load15=0.84,load5=1.18,n_cpus=8i,n_users=1i 1574753954000000000', 4711::text);
CALL db_worker.send_packet('system,host=fury uptime=607641i 1574753954000000000', 4711::text);
CALL db_worker.send_packet('system,host=fury uptime_format="7 days,  0:47" 1574753954000000000', 4711::text);
SELECT pg_sleep(2);

SELECT * FROM db_worker.cpu;	--Automatically created
SELECT * FROM db_worker.disk;
SELECT * FROM db_worker.system;

SELECT count(*) FROM pg_stat_activity WHERE pid = :worker_pid;
-- Syntax error, but the worker should not stop
CALL db_worker.send_packet('system,host=fury', 4711::text);
SELECT pg_sleep(1);
SELECT count(*) FROM pg_stat_activity WHERE pid = :worker_pid;

SELECT pg_terminate_backend(pid) FROM pg_stat_activity WHERE backend_type like '%Influx%';

DROP EXTENSION influx;
DROP TABLE db_worker.cpu;
DROP TABLE db_worker.disk;
DROP TABLE db_worker.system;
DROP SCHEMA db_worker;
