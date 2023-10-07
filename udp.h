#ifndef UDP_H_
#define UDP_H_

#include <postgres.h>

#include "worker.h"

#define INFLUX_UDP_FUNCTION_NAME "InfluxUdpWorkerMain"

void InfluxUdpWorkerInit(BackgroundWorker *worker, WorkerArgs *args);
void StartUdpBackgroundWorkers(WorkerArgs *args);

void PGDLLEXPORT InfluxUdpWorkerMain(Datum dbid) pg_attribute_noreturn();

#endif /* UDP_H_ */
