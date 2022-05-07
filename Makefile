EXTENSION = influx
DATA = influx--0.2.sql
MODULE_big = influx
OBJS = influx.o worker.o network.o parser.o

REGRESS = parse worker
REGRESS_OPTS += --load-extension=influx

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

influx.o: influx.c parser.h
network.o: network.c network.h
parser.o: parser.c parser.h
worker.o: worker.c worker.h network.h

