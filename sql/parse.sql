CREATE EXTENSION influx;

-- Expect 2016-06-13T17:43:50.1004002Z (from protocol specification)
select * from parse_influx('cpu foo=12 1465839830100400200');

SELECT * FROM parse_influx(E'measurement,tag=foo field=12i 1465839830100400200');
SELECT * FROM parse_influx(E'measurement,tag=\\"foo field=12i 1465839830100400200');
SELECT * FROM parse_influx(E'measurement,tag=foo field=12i 1465839830100400200\nmeasurement,tag=bar field=12 1465839830100400200');
SELECT * FROM parse_influx(E'myMeasurement,tag1=value1,tag2=value2 fieldKey="fieldValue" 1556813561098000000');
SELECT * FROM parse_influx(E'myMeasurementName fieldKey="fieldValue" 1556813561098000000');
SELECT * FROM parse_influx(E'myMeasurementName fieldKey="fieldValue" 1556813561098000000\nmyMeasurement,tag1=value1,tag2=value2 fieldKey="fieldValue" 1556813561098000000');
SELECT * FROM parse_influx(E'disk,mode=rw,path=/boot/efi free=527806464i,inodes_free=i,total=0i,used_percent=1.4929822952022749 1574753954000000000');
SELECT * FROM parse_influx(E'disk,mode=0 free="527806464i",total=\\0i 1574753954000000000');
SELECT * FROM parse_influx(E'disk,mode=0,path=0i free="527806464i",total=\\0i 1574753954000000000');

\set ON_ERROR_STOP OFF
SELECT * FROM parse_influx(E'measurement,tag field=12 12345');
SELECT * FROM parse_influx(E'measurement, field=12 12345');
SELECT * FROM parse_influx(E'measurement 12345');
\set ON_ERROR_STOP ON

CREATE TABLE lines(line text);
COPY lines FROM stdin;
cpu,cpu=cpu0,host=fury usage_guest=0,usage_guest_nice=0,usage_idle=95.91836734330231,usage_iowait=0,usage_irq=0,usage_nice=0,usage_softirq=0,usage_steal=0,usage_system=2.0408163264927324,usage_user=2.0408163264927324 1574753954000000000
cpu,cpu=cpu1,host=fury usage_guest=0,usage_guest_nice=0,usage_idle=92.1568627436434,usage_iowait=0,usage_irq=0,usage_nice=0,usage_softirq=0,usage_steal=0,usage_system=3.921568627286635,usage_user=3.9215686274649673 1574753954000000000
cpu,cpu=cpu2,host=fury usage_guest=0,usage_guest_nice=0,usage_idle=90.74074073814575,usage_iowait=0,usage_irq=0,usage_nice=0,usage_softirq=1.8518518517645204,usage_steal=0,usage_system=3.703703703360616,usage_user=3.7037037035290408 1574753954000000000
cpu,cpu=cpu3,host=fury usage_guest=0,usage_guest_nice=0,usage_idle=96.00000000209548,usage_iowait=0,usage_irq=0,usage_nice=0,usage_softirq=0,usage_steal=0,usage_system=2.0000000000436557,usage_user=2.0000000000436557 1574753954000000000
cpu,cpu=cpu4,host=fury usage_guest=0,usage_guest_nice=0,usage_idle=91.83673469254418,usage_iowait=0,usage_irq=0,usage_nice=0,usage_softirq=0,usage_steal=0,usage_system=0,usage_user=8.163265305599708 1574753954000000000
cpu,cpu=cpu5,host=fury usage_guest=0,usage_guest_nice=0,usage_idle=97.95918367165116,usage_iowait=0,usage_irq=0,usage_nice=0,usage_softirq=0,usage_steal=0,usage_system=2.0408163264927324,usage_user=0 1574753954000000000
cpu,cpu=cpu6,host=fury usage_guest=0,usage_guest_nice=0,usage_idle=96.00000000186265,usage_iowait=0,usage_irq=0,usage_nice=0,usage_softirq=0,usage_steal=0,usage_system=0,usage_user=3.9999999999563443 1574753954000000000
cpu,cpu=cpu7,host=fury usage_guest=0,usage_guest_nice=0,usage_idle=94.00000000023283,usage_iowait=0,usage_irq=0,usage_nice=0,usage_softirq=0,usage_steal=0,usage_system=2.0000000000436557,usage_user=4.0000000000873115 1574753954000000000
cpu,cpu=cpu-total,host=fury usage_guest=0,usage_guest_nice=0,usage_idle=93.38235294084079,usage_iowait=0.24509803921656045,usage_irq=0,usage_nice=0,usage_softirq=0.4901960784331209,usage_steal=0,usage_system=1.9607843135541514,usage_user=3.921568627108303 1574753954000000000
disk,device=nvme0n1p2,fstype=ext4,host=fury,mode=rw,path=/ free=912578965504i,inodes_free=61635422i,inodes_total=62480384i,inodes_used=844962i,total=1006530654208i,used=42751348736i,used_percent=4.475033200428718 1574753954000000000
disk,device=nvme0n1p1,fstype=vfat,host=fury,mode=rw,path=/boot/efi free=527806464i,inodes_free=0i,inodes_total=0i,inodes_used=0i,total=535805952i,used=7999488i,used_percent=1.4929822952022749 1574753954000000000
diskio,host=fury,name=nvme0n1p2 io_time=2595292i,iops_in_progress=0i,read_bytes=309936706560i,read_time=8109392i,reads=6129866i,weighted_io_time=39313044i,write_bytes=80954531840i,write_time=37212815i,writes=2290766i 1574753954000000000
diskio,host=fury,name=loop1 io_time=108i,iops_in_progress=0i,read_bytes=1316864i,read_time=72i,reads=269i,weighted_io_time=32i,write_bytes=0i,write_time=0i,writes=0i 1574753954000000000
diskio,host=fury,name=loop4 io_time=20i,iops_in_progress=0i,read_bytes=351232i,read_time=5i,reads=47i,weighted_io_time=0i,write_bytes=0i,write_time=0i,writes=0i 1574753954000000000
diskio,host=fury,name=loop6 io_time=5184i,iops_in_progress=0i,read_bytes=50564096i,read_time=75687i,reads=48370i,weighted_io_time=68692i,write_bytes=0i,write_time=0i,writes=0i 1574753954000000000
diskio,host=fury,name=nvme0n1 io_time=2599684i,iops_in_progress=0i,read_bytes=309946164224i,read_time=8152676i,reads=6132370i,weighted_io_time=39387288i,write_bytes=80954532864i,write_time=37254690i,writes=2333085i 1574753954000000000
diskio,host=fury,name=nvme0n1p1 io_time=152i,iops_in_progress=0i,read_bytes=6955008i,read_time=42986i,reads=2397i,weighted_io_time=42616i,write_bytes=1024i,write_time=0i,writes=2i 1574753954000000000
diskio,host=fury,name=loop0 io_time=264i,iops_in_progress=0i,read_bytes=1100800i,read_time=51i,reads=779i,weighted_io_time=0i,write_bytes=0i,write_time=0i,writes=0i 1574753954000000000
diskio,host=fury,name=loop2 io_time=79952i,iops_in_progress=0i,read_bytes=493705216i,read_time=1409949i,reads=481841i,weighted_io_time=811912i,write_bytes=0i,write_time=0i,writes=0i 1574753954000000000
diskio,host=fury,name=loop3 io_time=216i,iops_in_progress=0i,read_bytes=1125376i,read_time=124i,reads=803i,weighted_io_time=76i,write_bytes=0i,write_time=0i,writes=0i 1574753954000000000
diskio,host=fury,name=loop5 io_time=204i,iops_in_progress=0i,read_bytes=2736128i,read_time=191i,reads=1672i,weighted_io_time=0i,write_bytes=0i,write_time=0i,writes=0i 1574753954000000000
kernel,host=fury boot_time=1574146313i,context_switches=1659702661i,entropy_avail=3797i,interrupts=838000993i,processes_forked=430825i 1574753954000000000
mem,host=fury active=8663875584i,available=2632663040i,available_percent=15.976348422300173,buffered=32174080i,cached=8126435328i,commit_limit=10386731008i,committed_as=26931036160i,dirty=864256i,free=2134585344i,high_free=0i,high_total=0i,huge_page_size=2097152i,huge_pages_free=0i,huge_pages_total=0i,inactive=4678496256i,low_free=0i,low_total=0i,mapped=5682855936i,page_tables=80105472i,shared=7086600192i,slab=595992576i,swap_cached=55185408i,swap_free=658169856i,swap_total=2147479552i,total=16478502912i,used=6185308160i,used_percent=37.53561954645604,vmalloc_chunk=0i,vmalloc_total=35184372087808i,vmalloc_used=0i,wired=0i,write_back=0i,write_back_tmp=0i 1574753954000000000
processes,host=fury blocked=0i,dead=0i,idle=68i,paging=0i,running=1i,sleeping=298i,stopped=0i,total=367i,total_threads=1363i,unknown=0i,zombies=0i 1574753954000000000
swap,host=fury free=658169856i,total=2147479552i,used=1489309696i,used_percent=69.3515193014513 1574753954000000000
swap,host=fury in=804974592i,out=6606921728i 1574753954000000000
system,host=fury load1=2.13,load15=0.84,load5=1.18,n_cpus=8i,n_users=1i 1574753954000000000
system,host=fury uptime=607641i 1574753954000000000
system,host=fury uptime_format="7 days,  0:47" 1574753954000000000
\.

SELECT * FROM (SELECT parse_influx(line) FROM lines) x;

DROP EXTENSION influx;
