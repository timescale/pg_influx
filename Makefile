EXTENSION = influx
DATA = influx--0.1.sql
MODULE_big = influx
OBJS = influx.o worker.o network.o

REGRESS = parse worker
REGRESS_OPTS += --load-extension=influx

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

worker.o: worker.c worker.h network.h
network.o: network.c network.h
influx.o: influx.c

