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
#include "qflex/qflex.h"
#include "../libqflex/api.h"
#include "qemu/queue.h"
#include "qapi/error.h"

#define COPY_EXCP_HALTED 0x10003

bool qflex_inst_done = false;
bool qflex_prologue_done = false;
uint64_t qflex_prologue_pc = 0xDEADBEEF;
bool qflex_control_with_flexus = false;
bool qflex_trace_enabled = false;

#ifdef CONFIG_FLEXUS

flexus_state_t flexus_state;

void qflex_api_values_init(CPUState *cpu) {
    qflex_inst_done = false;
    qflex_prologue_done = false;
    qflex_prologue_pc = cpu_get_program_counter(cpu);
}

int qflex_prologue(CPUState *cpu) {
    int ret = 0;
    qflex_api_values_init(cpu);
    qflex_log_mask(QFLEX_LOG_GENERAL, "QFLEX: PROLOGUE START:%08lx\n"
                   "    -> Skips initial snapshot load long interrupt routine to normal user program\n", cpu_get_program_counter(cpu));
    while(!qflex_is_prologue_done()) {
        ret = qflex_cpu_step(cpu, PROLOGUE);
    }
    qflex_log_mask(QFLEX_LOG_GENERAL, "QFLEX: PROLOGUE END  :%08lx\n", cpu_get_program_counter(cpu));
    return ret;
}

int qflex_singlestep(CPUState *cpu) {
    int ret = 0;
    while(!qflex_is_inst_done() && (ret != COPY_EXCP_HALTED)) {
        ret = qflex_cpu_step(cpu, SINGLESTEP);
    }
    qflex_update_inst_done(false);
    return ret;
}

int advance_qemu(void * obj){
    CPUState *cpu = obj;
    return qflex_singlestep(cpu);
}


#ifdef CONFIG_FLEXUS
#include "../../../libqflex/flexus_proxy.h"
#include "../../../libqflex/api.h"
static const char* simulation_mode_strings[] = {
    "NONE",
    "TRACE",
    "TIMING",
    "LASTMODE"
};

int flexus_in_timing(void){ return flexus_state.mode == TIMING; }
int flexus_in_trace(void) { return flexus_state.mode == TRACE; }
bool flexus_in_simulation(void){ return flexus_in_timing() | flexus_in_trace(); }

const char* flexus_simulation_status(void){
    return simulation_mode_strings[flexus_state.mode];
}

void flexus_addDebugCfg(const char *filename, Error **errp){
    flexus_qmp(QMP_FLEXUS_ADDDEBUGCFG, filename, errp);
}
void flexus_setBreakCPU(const char * value, Error **errp){
    flexus_qmp(QMP_FLEXUS_SETBREAKCPU, value, errp);
}
void flexus_backupStats(const char *filename, Error **errp){
    flexus_qmp(QMP_FLEXUS_BACKUPSTATS, filename, errp);
}
void flexus_disableCategory(const char *component, Error **errp){
    flexus_qmp(QMP_FLEXUS_DISABLECATEGORY, component, errp);
}
void flexus_disableComponent(const char *component, const char *index, Error **errp){
    char* args = malloc((strlen(component)+strlen(index)*sizeof(char)));
    sprintf(args, "%s:%s",component, index);
    flexus_qmp(QMP_FLEXUS_DISABLECOMPONENT, args, errp);
}
void flexus_enableCategory(const char *component, Error **errp){
    flexus_qmp(QMP_FLEXUS_ENABLECATEGORY, component, errp);
}
void flexus_enableComponent(const char *component, const char *index, Error **errp){
    char* args = malloc((strlen(component)+strlen(index)*sizeof(char)));
    sprintf(args, "%s:%s",component, index);
    flexus_qmp(QMP_FLEXUS_ENABLECOMPONENT, args, errp);
}
void flexus_enterFastMode(Error **errp){
    flexus_qmp(QMP_FLEXUS_ENTERFASTMODE, NULL, errp);
}
void flexus_leaveFastMode(Error **errp){
    flexus_qmp(QMP_FLEXUS_LEAVEFASTMODE, NULL, errp);
}
void flexus_listCategories(Error **errp){
    flexus_qmp(QMP_FLEXUS_LISTCATEGORIES, NULL, errp);
}
void flexus_listComponents(Error **errp){
    flexus_qmp(QMP_FLEXUS_LISTCOMPONENTS, NULL, errp);
}
void flexus_listMeasurements(Error **errp){
    flexus_qmp(QMP_FLEXUS_LISTMEASUREMENTS, NULL, errp);
}
void flexus_log(const char *name, const char *interval, const char *regex, Error **errp){
    char* args = malloc((strlen(name)+strlen(interval)+strlen(regex))*sizeof(char));
    sprintf(args, "%s:%s:%s", name, interval, regex);
    flexus_qmp(QMP_FLEXUS_LOG, args, errp);
}
void flexus_parseConfiguration(const char *filename, Error **errp){
    flexus_qmp(QMP_FLEXUS_PARSECONFIGURATION, filename, errp);
}
void flexus_printConfiguration(Error **errp){
    flexus_qmp(QMP_FLEXUS_PRINTCONFIGURATION, NULL, errp);
}
void flexus_printCycleCount(Error **errp){
    flexus_qmp(QMP_FLEXUS_PRINTCYCLECOUNT, NULL, errp);
}
void flexus_printDebugConfiguration(Error **errp){
    flexus_qmp(QMP_FLEXUS_PRINTDEBUGCONFIGURATION, NULL, errp);
}
void flexus_printMMU(const char* cpu, Error **errp){
    flexus_qmp(QMP_FLEXUS_PRINTMMU, cpu, errp);
}
void flexus_printMeasurement(const char *measurement, Error **errp){
    flexus_qmp(QMP_FLEXUS_PRINTMEASUREMENT, measurement, errp);
}
void flexus_printProfile(Error **errp){
    flexus_qmp(QMP_FLEXUS_PRINTPROFILE, NULL, errp);
}
void flexus_quiesce(Error **errp){
    flexus_qmp(QMP_FLEXUS_QUIESCE, NULL, errp);
}
void flexus_reloadDebugCfg(Error **errp){
    flexus_qmp(QMP_FLEXUS_RELOADDEBUGCFG, NULL, errp);
}
void flexus_resetProfile(Error **errp){
    flexus_qmp(QMP_FLEXUS_RESETPROFILE, NULL, errp);
}
void flexus_saveStats(const char *filename, Error **errp){
    flexus_qmp(QMP_FLEXUS_SAVESTATS, filename, errp);
}
void flexus_setBreakInsn(const char *value, Error **errp){
    flexus_qmp(QMP_FLEXUS_SETBREAKCPU, value, errp);
}
void flexus_setConfiguration(const char *name, const char *value, Error **errp){
    char* args = malloc((strlen(name)+strlen(value))*sizeof(char));
    sprintf(args, "%s:%s",name, value);
    flexus_qmp(QMP_FLEXUS_SETCONFIGURATION, args, errp);
}
void flexus_setDebug(const char *debugseverity, Error **errp){
    flexus_qmp(QMP_FLEXUS_SETDEBUG, debugseverity, errp);
}
void flexus_setProfileInterval(const char *value, Error **errp){
    flexus_qmp(QMP_FLEXUS_SETPROFILEINTERVAL, value, errp);
}
void flexus_setRegionInterval(const char *value, Error **errp){
    flexus_qmp(QMP_FLEXUS_SETREGIONINTERVAL, value, errp);
}
void flexus_setStatInterval(const char *value, Error **errp){
    flexus_qmp(QMP_FLEXUS_SETSTATINTERVAL, value, errp);
}
void flexus_setStopCycle(const char *value, Error **errp){
    flexus_qmp(QMP_FLEXUS_SETSTOPCYCLE, value, errp);
}
void flexus_setTimestampInterval(const char *value, Error **errp){
    flexus_qmp(QMP_FLEXUS_SETTIMESTAMPINTERVAL, value, errp);
}
void flexus_terminateSimulation(Error **errp){
    flexus_qmp(QMP_FLEXUS_TERMINATESIMULATION, NULL, errp);
}
void flexus_writeConfiguration(const char *filename, Error **errp){
    flexus_qmp(QMP_FLEXUS_WRITECONFIGURATION, filename, errp);
}
void flexus_writeDebugConfiguration(Error **errp){
    flexus_qmp(QMP_FLEXUS_WRITEDEBUGCONFIGURATION, NULL, errp);
}
void flexus_writeMeasurement(const char *measurement, const char *filename, Error **errp){
    char* args = malloc((strlen(measurement)+strlen(filename))*sizeof(char));
    sprintf(args, "%s:%s",measurement, filename);
    flexus_qmp(QMP_FLEXUS_WRITEMEASUREMENT, args, errp);
}
void flexus_writeProfile(const char *filename, Error **errp){
    flexus_qmp(QMP_FLEXUS_WRITEPROFILE, filename, errp);
}
void flexus_doSave(const char *dir_name, Error **errp){
    flexus_qmp(QMP_FLEXUS_DOSAVE, dir_name, errp);

}
void flexus_doLoad(const char *dir_name, Error **errp){
    int file_count = 0;
    DIR * dirp;
    struct dirent * entry;

    dirp = opendir(dir_name);
    while ((entry = readdir(dirp)) != NULL) {
        if (entry->d_type == DT_REG) { /* If the entry is a regular file */
             file_count++;
        }
    }
    closedir(dirp);

    if (file_count > 2) // might not be best
        flexus_qmp(QMP_FLEXUS_DOLOAD, dir_name, errp);
}

#endif /* CONFIG_FLEXUS */

#ifdef CONFIG_QUANTUM
typedef struct {
    uint64_t quantum_value, quantum_record_value, quantum_node_value,quantum_step_value;
    char* quantum_file_value;
    uint64_t total_num_instructions, last_num_instruction;
    bool quantum_pause;
} quantum_state_t;

static quantum_state_t quantum_state;

bool query_quantum_pause_state(void) { return quantum_state.quantum_pause; }
void quantum_pause(void)    { quantum_state.quantum_pause = true; }
void quantum_unpause(void)  { quantum_state.quantum_pause = false; qmp_cont(NULL); }
uint64_t* increment_total_num_instr(void) {
    quantum_state.total_num_instructions++;
    return &(quantum_state.total_num_instructions);
}
uint64_t query_total_num_instr(void)        { return quantum_state.total_num_instructions; }
uint64_t query_quantum_core_value(void)     { return quantum_state.quantum_value; }
uint64_t query_quantum_record_value(void)   { return quantum_state.quantum_record_value; }
uint64_t query_quantum_step_value(void)     { return quantum_state.quantum_step_value; }
uint64_t query_quantum_node_value(void)     { return quantum_state.quantum_node_value; }
const char* query_quantum_file_value(void)  { return quantum_state.quantum_file_value; }
void set_total_num_instr(uint64_t val)      { quantum_state.total_num_instructions = val; }
void set_quantum_value(uint64_t val)        { quantum_state.quantum_value = val; }
void set_quantum_record_value(uint64_t val) { quantum_state.quantum_record_value = val; }
void set_quantum_node_value(uint64_t val)   { quantum_state.quantum_node_value = val; }
#endif /* CONFIG_QUANTUM */

#ifdef CONFIG_EXTSNAP


ckpt_state_t ckpt_state;
static bool using_phases;
static bool using_ckpt;
static char* snap_name;
static QLIST_HEAD(, phases_state_t) phases_head = QLIST_HEAD_INITIALIZER(phases_head);
static bool cont_requested, save_requested, quit_requested;

void extsnap_insert_phases(phases_state_t * head) { QLIST_INSERT_HEAD(&phases_head, head, next); }
static const char* get_phases_prefix(void) { return phases_prefix; }
static const char* get_base_ckpt_name(void) { return ckpt_state.base_snap_name; }
uint64_t get_ckpt_interval(void){ return ckpt_state.ckpt_interval; }
uint64_t get_ckpt_end(void)     { return ckpt_state.ckpt_end; }
const char* get_ckpt_name(void) { return snap_name; }
bool phase_is_valid(void)   { return (QLIST_EMPTY(&phases_head) ? false : true); }
bool is_phases_enabled(void){ return using_phases; }
bool is_ckpt_enabled(void)  { return using_ckpt; }
void toggle_phases_creation(void) { using_phases = !using_phases; }
void toggle_ckpt_creation(void)   { using_ckpt   = !using_ckpt; }
void toggle_save_request(void)  { save_requested = !save_requested; }
void toggle_cont_request(void)  { cont_requested = !cont_requested; }
void request_cont(void) { cont_requested = true; }
void request_quit(void) { quit_requested = true; }
bool save_request_pending(void) { return save_requested; }
bool cont_request_pending(void) { return cont_requested; }
bool quit_request_pending(void) { return quit_requested; }

static int get_phase_id(void)
{
    if (QLIST_EMPTY(&phases_head)) assert(false);
    phases_state_t *p = QLIST_FIRST(&phases_head);
    return p->id;
}
uint64_t get_phase_value(void){
    if (QLIST_EMPTY(&phases_head)) assert(false);
    phases_state_t *p = QLIST_FIRST(&phases_head);
    return p->val;
}
void set_base_ckpt_name(const char* str){
    if (strcmp(str,"")==0) {
        ckpt_state.base_snap_name = (char*)malloc(6*sizeof(char));
        for (int i =0; i<6;++i) {
            char randomletter = 'A' + (random() % 26);
            ckpt_state.base_snap_name[i] = randomletter;
        }
    } else {
        ckpt_state.base_snap_name = strdup(str);
    }
}
static inline void request_save(const char*str) {
    snap_name = strdup(str);
    save_requested = true;
}
void save_ckpt(void) {
    char* name = (char*)malloc(strlen(get_base_ckpt_name()) + 10*sizeof(char));
    sprintf(name, "%s_ckpt_%03d", get_base_ckpt_name(), ckpt_state.ckpt_id++);
    request_save(name);
}

void save_phase(void) {
    char* name = (char*)malloc(strlen(get_phases_prefix()) + 5*sizeof(char));
    sprintf(name, "%s_%03d", get_phases_prefix(), get_phase_id());
    request_save(name);
}

void pop_phase(void) {
    if (QLIST_EMPTY(&phases_head)) assert(false);
    phases_state_t *p = QLIST_FIRST(&phases_head);
    QLIST_REMOVE(p, next);
}

#endif /* CONFIG_EXTSNAP */

#endif // CONFIG_FLEXUS

