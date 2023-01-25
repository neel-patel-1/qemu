#include "qemu/osdep.h"
#include "qemu-version.h"
#include "qemu/cutils.h"
#include "monitor/monitor.h"
#include "sysemu/sysemu.h"
#include "qemu/config-file.h"
#include "qemu/uuid.h"
#include "qmp-commands.h"
#include "chardev/char.h"
#include "ui/qemu-spice.h"
#include "ui/vnc.h"
#include "sysemu/kvm.h"
#include "sysemu/arch_init.h"
#include "hw/qdev.h"
#include "sysemu/blockdev.h"
#include "sysemu/block-backend.h"
#include "qom/qom-qobject.h"
#include "qapi/qmp/qerror.h"
#include "qapi/qmp/qobject.h"
#include "qapi/qobject-input-visitor.h"
#include "hw/boards.h"
#include "qom/object_interfaces.h"

#include "qflex/qflex.h"
#include "qflex/qflex-qmp.h"


void qmp_flexus_addDebugCfg(const char *filename, Error **errp)
{
    flexus_addDebugCfg(filename, errp);
}
void qmp_flexus_setBreakCPU(const char* value, Error **errp)
{
    flexus_setBreakCPU(value, errp);
}
void qmp_flexus_backupStats(const char *filename, Error **errp)
{
    flexus_backupStats(filename, errp);
}

void qmp_flexus_disableCategory(const char *component, Error **errp)
{
    flexus_disableCategory(component, errp);
}

void qmp_flexus_disableComponent(const char *component, const char *index, Error **errp)
{
    flexus_disableComponent(component, index, errp);
}

void qmp_flexus_enableCategory(const char *component, Error **errp)
{
    flexus_enableCategory(component, errp);
}

void qmp_flexus_enableComponent(const char *component, const char *index, Error **errp)
{
    flexus_enableComponent(component, index, errp);
}

void qmp_flexus_enterFastMode(Error **errp)
{
    flexus_enterFastMode(errp);
}

void qmp_flexus_leaveFastMode(Error **errp)
{
    flexus_leaveFastMode(errp);
}

void qmp_flexus_listCategories(Error **errp)
{
    flexus_listCategories(errp);
}

void qmp_flexus_listComponents(Error **errp)
{
    flexus_listComponents(errp);
}

void qmp_flexus_listMeasurements(Error **errp)
{
    flexus_listMeasurements(errp);
}

void qmp_flexus_log(const char *name, const char *interval, const char *regex, Error **errp)
{
    flexus_log(name, interval, regex, errp);
}

void qmp_flexus_parseConfiguration(const char *filename, Error **errp)
{
    flexus_parseConfiguration(filename, errp);
}

void qmp_flexus_printConfiguration(Error **errp)
{
    flexus_printConfiguration(errp);
}

void qmp_flexus_printCycleCount(Error **errp)
{
    flexus_printCycleCount(errp);
}

void qmp_flexus_printDebugConfiguration(Error **errp)
{
    flexus_printDebugConfiguration(errp);
}

void qmp_flexus_printMMU(const char* cpu, Error **errp)
{
    flexus_printMMU(cpu, errp);
}

void qmp_flexus_printMeasurement(const char *measurement, Error **errp)
{
    flexus_printMeasurement(measurement, errp);
}

void qmp_flexus_printProfile(Error **errp)
{
    flexus_printProfile(errp);
}

void qmp_flexus_quiesce(Error **errp)
{
    flexus_quiesce(errp);
}

void qmp_flexus_reloadDebugCfg(Error **errp)
{
    flexus_reloadDebugCfg(errp);
}

void qmp_flexus_resetProfile(Error **errp)
{
    flexus_resetProfile(errp);
}

void qmp_flexus_saveStats(const char *filename, Error **errp)
{
    flexus_saveStats(filename, errp);
}

void qmp_flexus_setBreakInsn(const char *value, Error **errp)
{
    flexus_setBreakInsn(value, errp);
}

void qmp_flexus_setConfiguration(const char *name, const char *value, Error **errp)
{
    flexus_setConfiguration(name, value, errp);
}

void qmp_flexus_setDebug(const char *debugseverity, Error **errp)
{
    flexus_setDebug(debugseverity, errp);
}

void qmp_flexus_setProfileInterval(const char *value, Error **errp)
{
    flexus_setProfileInterval(value, errp);
}

void qmp_flexus_setRegionInterval(const char *value, Error **errp)
{
    flexus_setRegionInterval(value, errp);
}

void qmp_flexus_setStatInterval(const char *value, Error **errp)
{
    flexus_setStatInterval(value, errp);
}

void qmp_flexus_setStopCycle(const char *value, Error **errp)
{
    flexus_setStopCycle(value, errp);
}

void qmp_flexus_setTimestampInterval(const char *value, Error **errp)
{
    flexus_setTimestampInterval(value, errp);
}

void qmp_flexus_terminateSimulation(Error **errp)
{
    flexus_terminateSimulation(errp);
}

void qmp_flexus_writeConfiguration(const char *filename, Error **errp)
{
    flexus_writeConfiguration(filename, errp);
}

void qmp_flexus_writeDebugConfiguration(Error **errp)
{
    flexus_writeDebugConfiguration(errp);
}

void qmp_flexus_writeMeasurement(const char *measurement, const char *filename, Error **errp)
{
    flexus_writeMeasurement(measurement, filename, errp);
}

void qmp_flexus_writeProfile(const char *filename, Error **errp)
{
    flexus_writeProfile(filename, errp);
}

