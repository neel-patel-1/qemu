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
#include "qemu-common.h"
#include "qemu/error-report.h"
#include "qapi/qmp/qerror.h"
#include "qemu/option_int.h"
#include "qemu/main-loop.h"

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
    .skip_interrupts = false
};

int qflex_singlestep(CPUState *cpu) {
    int ret;
    qflex_update_exit_main_loop(false);
    qemu_log("QFlex Singlestep: 0x%016lx\n", QFLEX_GET_ARCH(pc)(cpu));
    ret = qflex_cpu_step(cpu);
    if(ret) {
        // TODO How to handle this case?
        // qemu_log("QFLEX: Singlestep went wrong\n"); 
    }

    return ret;
}

int qflex_adaptative_execution(CPUState *cpu) {
    while(1) {
#ifdef CONFIG_DEVTEROFLEX
        if(devteroflex_is_running()) {
            devteroflex_singlestepping_flow();
        }
        else if (qflexState.singlestep) {
#else 
        if (qflexState.singlestep) {
#endif
            qflex_singlestep(cpu);
        }
        else if(!qflex_is_exit_main_loop()) {
            break;
        } 
    }
    return 0;
}