//  DO-NOT-REMOVE begin-copyright-block
// QFlex consists of several software components that are governed by various
// licensing terms, in addition to software that was developed internally.
// Anyone interested in using QFlex needs to fully understand and abide by the
// licenses governing all the software components.
// 
// ### Software developed externally (not by the QFlex group)
// 
//     * [NS-3] (https://www.gnu.org/copyleft/gpl.html)
//     * [QEMU] (http://wiki.qemu.org/License)
//     * [SimFlex] (http://parsa.epfl.ch/simflex/)
//     * [GNU PTH] (https://www.gnu.org/software/pth/)
// 
// ### Software developed internally (by the QFlex group)
// **QFlex License**
// 
// QFlex
// Copyright (c) 2020, Parallel Systems Architecture Lab, EPFL
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
// 
//     * Redistributions of source code must retain the above copyright notice,
//       this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright notice,
//       this list of conditions and the following disclaimer in the documentation
//       and/or other materials provided with the distribution.
//     * Neither the name of the Parallel Systems Architecture Laboratory, EPFL,
//       nor the names of its contributors may be used to endorse or promote
//       products derived from this software without specific prior written
//       permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE PARALLEL SYSTEMS ARCHITECTURE LABORATORY,
// EPFL BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
// GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//  DO-NOT-REMOVE end-copyright-block
#ifndef QFLEX_H
#define QFLEX_H

#include <stdbool.h>

#include "qemu/osdep.h"
#include "qemu/thread.h"
#include "qemu/queue.h"

#include "qflex/qflex-log.h"

#define QFLEX_EXEC_IN  (0)
#define QFLEX_EXEC_OUT (1)

/** NOTE for cpu_exec (accel/tcg/cpu_exec.c)
  * Depending on the desired type of execution,
  * cpu_exec should break from the double while loop
  * in the correct manner.
  */
typedef enum {
    PROLOGUE,   // Breaks when the Arch State is back to the initial user program
    SINGLESTEP, // Breaks when a single TB (instruction) is executed
    EXECEXCP,   // Breaks when the exeception routine is done
    QEMU        // Normal qemu execution
} QFlexExecType_t;

extern bool qflex_inst_done;
extern bool qflex_prologue_done;
extern uint64_t qflex_prologue_pc;
extern bool qflex_broke_loop;
extern bool qflex_control_with_flexus;
extern bool qflex_trace_enabled;

/** qflex_api_values_init
 * Inits extern flags and vals
 */
void qflex_api_values_init(CPUState *cpu);

/** qflex_prologue
 * When starting from a saved vm state, QEMU first batch of instructions
 * are many nested interrupts.
 * This functions skips this part till QEMU is back into the USER program
 */
int qflex_prologue(CPUState *cpu);
int qflex_singlestep(CPUState *cpu);

/** qflex_cpu_step (cpus.c)
 */
int qflex_cpu_step(CPUState *cpu, QFlexExecType_t type);

/** qflex_cpu_exec (accel/tcg/cpu-exec.c)
 * mirror cpu_exec, with qflex execution flow control
 * for TCG execution. Type defines how the while loop break.
 */
int qflex_cpu_exec(CPUState *cpu, QFlexExecType_t type);
void qflex_init(void);

/* Get and Setters for flags and vars
 *
 */
static inline bool qflex_is_inst_done(void)     { return qflex_inst_done; }
static inline bool qflex_is_prologue_done(void) { return qflex_prologue_done; }
static inline bool qflex_update_prologue_done(uint64_t cur_pc) {
    qflex_prologue_done = ((cur_pc >> 48) == 0);
    return qflex_prologue_done;
}
static inline void qflex_update_inst_done(bool done) { qflex_inst_done = done; }

#if defined(CONFIG_QUANTUM) || defined(CONFIG_FLEXUS) || defined(CONFIG_EXTSNAP)

#ifdef CONFIG_FLEXUS
#include "../../../libqflex/api.h"
#endif

void processForOpts(uint64_t *val, const char* qopt, Error **errp);
void processLetterforExponent(uint64_t *val, char c, Error **errp);
#endif

#ifdef CONFIG_FLEXUS
#include "../../../libqflex/flexus_proxy.h"


typedef enum simulation_mode {
    NO_SIM,
    TRACE,
    TIMING,
    LASTMODE,
} simulation_mode;

typedef struct flexus_state_t {
    simulation_mode mode;
    int nb_cores;
    uint64_t sim_cycles;
    const char* config_file; // user_postload
    const char* dynlib_path;
    const char* load_dir;
    const char* debug_mode;
} flexus_state_t;

extern flexus_state_t flexus_state;

void configure_flexus(QemuOpts *opts, Error **errp);

void set_flexus_snap_dir(const char* dir_name);
const char* flexus_simulation_status(void);
bool hasSimulator(void);
bool flexus_in_simulation(void);
void flexus_qmp(qmp_flexus_cmd_t cmd, const char* args, Error **errp);
void flexus_addDebugCfg(const char *filename, Error **errp);
void flexus_setBreakCPU(const char * value, Error **errp);
void flexus_backupStats(const char *filename, Error **errp);
void flexus_disableCategory(const char *component, Error **errp);
void flexus_disableComponent(const char *component, const char *index, Error **errp);
void flexus_enableCategory(const char *component, Error **errp);
void flexus_enableComponent(const char *component, const char *index, Error **errp);
void flexus_enterFastMode(Error **errp);
void flexus_leaveFastMode(Error **errp);
void flexus_listCategories(Error **errp);
void flexus_listComponents(Error **errp);
void flexus_listMeasurements(Error **errp);
void flexus_log(const char *name, const char *interval, const char *regex, Error **errp);
void flexus_parseConfiguration(const char *filename, Error **errp);
void flexus_printConfiguration(Error **errp);
void flexus_printCycleCount(Error **errp);
void flexus_printDebugConfiguration(Error **errp);
void flexus_printMMU(const char * cpu, Error **errp);
void flexus_printMeasurement(const char *measurement, Error **errp);
void flexus_printProfile(Error **errp);
void flexus_quiesce(Error **errp);
void flexus_reloadDebugCfg(Error **errp);
void flexus_resetProfile(Error **errp);
void flexus_saveStats(const char *filename, Error **errp);
void flexus_setBreakInsn(const char *value, Error **errp);
void flexus_setConfiguration(const char *name, const char *value, Error **errp);
void flexus_setDebug(const char *debugseverity, Error **errp);
void flexus_setProfileInterval(const char *value, Error **errp);
void flexus_setRegionInterval(const char *value, Error **errp);
void flexus_setStatInterval(const char *value, Error **errp);
void flexus_setStopCycle(const char *value, Error **errp);
void flexus_setTimestampInterval(const char *value, Error **errp);
void flexus_status(Error **errp);
void flexus_terminateSimulation(Error **errp);
void flexus_writeConfiguration(const char *filename, Error **errp);
void flexus_writeDebugConfiguration(Error **errp);
void flexus_writeMeasurement(const char *measurement, const char *filename, Error **errp);
void flexus_writeProfile(const char *filename, Error **errp);
int flexus_in_timing(void);
int flexus_in_trace(void);
void flexus_doSave(const char* dir_name, Error **errp);
void flexus_doLoad(const char* dir_name, Error **errp);
#endif

#ifdef CONFIG_EXTSNAP
#include <dirent.h>
extern bool exton;
void configure_extsnap(bool enable);
 
typedef struct phases_state_t {
    uint64_t val;
    int id;
    QLIST_ENTRY(phases_state_t) next;
} phases_state_t;

typedef struct ckpt_state_t {
    uint64_t ckpt_interval;
    uint64_t ckpt_end;
    char* base_snap_name;
    int ckpt_id;
} ckpt_state_t;

extern ckpt_state_t ckpt_state;
extern char* phases_prefix;

int save_vmstate_ext(Monitor *mon, const char *name);
int save_vmstate_ext_test(Monitor *mon, const char *name);
int incremental_load_vmstate_ext(const char *name, Monitor* mon);

void configure_phases(QemuOpts *opts, Error **errp);
void configure_ckpt(QemuOpts *opts, Error **errp);
void extsnap_insert_phases(phases_state_t * head);

int create_tmp_overlay(void);
int delete_tmp_overlay(void);
uint64_t get_phase_value(void);
bool is_phases_enabled(void);
bool is_ckpt_enabled(void);
void toggle_phases_creation(void);
void toggle_ckpt_creation(void);
bool phase_is_valid(void);
void save_phase(void);
void save_ckpt(void);
void pop_phase(void);
bool save_request_pending(void);
bool cont_request_pending(void);
bool quit_request_pending(void);
void request_cont(void);
void request_quit(void);
void toggle_cont_request(void);
void toggle_save_request(void);
void set_base_ckpt_name(const char* str);
const char* get_ckpt_name(void);
uint64_t get_ckpt_interval(void);
uint64_t get_ckpt_end(void);
bool can_quit(void);
void toggle_can_quit(void);
#endif

#ifdef CONFIG_QUANTUM
bool query_quantum_pause_state(void);
void quantum_pause(void);
void quantum_unpause(void);
uint64_t* increment_total_num_instr(void);
uint64_t query_total_num_instr(void);
void set_total_num_instr(uint64_t val);
uint64_t query_quantum_core_value(void);
uint64_t query_quantum_record_value(void);
uint64_t query_quantum_step_value(void);
uint64_t query_quantum_node_value(void);
const char* query_quantum_file_value(void);
void set_quantum_value(uint64_t val);
void set_quantum_record_value(uint64_t val);
void set_quantum_node_value(uint64_t val);
void cpu_dbg(DbgDataAll *info);
void cpu_zero_all(void);
void configure_quantum(QemuOpts *opts, Error **errp);
#endif

#endif /* QFLEX_H */
