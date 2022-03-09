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

/* Get and Setters for state flags and vars
 */
static inline QflexState_t qflex_get_state(void)  { return qflexState; }
static inline bool qflex_is_inst_done(void)       { return qflexState.inst_done; }
static inline bool qflex_is_broke_loop(void)      { return qflexState.broke_loop; }
static inline bool qflex_is_exit_main_loop(void)  { return qflexState.exit_main_loop; }
static inline bool qflex_is_skip_interrupts(void)  { return qflexState.skip_interrupts; }

static inline void qflex_update_inst_done(bool done) {
    qflexState.inst_done = done; }
static inline void qflex_update_broke_loop(bool broke) {
    qflexState.broke_loop = broke; }
static inline void qflex_update_exit_main_loop(bool exit) { 
    qflexState.exit_main_loop = exit; }
static inline void qflex_update_skip_interrupts(bool skip) { 
    qflexState.skip_interrupts = skip; }

static inline bool qflex_is_singlestep(void) { return qflexState.singlestep; }
static inline void qflex_singlestep_start(void) {  qflexState.singlestep = true; }
static inline void qflex_singlestep_stop(void) {  qflexState.singlestep = false; }


/* Located in cpu-exec.c
 * Because QFLEX relies sometimes on added TB instrumentation
 * Flush already decoded TBs such that the extra TCG insts are generated
 */
void qflex_tb_flush(void);
#endif /* QFLEX_H */
