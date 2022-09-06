#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

#include "qemu/osdep.h"
#include "qemu/thread.h"

#include "qapi/error.h"
#include "qemu/error-report.h"
#include "qapi/qmp/qerror.h"
#include "qemu/option_int.h"
#include "qemu/main-loop.h"

#include "exec/log.h"
#include "qflex/qflex.h"
#include "qflex/qflex-traces.h"

#ifdef CONFIG_DEVTEROFLEX
#include "qflex/devteroflex/devteroflex.h"
#endif

QflexState_t qflexState = {
    .inst_done = false,
    .broke_loop = false,
    .singlestep = false,
    .exit_main_loop = false,
    .skip_interrupts = false,
    .log_inst = false
};



static int qflex_singlestep_with_retry(CPUState *cpu, bool retry) {
    int ret = 0;
    uint64_t pc_ss = QFLEX_GET_ARCH(pc)(cpu);
    uint32_t asid = QFLEX_GET_ARCH(asid)(cpu);
    uint64_t pc_ss_after;

    if(qflexState.log_inst) {
        qemu_log("CPU[%i]:ASID[%x]:", cpu->cpu_index, asid); 
        QFLEX_GET_ARCH(log_inst)(cpu);
    }

    ret = qflex_cpu_step(cpu);

    pc_ss_after = QFLEX_GET_ARCH(pc)(cpu);
    if(pc_ss == pc_ss_after) {
        if(retry) {
            printf("QFlex singlestep went wrong twice in a row: ret = %x\n", ret);
            qemu_log("QFlex singlestep went wrong twice in a row: ret = %x\n", ret);
            assert(false);
        } else {
            printf("QFlex singlestep went wrong, did not advance a step: ret = %x\n ---- Retrying\n", ret);
            qemu_log("QFlex singlestep went wrong, did not advance a step: ret = %x\n ---- Retrying\n", ret);
            ret = qflex_singlestep_with_retry(cpu, true);
        }
    }

    return ret;
}

int qflex_singlestep(CPUState *cpu) {
    return qflex_singlestep_with_retry(cpu, false);
}

int qflex_adaptative_execution(CPUState *cpu) {
    qflexState.exit_main_loop = false;
    while(1) {
#ifdef CONFIG_DEVTEROFLEX
        if(devteroflexConfig.enabled && devteroflexConfig.running) {
            devteroflex_singlestepping_flow();
        }
        else if (qflexState.singlestep) {
#else 
        if (qflexState.singlestep) {
#endif
            qflex_singlestep(cpu);
        }
        else if(!qflexState.exit_main_loop) {
            break;
        } 
    }
    return 0;
}