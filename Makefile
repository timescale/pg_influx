# Copyright 2022 Timescale Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

EXTENSION = influx
DATA = influx--0.4.sql
MODULE_big = influx
OBJS = influx.o worker.o network.o ingest.o cache.o metric.o

REGRESS = parse worker inval
REGRESS_OPTS += --load-extension=influx

package-version = $(shell git describe --long --match="v[0-9]*" | cut -d- -f1 | sed 's/^v//')
dist-name = postgresql-pg-influx-$(package-version)
dist-file = $(dist-name).tar.gz

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

# .gitattributes make sure that we do not include .github and other
# directories that are part of the repository CI/CD.
dist:
	git archive --prefix=$(dist-name)/ --format=tar.gz -o $(dist-file) HEAD

cache.o: cache.c cache.h
influx.o: influx.c influx.h ingest.h metric.h worker.h
ingest.o: ingest.c ingest.h metric.h
metric.o: metric.c metric.h cache.h
network.o: network.c network.h
worker.o: worker.c worker.h cache.h influx.h ingest.h metric.h network.h

