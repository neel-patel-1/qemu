#include "sysemu/tcg.h"

#include "qemu/osdep.h"
#include <dirent.h>
#include "monitor/monitor-internal.h"
#include "qapi/error.h"
#include "qapi/qmp/qdict.h"
#include "qapi/qmp/qnum.h"
#include "hw/core/cpu.h"

#include "qflex/qflex-hmp.h"
#include "qflex/qflex.h"
#include "qflex/qflex-traces.h"

void hmp_qflex_log(Monitor *mon, const QDict *qdict)
{
    int mask;
    const char *items = qdict_get_str(qdict, "items");

    if (!strcmp(items, "none")) {
        mask = 0;
    } else {
        mask = qflex_str_to_log_mask(items);
        if (!mask) {
            monitor_printf(mon, "None of your logging arguments matched.");
            return;
        }
    }
    qflex_set_log(mask);
}

void hmp_qflex_singlestep_start(Monitor *mon, const QDict *qdict) {
    int error = 0;
    if(!tcg_enabled()) {
       monitor_printf(mon, "`singlestep` available only with TCG.");
       error |= 1;
    }
    if(!qemu_loglevel_mask(CPU_LOG_TB_NOCHAIN)) {
        monitor_printf(mon, "`singlestep` available only with `-d nochain`.");
        error |= 1;
    }
    CPUState *cpu;
    CPU_FOREACH(cpu) {
        if(cpu->singlestep_enabled) {
            monitor_printf(mon, "`singlestep` available only with `-singlestep`.");
            error |= 1;
        }
    }
    if(error) return;

    qflexState.singlestep = true;
}

void hmp_qflex_singlestep_stop(Monitor *mon, const QDict *qdict) {
    if(!qflexState.singlestep) {
       monitor_printf(mon, "singlestepping was not running");
    }
    qflexState.singlestep = false;
}

void hmp_qflex_mem_trace_start(Monitor *mon, const QDict *qdict) {
    size_t nb_insn = qdict_get_int(qdict, "nb_insn");
    int trace_type = qdict_get_int(qdict, "trace_type");
    if(!qflexTraceState.fileInit) {
        monitor_printf(mon, "Trace buffer files are not opened.\n"
                            "Please add `-qflex-gen-mem-trace core_count=YOUR_NB_OF_CORES`"
                            "argument to qemu before launching.\n");
        return;
    }
    qflex_mem_trace_start(nb_insn, trace_type);
}

void hmp_qflex_mem_trace_stop(Monitor *mon, const QDict *qdict) {
    qflex_mem_trace_stop();
}

void hmp_qflex_mem_trace_end(Monitor *mon, const QDict *qdict) {
    qflex_mem_trace_end();
}

void hmp_qflex_mem_trace_log_stats(Monitor *mon, const QDict *qdict) {
    char log[256];
    qflex_mem_trace_log_stats(log, 256);
    monitor_printf(mon, "%s", log);
}
