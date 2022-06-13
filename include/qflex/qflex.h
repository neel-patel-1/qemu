#ifndef QFLEX_H
#define QFLEX_H

#include <stdbool.h>

#include "qemu/osdep.h"
#include "qemu/thread.h"

#include "qflex/qflex-log.h"
#include "qflex/qflex-arch.h"

#define QFLEX(name)       glue(qflex_, name)

typedef struct QflexState_t {
    bool singlestep;
    bool inst_done;
    bool broke_loop;
    bool exit_main_loop;
    bool skip_interrupts;
    bool log_inst;
} QflexState_t;

extern QflexState_t qflexState;

/** qflex_singlestep
 * This functions executes a single instruction (as TB) before returning.
 * It sets the broke_loop flag if it broke the main execution loop (in cpu-exec.c)
 */
int qflex_singlestep(CPUState *cpu);

/** qflex_adaptative_execution
 * This executes normal qemu till a different mode flag is set.
 */
int qflex_adaptative_execution(CPUState *cpu);

/** qflex_cpu_step (cpus.c)
 */
int qflex_cpu_step(void *arg);

/* Located in cpu-exec.c
 * Because QFLEX relies sometimes on added TB instrumentation
 * Flush already decoded TBs such that the extra TCG insts are generated
 */
void qflex_tb_flush(void);
#endif /* QFLEX_H */
