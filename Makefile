EXTENSION = influx
DATA = influx--0.1.sql
MODULE = influx

REGRESS = parse
REGRESS_OPTS += --load-extension=influx

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

