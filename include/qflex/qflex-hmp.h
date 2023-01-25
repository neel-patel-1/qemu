#ifndef QFLEX_HMP_H
#define QFLEX_HMP_H

#include "qemu-common.h"
#include "qemu/readline.h"
#include "qapi-types.h"
#include "qapi/qmp/qdict.h"

void hmp_flexus_setDebug(Monitor *mon, const QDict *qdict);
void hmp_flexus_setStatInterval(Monitor *mon, const QDict *qdict);
void hmp_flexus_setProfileInterval(Monitor *mon, const QDict *qdict);
void hmp_flexus_setRegionInterval(Monitor *mon, const QDict *qdict);
void hmp_flexus_setTimestampInterval(Monitor *mon, const QDict *qdict);
void hmp_flexus_printCycleCount(Monitor *mon, const QDict *qdict);
void hmp_flexus_setStopCycle(Monitor *mon, const QDict *qdict);
void hmp_flexus_setBreakCPU(Monitor *mon, const QDict *qdict);
void hmp_flexus_setBreakInsn(Monitor *mon, const QDict *qdict);
void hmp_flexus_printProfile(Monitor *mon, const QDict *qdict);
void hmp_flexus_resetProfile(Monitor *mon, const QDict *qdict);
void hmp_flexus_writeProfile(Monitor *mon, const QDict *qdict);
void hmp_flexus_printConfiguration(Monitor *mon, const QDict *qdict);
void hmp_flexus_writeConfiguration(Monitor *mon, const QDict *qdict);
void hmp_flexus_parseConfiguration(Monitor *mon, const QDict *qdict);
void hmp_flexus_setConfiguration(Monitor *mon, const QDict *qdict);
void hmp_flexus_printMeasurement(Monitor *mon, const QDict *qdict);
void hmp_flexus_listMeasurements(Monitor *mon, const QDict *qdict);
void hmp_flexus_writeMeasurement(Monitor *mon, const QDict *qdict);
void hmp_flexus_enterFastMode(Monitor *mon, const QDict *qdict);
void hmp_flexus_leaveFastMode(Monitor *mon, const QDict *qdict);
void hmp_flexus_backupStats(Monitor *mon, const QDict *qdict);
void hmp_flexus_saveStats(Monitor *mon, const QDict *qdict);
void hmp_flexus_reloadDebugCfg(Monitor *mon, const QDict *qdict);
void hmp_flexus_addDebugCfg(Monitor *mon, const QDict *qdict);
void hmp_flexus_enableCategory(Monitor *mon, const QDict *qdict);
void hmp_flexus_disableCategory(Monitor *mon, const QDict *qdict);
void hmp_flexus_listCategories(Monitor *mon, const QDict *qdict);
void hmp_flexus_enableComponent(Monitor *mon, const QDict *qdict);
void hmp_flexus_disableComponent(Monitor *mon, const QDict *qdict);
void hmp_flexus_listComponents(Monitor *mon, const QDict *qdict);
void hmp_flexus_printDebugConfiguration(Monitor *mon, const QDict *qdict);
void hmp_flexus_writeDebugConfiguration(Monitor *mon, const QDict *qdict);
void hmp_flexus_log(Monitor *mon, const QDict *qdict);
void hmp_flexus_printMMU(Monitor *mon, const QDict *qdict);

#ifdef CONFIG_EXTSNAP
void hmp_savevm_ext(Monitor *mon, const QDict *qdict);
void hmp_loadvm_ext(Monitor *mon, const QDict *qdict);
#endif

#ifdef CONFIG_QUANTUM
void hmp_quantum_pause(Monitor *mon, const QDict *qdict);
void hmp_quantum_set(Monitor *mon, const QDict *qdict);
void hmp_quantum_get(Monitor *mon, const QDict *qdict);
void hmp_quantum_cpu_dbg(Monitor *mon,  const QDict *qdict);
void hmp_quantum_cpu_zero_all(Monitor *mon,  const QDict *qdict);
#endif

#endif