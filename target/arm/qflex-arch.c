#include "qemu/osdep.h"
#include "cpu.h"
#include "internals.h"

#include "qflex-helper.h"
#include "qflex/qflex.h"

/* qflex/qflex-arch.h
 */
#define ENV(cpu) ((CPUARMState *) cpu->env_ptr)

uint64_t QFLEX_GET_ARCH(pc)(CPUState *cs) { return ENV(cs)->pc; }
int      QFLEX_GET_ARCH(el)(CPUState *cs) { return arm_current_el(ENV(cs)); }

/**
 * NOTE: Layout ttbrN register: (src: https://developer.arm.com/docs/ddi0595/h/aarch64-system-registers/ttbr0_el1)
 * ASID:  bits [63:48]
 * BADDR: bits [47:1]
 * CnP:   bit  [0]
 */
uint64_t QFLEX_GET_ARCH(asid)(CPUState *cs) {
    if (true /* TODO: when do we need ttbr1 ? */) {
        return ENV(cs)->cp15.ttbr0_ns >> 48;
    } else {
        return ENV(cs)->cp15.ttbr1_ns >> 48;
    }
}

uint64_t QFLEX_GET_ARCH(asid_reg)(CPUState *cs) {
    if (true /* TODO: when do we need ttbr1 ? */) {
        return ENV(cs)->cp15.ttbr0_ns;
    } else {
        return ENV(cs)->cp15.ttbr1_ns;
    }
}

uint64_t QFLEX_GET_ARCH(tid)(CPUState *cs) {
    int curr_el = arm_current_el(ENV(cs));
    if(curr_el == 0) {
        return ENV(cs)->cp15.tpidrurw_ns;
    } else {
        return ENV(cs)->cp15.tpidrprw_ns;
    }
}

int QFLEX_GET_ARCH(reg)(CPUState *cs, int reg_index) {
    assert(reg_index < 32);
    return ENV(cs)->xregs[reg_index];
}

uint64_t gva_to_hva(CPUState *cs, uint64_t addr, int access_type) {
    return gva_to_hva_arch(cs, addr, (MMUAccessType) access_type);
}

uint64_t gva_to_hva_with_asid(uint64_t asid_reg, uint64_t vaddr, int access_type) {
    CPUState *cs = first_cpu;
    return gva_to_hva_arch_with_asid(cs, vaddr, (MMUAccessType) access_type, asid_reg);
}

void qflex_print_state_asid_tid(CPUState* cs) {
    CPUARMState *env = cs->env_ptr;
    qemu_log("[MEM]CPU%" PRIu32 "\n"
             "  TTBR0:0[0x%04" PRIx64 "]:1[0x%4" PRIx64 "]:2[0x%4" PRIx64 "]:3[0x%4" PRIx64 "]\n"
             "  TTBR1:0[0x%04" PRIx64 "]:1[0x%4" PRIx64 "]:2[0x%4" PRIx64 "]:3[0x%4" PRIx64 "]\n"
             "  CXIDR:0[0x%04" PRIx64 "]:1[0x%4" PRIx64 "]:2[0x%4" PRIx64 "]:3[0x%4" PRIx64 "]\n"
             "  TPIDR:0[0x%016" PRIx64 "]:1[0x%016" PRIx64 "]:2[0x%016" PRIx64 "]:3[0x%016" PRIx64 "]\n"
             "  TPIDX:0[0x%016" PRIx64 "]:R[0x%016" PRIx64 "]\n",
             cs->cpu_index,
             env->cp15.ttbr0_el[0], env->cp15.ttbr0_el[1], env->cp15.ttbr0_el[2], env->cp15.ttbr0_el[3],
             env->cp15.ttbr1_el[0], env->cp15.ttbr1_el[1], env->cp15.ttbr1_el[2], env->cp15.ttbr1_el[3],
             env->cp15.contextidr_el[0], env->cp15.contextidr_el[1], env->cp15.contextidr_el[2], env->cp15.contextidr_el[3],
             env->cp15.tpidr_el[0], env->cp15.tpidr_el[1], env->cp15.tpidr_el[2], env->cp15.tpidr_el[3],
             env->cp15.tpidruro_ns, env->cp15.tpidrro_el[0]);
}