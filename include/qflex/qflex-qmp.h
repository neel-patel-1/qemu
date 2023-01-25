#ifndef QMP_COMMANDS_H
#define QMP_COMMANDS_H

#include "qapi-types.h"
#include "qapi/qmp/qdict.h"
#include "qapi/qmp/dispatch.h"
#include "qapi/error.h"

void qmp_flexus_addDebugCfg(const char *filename, Error **errp);
void qmp_marshal_flexus_addDebugCfg(QDict *args, QObject **ret, Error **errp);
void qmp_flexus_backupStats(const char *filename, Error **errp);
void qmp_marshal_flexus_backupStats(QDict *args, QObject **ret, Error **errp);
void qmp_flexus_disableCategory(const char *component, Error **errp);
void qmp_marshal_flexus_disableCategory(QDict *args, QObject **ret, Error **errp);
void qmp_flexus_disableComponent(const char *component, const char *index, Error **errp);
void qmp_marshal_flexus_disableComponent(QDict *args, QObject **ret, Error **errp);
void qmp_flexus_enableCategory(const char *component, Error **errp);
void qmp_marshal_flexus_enableCategory(QDict *args, QObject **ret, Error **errp);
void qmp_flexus_enableComponent(const char *component, const char *index, Error **errp);
void qmp_marshal_flexus_enableComponent(QDict *args, QObject **ret, Error **errp);
void qmp_flexus_enterFastMode(Error **errp);
void qmp_marshal_flexus_enterFastMode(QDict *args, QObject **ret, Error **errp);
void qmp_flexus_leaveFastMode(Error **errp);
void qmp_marshal_flexus_leaveFastMode(QDict *args, QObject **ret, Error **errp);
void qmp_flexus_listCategories(Error **errp);
void qmp_marshal_flexus_listCategories(QDict *args, QObject **ret, Error **errp);
void qmp_flexus_listComponents(Error **errp);
void qmp_marshal_flexus_listComponents(QDict *args, QObject **ret, Error **errp);
void qmp_flexus_listMeasurements(Error **errp);
void qmp_marshal_flexus_listMeasurements(QDict *args, QObject **ret, Error **errp);
void qmp_flexus_log(const char *name, const char *interval, const char *regex, Error **errp);
void qmp_marshal_flexus_log(QDict *args, QObject **ret, Error **errp);
void qmp_flexus_parseConfiguration(const char *filename, Error **errp);
void qmp_marshal_flexus_parseConfiguration(QDict *args, QObject **ret, Error **errp);
void qmp_flexus_printConfiguration(Error **errp);
void qmp_marshal_flexus_printConfiguration(QDict *args, QObject **ret, Error **errp);
void qmp_flexus_printCycleCount(Error **errp);
void qmp_marshal_flexus_printCycleCount(QDict *args, QObject **ret, Error **errp);
void qmp_flexus_printDebugConfiguration(Error **errp);
void qmp_marshal_flexus_printDebugConfiguration(QDict *args, QObject **ret, Error **errp);
void qmp_flexus_printMMU(const char *cpu, Error **errp);
void qmp_marshal_flexus_printMMU(QDict *args, QObject **ret, Error **errp);
void qmp_flexus_printMeasurement(const char *measurement, Error **errp);
void qmp_marshal_flexus_printMeasurement(QDict *args, QObject **ret, Error **errp);
void qmp_flexus_printProfile(Error **errp);
void qmp_marshal_flexus_printProfile(QDict *args, QObject **ret, Error **errp);
void qmp_flexus_quiesce(Error **errp);
void qmp_marshal_flexus_quiesce(QDict *args, QObject **ret, Error **errp);
void qmp_flexus_reloadDebugCfg(Error **errp);
void qmp_marshal_flexus_reloadDebugCfg(QDict *args, QObject **ret, Error **errp);
void qmp_flexus_resetProfile(Error **errp);
void qmp_marshal_flexus_resetProfile(QDict *args, QObject **ret, Error **errp);
void qmp_flexus_saveStats(const char *filename, Error **errp);
void qmp_marshal_flexus_saveStats(QDict *args, QObject **ret, Error **errp);
void qmp_flexus_setBreakCPU(const char *value, Error **errp);
void qmp_marshal_flexus_setBreakCPU(QDict *args, QObject **ret, Error **errp);
void qmp_flexus_setBreakInsn(const char *value, Error **errp);
void qmp_marshal_flexus_setBreakInsn(QDict *args, QObject **ret, Error **errp);
void qmp_flexus_setConfiguration(const char *name, const char *value, Error **errp);
void qmp_marshal_flexus_setConfiguration(QDict *args, QObject **ret, Error **errp);
void qmp_flexus_setDebug(const char *debugseverity, Error **errp);
void qmp_marshal_flexus_setDebug(QDict *args, QObject **ret, Error **errp);
void qmp_flexus_setProfileInterval(const char *value, Error **errp);
void qmp_marshal_flexus_setProfileInterval(QDict *args, QObject **ret, Error **errp);
void qmp_flexus_setRegionInterval(const char *value, Error **errp);
void qmp_marshal_flexus_setRegionInterval(QDict *args, QObject **ret, Error **errp);
void qmp_flexus_setStatInterval(const char *value, Error **errp);
void qmp_marshal_flexus_setStatInterval(QDict *args, QObject **ret, Error **errp);
void qmp_flexus_setStopCycle(const char *value, Error **errp);
void qmp_marshal_flexus_setStopCycle(QDict *args, QObject **ret, Error **errp);
void qmp_flexus_setTimestampInterval(const char *value, Error **errp);
void qmp_marshal_flexus_setTimestampInterval(QDict *args, QObject **ret, Error **errp);
void qmp_flexus_terminateSimulation(Error **errp);
void qmp_marshal_flexus_terminateSimulation(QDict *args, QObject **ret, Error **errp);
void qmp_flexus_writeConfiguration(const char *filename, Error **errp);
void qmp_marshal_flexus_writeConfiguration(QDict *args, QObject **ret, Error **errp);
void qmp_flexus_writeDebugConfiguration(Error **errp);
void qmp_marshal_flexus_writeDebugConfiguration(QDict *args, QObject **ret, Error **errp);
void qmp_flexus_writeMeasurement(const char *measurement, const char *filename, Error **errp);
void qmp_marshal_flexus_writeMeasurement(QDict *args, QObject **ret, Error **errp);
void qmp_flexus_writeProfile(const char *filename, Error **errp);
void qmp_marshal_flexus_writeProfile(QDict *args, QObject **ret, Error **errp);

#endif