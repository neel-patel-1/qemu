#include "qemu/osdep.h"
#include "cpu.h"
#include "exec/helper-proto.h"
#include "exec/log.h"
#include "exec/exec-all.h"

#include "qflex/qflex.h"
#include "qflex-helper.h"
#include "qflex/qflex-helper-a64.h"
#include "qflex/qflex-traces.h"

#ifdef CONFIG_DEVTEROFLEX
#include "qflex/devteroflex/devteroflex.h"
#include "qflex/devteroflex/verification.h"
#include "qflex/devteroflex/demand-paging.h"
#endif /* CONFIG_DEVTEROFLEX */


/* TCG helper functions. (See exec/helper-proto.h  and target/arch/helper-a64.h)
 * This one expands prototypes for the helper functions.
 * They get executed in the TB
 * To use them: in translate.c or translate-a64.c
 * ex: HELPER(qflex_func)(arg1, arg2, ..., argn)
 * gen_helper_qflex_func(arg1, arg2, ..., argn)
 */

/**
 * @brief HELPER(qflex_magic_inst)
 * In ARM, hint instruction (which is like a NOP) comes with an int with range 0-127
 * Big part of this range is defined as a normal NOP.
 * Too see which indexes are already used ref (curently 39-127 is free) :
 * https://developer.arm.com/docs/ddi0596/a/a64-base-instructions-alphabetic-order/hint-hint-instruction
 *
 * This function is called when a HINT n (90 < n < 127) TB is executed
 * nop_op: in HINT n, it's the selected n.
 *
 * To do more complex calls from QEMU guest to host
 * You can chain two nops
 *
 */
#include "qflex/qflex-qemu-calls.h"
static uint64_t prev_nop_op = 0;

static inline void qflex_mem_trace_cmds(CPUState *cs, uint64_t nop_op) {
    switch(nop_op) {
        case MEM_TRACE_START:   qflex_mem_trace_start(-1, QFLEX_GEN_TRACE_MEDIUM); break;
        case MEM_TRACE_STOP:    qflex_mem_trace_stop();       break;
        case MEM_TRACE_START_INST: qflex_mem_trace_start(QFLEX_GET_ARCH(reg)(cs, 0), QFLEX_GEN_TRACE_TINY_INST); break;
        case MEM_TRACE_RESULTS: qflex_mem_trace_log_direct(); break;
    }
}

static inline void qflex_cmds(CPUState *cs, uint64_t nop_op) {
    switch(nop_op) {
        case QFLEX_SINGLESTEP_START: qflex_singlestep_start(); break;
        case QFLEX_SINGLESTEP_STOP: qflex_singlestep_stop(); break;
        case QFLEX_PRINT_ASID_TID:  qflex_print_state_asid_tid(cs); break;
        default: break;
    }
}

#ifdef CONFIG_DEVTEROFLEX
static inline void devteroflex_cmds(CPUState *cs, uint64_t nop_op) {
    switch(nop_op) {
        case DEVTEROFLEX_FLOW_START: 
            devteroflex_start(); 
            qemu_loglevel |= CPU_LOG_TB_IN_ASM;
            qemu_loglevel |= CPU_LOG_INT;
            break;
        case DEVTEROFLEX_FLOW_STOP: 
            devteroflex_stop(); 
            qemu_loglevel &= ~CPU_LOG_TB_IN_ASM;
            qemu_loglevel &= ~CPU_LOG_INT;
            break;
        default: break;
    }
}
#endif

void HELPER(qflex_magic_inst)(CPUARMState *env, uint64_t nop_op) {
    CPUState *cs = env_cpu(env);
    assert(nop_op >= 90);
    assert(nop_op <= 127);
    qflex_log_mask(QFLEX_LOG_MAGIC_INST, "MAGIC_INST:%"PRIu64"\n", nop_op);

    // CPUState *cs = CPU(env_archcpu(env));

    // Make chained nop_op event
    switch(prev_nop_op) {

        case QFLEX_OP:
            qflex_cmds(cs, nop_op);
            prev_nop_op = 0;
            return; // HELPER EXIT

        case MEM_TRACE_OP: 
            qflex_mem_trace_cmds(cs, nop_op);
            prev_nop_op = 0;
            return; // HELPER EXIT

#ifdef CONFIG_DEVTEROFLEX
        case DEVTEROFLEX_OP:
            devteroflex_cmds(cs, nop_op);
            prev_nop_op = 0;
            return; // HELPER EXIT
#endif
        default: break;
    }

    // Get chained nop_op op type
    switch(nop_op) {
 #ifdef CONFIG_DEVTEROFLEX
        case DEVTEROFLEX_OP:
 #endif
        case QFLEX_OP:
        case MEM_TRACE_OP: 
            prev_nop_op = nop_op; 
            break;
        default: prev_nop_op = 0; break;
    }
}

/**
 * @brief HELPER(qflex_mem_trace)
 * Helper gets executed before a LD/ST
 */
void HELPER(qflex_mem_trace)(CPUARMState* env, uint64_t addr, uint64_t type) {
    CPUState *cs = CPU(env_archcpu(env));
    qflex_log_mask(QFLEX_LOG_LDST, "[MEM]CPU%u:%"PRIu64":0x%016"PRIx64"\n", cs->cpu_index, type, addr);
    
    int inst;
    if(qflex_mem_trace_gen_trace()) {
        uint64_t paddr = gva_to_hva(cs, addr, type);
        if(paddr != -1)  {
            qflex_mem_trace_memaccess(addr, paddr, cs->cpu_index, type, arm_current_el(env));
            if(type == MMU_INST_FETCH) {
                inst = *(uint32_t *) paddr;
                QflexInstTraceFull_t trace = {
                    .cpu_index = cs->cpu_index, 
                    .asid = QFLEX_GET_ARCH(asid)(cs), 
                    .tid = QFLEX_GET_ARCH(tid)(cs), 
                    .pc = addr, 
                    .inst = inst
                };
                QflexInstTraceMedium_t traceMed = {
                    .asid = QFLEX_GET_ARCH(asid)(cs),
                    .el = QFLEX_GET_ARCH(el)(cs),
                    .tid = env->cp15.tpidrurw_ns,
                    .tid2 = env->cp15.tpidrurw_ns
                };
                switch(qflexTraceState.gen_inst_trace_type) {
                    case QFLEX_GEN_TRACE_TINY_INST:     qflex_inst_trace(cs->cpu_index, QFLEX_GET_ARCH(asid)(cs), inst); break;
                    case QFLEX_GEN_TRACE_MEDIUM:      qflex_inst_trace_medium(cs->cpu_index, &traceMed); break;
                    case QFLEX_GEN_TRACE_FULL_INST:  qflex_inst_trace_full(&trace);  break;
                }
            }
        }
    }

#ifdef CONFIG_DEVTEROFLEX
    if(devteroflex_is_enabled() && !devteroflexConfig.is_debug) {
        // In debug mode, we only sync page after the operation is done.
        if(type != MMU_INST_FETCH) {
            devteroflex_synchronize_page(cs, addr, type);
        }
    }

    if(gen_verification()) {
        if(type == MMU_INST_FETCH) {
            gen_verification_add_state(cs, addr);
        } else {
            gen_verification_add_mem(cs, addr);
        }
    }
#endif
}

/**
 * @brief the helper executed after a memory operation is done.
 */
void HELPER(qflex_post_mem)(CPUARMState* env, uint64_t addr, uint32_t type, uint32_t size, uint32_t is_last_operation){
#ifdef CONFIG_DEVTEROFLEX
    if(devteroflex_is_enabled() && devteroflexConfig.is_debug) {
        if(type == MMU_DATA_STORE) {
            // here the page checking will be done.
            // it will evicted related pages.
            if(is_last_operation) {
                // always check the last operation.
                devteroflex_synchronize_page(CPU(env_archcpu(env)), addr, type);
            } else {
                // is the element the last one in the page? If so, we will check it.
                if((addr & 0xFFF) == (4096 - size)) {
                    devteroflex_synchronize_page(CPU(env_archcpu(env)), addr, type);
                }
            }
        }
    }
#endif
}

/**
 * @brief HELPER(qflex_executed_instruction)
 * location: location of the gen_helper_ in the transalation.
 *           EXEC_IN : Started executing a TB
 */
void HELPER(qflex_executed_instruction)(CPUARMState* env, uint64_t pc, int location) {
    CPUState *cs = CPU(env_archcpu(env));

    switch(location) {
        case QFLEX_EXEC_IN:
            if(unlikely(qflex_loglevel_mask(QFLEX_LOG_TB_EXEC))) {
                FILE* logfile = qemu_log_lock();
                qemu_log("IN[%d]  :", cs->cpu_index);
                log_target_disas(cs, pc, 4);
                qemu_log_unlock(logfile);
            }
            qflex_update_inst_done(true);
            break;
        default: break;
    }
}

/**
 * @brief HELPER(qflex_exception_return)
 * This helper gets called after a ERET TB execution is done.
 * The env passed as argument has already changed EL and jumped to the ELR.
 * For the moment not needed.
 */
void HELPER(qflex_exception_return)(CPUARMState *env) { return; }
