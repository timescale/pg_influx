#ifndef GUC_H_
#define GUC_H_

extern int InfluxWorkersCount;
extern char *InfluxServiceName;
extern char *InfluxSchemaName;
extern char *InfluxDatabaseName;
extern char *InfluxRoleName;

extern void InfluxGucInitDynamic(void);
extern void InfluxGucInitStatic(void);

#endif /* GUC_H_ */
