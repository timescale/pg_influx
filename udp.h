#ifndef UDP_H_
#define UDP_H_

#include <postgres.h>

#include "worker.h"

#define INFLUX_UDP_FUNCTION_NAME "InfluxUdpWorkerMain"

void InfluxUdpWorkerInit(BackgroundWorker *worker, WorkerArgs *args);
void InfluxUdpWorkerMain(Datum dbid) pg_attribute_noreturn();
void StartUdpBackgroundWorkers(WorkerArgs *args);

#endif /* UDP_H_ */
