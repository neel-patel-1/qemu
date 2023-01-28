#include "qemu/osdep.h"
#include "hmp.h"
#include "net/net.h"
#include "net/eth.h"
#include "chardev/char.h"
#include "sysemu/block-backend.h"
#include "sysemu/sysemu.h"
#include "qemu/config-file.h"
#include "qemu/option.h"
#include "qemu/timer.h"
#include "qmp-commands.h"
#include "qemu/sockets.h"
#include "monitor/monitor.h"
#include "monitor/qdev.h"
#include "qapi/opts-visitor.h"
#include "qapi/qmp/qerror.h"
#include "qapi/string-input-visitor.h"
#include "qapi/string-output-visitor.h"
#include "qapi-visit.h"
#include "qom/object_interfaces.h"
#include "ui/console.h"
#include "block/nbd.h"
#include "block/qapi.h"
#include "qemu-io.h"
#include "qemu/cutils.h"
#include "qemu/error-report.h"
#include "exec/ramlist.h"
#include "hw/intc/intc.h"
#include "migration/snapshot.h"
#include "migration/misc.h"
#include "qapi/error.h"

#include "../../../libqflex/api.h"
#include "qflex/qflex.h"

void flexus_qmp(qmp_flexus_cmd_t cmd, const char* args, Error **errp){
    if (flexus_in_simulation()) {
        flexus_dynlib_fns.qflex_sim_qmp(cmd, args);
    } else {
        error_setg(errp, "flexus is not running");
    }
}

static void hmp_handle_error(Monitor *mon, Error **errp)
{
    assert(errp);
    if (*errp) {
        error_report_err(*errp);
    }
}

void hmp_flexus_setDebug(Monitor *mon, const QDict *qdict)
{
    Error *err = NULL;
    const char *severity = qdict_get_str(qdict, "severity");

    qmp_flexus_setDebug(severity, &err);
    hmp_handle_error(mon,&err);
}
void hmp_flexus_setStatInterval(Monitor *mon, const QDict *qdict)
{
    Error *err = NULL;
    const char *value = qdict_get_str(qdict, "value");

    qmp_flexus_setDebug(value, &err);
    hmp_handle_error(mon,&err);
}
void hmp_flexus_setProfileInterval(Monitor *mon, const QDict *qdict)
{
    Error *err = NULL;
    const char *value = qdict_get_str(qdict, "value");

    qmp_flexus_setDebug(value, &err);
    hmp_handle_error(mon,&err);
}
void hmp_flexus_setRegionInterval(Monitor *mon, const QDict *qdict)
{
    Error *err = NULL;
    const char *value = qdict_get_str(qdict, "value");

    qmp_flexus_setDebug(value, &err);
    hmp_handle_error(mon,&err);
}
void hmp_flexus_setTimestampInterval(Monitor *mon, const QDict *qdict)
{
    Error *err = NULL;
    const char *value = qdict_get_str(qdict, "value");

    qmp_flexus_setDebug(value, &err);
    hmp_handle_error(mon,&err);
}

void hmp_flexus_printCycleCount(Monitor *mon, const QDict *qdict)
{
        Error *err = NULL;
        qmp_flexus_printCycleCount(&err);
        hmp_handle_error(mon,&err);
}


void hmp_flexus_setStopCycle(Monitor *mon, const QDict *qdict)
{
        Error *err = NULL;
        const char *value = qdict_get_str(qdict, "value");
        qmp_flexus_setStopCycle(value, &err);
        hmp_handle_error(mon,&err);
}


void hmp_flexus_setBreakCPU(Monitor *mon, const QDict *qdict)
{
        Error *err = NULL;
        const char *value = qdict_get_str(qdict, "value");

        qmp_flexus_setBreakCPU(value, &err);
        hmp_handle_error(mon,&err);
}


void hmp_flexus_setBreakInsn(Monitor *mon, const QDict *qdict)
{
        Error *err = NULL;
        const char *value = qdict_get_str(qdict, "value");

        qmp_flexus_setBreakInsn(value, &err);
        hmp_handle_error(mon,&err);
}


void hmp_flexus_printProfile(Monitor *mon, const QDict *qdict)
{
        Error *err = NULL;
        qmp_flexus_printProfile(&err);
        hmp_handle_error(mon,&err);
}


void hmp_flexus_resetProfile(Monitor *mon, const QDict *qdict)
{
        Error *err = NULL;
        qmp_flexus_resetProfile(&err);
        hmp_handle_error(mon,&err);
}


void hmp_flexus_writeProfile(Monitor *mon, const QDict *qdict)
{
        Error *err = NULL;
        const char *filename = qdict_get_str(qdict, "filename");

        qmp_flexus_writeProfile(filename, &err);
        hmp_handle_error(mon,&err);
}


void hmp_flexus_printConfiguration(Monitor *mon, const QDict *qdict)
{
        Error *err = NULL;
        qmp_flexus_printConfiguration(&err);
        hmp_handle_error(mon,&err);
}


void hmp_flexus_writeConfiguration(Monitor *mon, const QDict *qdict)
{
        Error *err = NULL;
        const char *filename = qdict_get_str(qdict, "filename");

        qmp_flexus_writeConfiguration(filename, &err);
        hmp_handle_error(mon,&err);
}


void hmp_flexus_parseConfiguration(Monitor *mon, const QDict *qdict)
{
        Error *err = NULL;
        const char *filename = qdict_get_str(qdict, "filename");

        qmp_flexus_parseConfiguration(filename, &err);
        hmp_handle_error(mon,&err);
}


void hmp_flexus_setConfiguration(Monitor *mon, const QDict *qdict)
{
        Error *err = NULL;
        const char *name = qdict_get_str(qdict, "name");
        const char *value = qdict_get_str(qdict, "value");

        qmp_flexus_setConfiguration(name, value, &err);
        hmp_handle_error(mon,&err);
}


void hmp_flexus_printMeasurement(Monitor *mon, const QDict *qdict)
{
        Error *err = NULL;
        const char *measurement = qdict_get_str(qdict, "measurement");

        qmp_flexus_printMeasurement(measurement, &err);
        hmp_handle_error(mon,&err);
}


void hmp_flexus_listMeasurements(Monitor *mon, const QDict *qdict)
{
        Error *err = NULL;
        qmp_flexus_listMeasurements(&err);
        hmp_handle_error(mon,&err);
}


void hmp_flexus_writeMeasurement(Monitor *mon, const QDict *qdict)
{
        Error *err = NULL;
        const char *measurement = qdict_get_str(qdict, "measurement");
        const char *filename = qdict_get_str(qdict, "filename");

        qmp_flexus_writeMeasurement(measurement, filename, &err);
        hmp_handle_error(mon,&err);
}


void hmp_flexus_enterFastMode(Monitor *mon, const QDict *qdict)
{
        Error *err = NULL;
        qmp_flexus_enterFastMode(&err);
        hmp_handle_error(mon,&err);
}


void hmp_flexus_leaveFastMode(Monitor *mon, const QDict *qdict)
{
        Error *err = NULL;
        qmp_flexus_leaveFastMode(&err);
        hmp_handle_error(mon,&err);
}


void hmp_flexus_backupStats(Monitor *mon, const QDict *qdict)
{
        Error *err = NULL;
        const char *filename = qdict_get_str(qdict, "filename");

        qmp_flexus_backupStats(filename, &err);
        hmp_handle_error(mon,&err);
}


void hmp_flexus_saveStats(Monitor *mon, const QDict *qdict)
{
        Error *err = NULL;
        const char *filename = qdict_get_str(qdict, "filename");

        qmp_flexus_saveStats(filename, &err);
        hmp_handle_error(mon,&err);
}


void hmp_flexus_reloadDebugCfg(Monitor *mon, const QDict *qdict)
{
        Error *err = NULL;
        qmp_flexus_reloadDebugCfg(&err);
        hmp_handle_error(mon,&err);
}


void hmp_flexus_addDebugCfg(Monitor *mon, const QDict *qdict)
{
        Error *err = NULL;
        const char *filename = qdict_get_str(qdict, "filename");

        qmp_flexus_addDebugCfg(filename, &err);
        hmp_handle_error(mon,&err);
}


void hmp_flexus_enableCategory(Monitor *mon, const QDict *qdict)
{
        Error *err = NULL;
        const char *component = qdict_get_str(qdict, "component");

        qmp_flexus_enableCategory(component, &err);
        hmp_handle_error(mon,&err);
}


void hmp_flexus_disableCategory(Monitor *mon, const QDict *qdict)
{
        Error *err = NULL;
        const char *component = qdict_get_str(qdict, "component");

        qmp_flexus_disableCategory(component, &err);
        hmp_handle_error(mon,&err);
}


void hmp_flexus_listCategories(Monitor *mon, const QDict *qdict)
{
        Error *err = NULL;
        qmp_flexus_listCategories(&err);
        hmp_handle_error(mon,&err);
}


void hmp_flexus_enableComponent(Monitor *mon, const QDict *qdict)
{
        Error *err = NULL;
        const char *component = qdict_get_str(qdict, "component");
        const char *index = qdict_get_str(qdict, "index");

        qmp_flexus_enableComponent(component, index, &err);
        hmp_handle_error(mon,&err);
}


void hmp_flexus_disableComponent(Monitor *mon, const QDict *qdict)
{
        Error *err = NULL;
        const char *component = qdict_get_str(qdict, "component");
        const char *index = qdict_get_str(qdict, "index");

        qmp_flexus_disableComponent(component, index, &err);
        hmp_handle_error(mon,&err);
}


void hmp_flexus_listComponents(Monitor *mon, const QDict *qdict)
{
        Error *err = NULL;
        qmp_flexus_listComponents(&err);
        hmp_handle_error(mon,&err);
}


void hmp_flexus_printDebugConfiguration(Monitor *mon, const QDict *qdict)
{
        Error *err = NULL;
        qmp_flexus_printDebugConfiguration(&err);
        hmp_handle_error(mon,&err);
}


void hmp_flexus_writeDebugConfiguration(Monitor *mon, const QDict *qdict)
{
        Error *err = NULL;
        qmp_flexus_writeDebugConfiguration(&err);
        hmp_handle_error(mon,&err);
}


void hmp_flexus_log(Monitor *mon, const QDict *qdict)
{
        Error *err = NULL;
        const char *name = qdict_get_str(qdict, "name");
        const char *interval = qdict_get_str(qdict, "interval");
        const char *regex = qdict_get_str(qdict, "regex");

        qmp_flexus_log(name, interval, regex, &err);
        hmp_handle_error(mon,&err);
}


void hmp_flexus_printMMU(Monitor *mon, const QDict *qdict)
{
        Error *err = NULL;
        const char *cpu = qdict_get_str(qdict, "cpu");

        qmp_flexus_printMMU(cpu, &err);
        hmp_handle_error(mon,&err);
}

#ifdef CONFIG_QUANTUM
void hmp_quantum_pause(Monitor *mon, const QDict *qdict)
{
    Error *err = NULL;
    qmp_quantum_pause(&err);
    hmp_handle_error(mon, &err);
}

void hmp_quantum_get(Monitor *mon, const QDict *qdict)
{
    Error *err = NULL;
    QuantumInfo* info;


    info = qmp_quantum_get_all(&err);
    monitor_printf(mon, "Current Quantums are set to: core %lu record: %lu node: %lu:\n", info->quantum_core,
                                                                                          info->quantum_record,
                                                                                          info->quantum_node);

    hmp_handle_error(mon, &err);
}

void hmp_quantum_set(Monitor *mon, const QDict *qdict)
{
    const char*  val = qdict_get_str(qdict, "value");
    const char*  rec = qdict_get_str(qdict, "record");
    const char*  no = qdict_get_str(qdict, "core");

    Error *err = NULL;

    uint64_t v,r,n;
    processForOpts(&v,val,&err);
    processForOpts(&r,rec,&err);
    processForOpts(&n,no,&err);

    set_quantum_value(v);
    set_quantum_record_value(r);
    set_quantum_node_value(n);
}

void hmp_quantum_cpu_dbg(Monitor *mon,  const QDict *qdict)
{
    Error *err = NULL;

    DbgDataAll* dbg = qmp_cpu_dbg(&err);
    for (int i = 0; i < dbg->size; i++)
        monitor_printf(mon, "%lu\n%s", dbg->data[i].instr, dbg->data[i].data);

}

void hmp_quantum_cpu_zero_all(Monitor *mon,  const QDict *qdict)
{
    Error *err = NULL;
    qmp_cpu_zero_all(&err);
    monitor_printf(mon, "Zeroed out CPU instruction count and debug information.");

}
#endif

#ifdef CONFIG_EXTSNAP
void hmp_loadvm_ext(Monitor *mon, const QDict *qdict)
{
    const char *name = qdict_get_str(qdict, "name");

    if (exton == false) {
	monitor_printf(mon, "Error: external snapshot subsystem was disabled\n");
        return;
    }

    if (incremental_load_vmstate_ext(name, mon) < 0) {
	monitor_printf(mon, "Error: can't load the snapshot with args: %s\n", name);
    }
}

void hmp_savevm_ext(Monitor *mon, const QDict *qdict)
{
    const char *name = qdict_get_str(qdict, "name");
    if (exton == false) {
    monitor_printf(mon, "Error: external snapshot subsystem was disabled\n");
        return;
    }
    save_vmstate_ext(mon, name);
}
#endif //EXTSNAP