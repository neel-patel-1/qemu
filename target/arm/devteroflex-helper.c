#include "qemu/osdep.h"
#include "cpu.h"
#include "exec/helper-proto.h"

#include "qflex/qflex-arch.h"
#include "qflex/devteroflex/devteroflex.h"

/** 
 * NOTE: Functions that need to access architecture specific state (e.g. 
 *       registers) should be located here. You can find in the structure 
 *       CPUARMState all state relevant.
 */

void devteroflex_pack_archstate(DevteroflexArchState *devteroflex, CPUState *cpu ) {
    CPUARMState *env = cpu->env_ptr;

    memcpy(&devteroflex->xregs,     &env->xregs, 32*sizeof(uint64_t));
    devteroflex->pc = env->pc;
    devteroflex->sp = 0;

    uint64_t nzcv =
        ((env->CF)           ? 1 << ARCH_PSTATE_CF_MASK : 0) |
        ((env->VF & (1<<31)) ? 1 << ARCH_PSTATE_VF_MASK : 0) |
        ((env->NF & (1<<31)) ? 1 << ARCH_PSTATE_NF_MASK : 0) |
        (!(env->ZF)          ? 1 << ARCH_PSTATE_ZF_MASK : 0);
    devteroflex->flags = nzcv;
}

void devteroflex_unpack_archstate(CPUState *cpu, DevteroflexArchState *devteroflex) {
    CPUARMState *env = cpu->env_ptr;

    memcpy(&env->xregs, &devteroflex->xregs, 32*sizeof(uint64_t));
    env->pc = devteroflex->pc;
    env->sp_el[QFLEX_GET_ARCH(el)(cpu)] = devteroflex->sp;

    uint32_t nzcv = devteroflex->flags;
    env->CF = (nzcv & ARCH_PSTATE_CF_MASK) ? 1 : 0;
    env->VF = (nzcv & ARCH_PSTATE_VF_MASK) ? (1 << 31) : 0;
    env->NF = (nzcv & ARCH_PSTATE_NF_MASK) ? (1 << 31) : 0;
    env->ZF = !(nzcv & ARCH_PSTATE_ZF_MASK) ? 1 : 0;
}

bool devteroflex_compare_archstate(const CPUState *cpu, DevteroflexArchState *devteroflex) {
    const CPUARMState *env = cpu->env_ptr;

    for(int i = 0; i < 32; ++i) {
        if(env->xregs[i] != devteroflex->xregs[i]) {
            qemu_log(
                "Mismatched register %d is detected. \n QEMU: %lx, FPGA: %lx \n", 
                i, 
                env->xregs[i],
                devteroflex->xregs[i]
            );
            return true;
        }
    }

    if(env->pc != devteroflex->pc) {
        qemu_log(
            "Mismatched register PC is detected. \n QEMU: %lx, FPGA: %lx \n", 
            env->pc,
            devteroflex->pc
        );
        return true;
    }
    // we didn't compare the NZCV now.

    return false;
}

/** devteroflex_example_instrumentation
 *  This function is an inserted callback to be executed on an instruction execution.
 *  Instrumenting instruction execution in such fashion slowdowns significantly QEMU
 *  emulation speed.
 */
#include "qflex/devteroflex/custom-instrumentation.h"
void HELPER(devteroflex_example_instrumentation)(CPUARMState *env, uint64_t arg1, uint64_t arg2)
{
    CPUState *cs = CPU(env_archcpu(env));
    // Here you can insert any function callback
    devteroflex_example_callback(cs->cpu_index, arg1, arg2);
}
