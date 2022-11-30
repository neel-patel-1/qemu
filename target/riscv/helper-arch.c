#include "qemu/osdep.h"
#include "cpu.h"
#include "exec/helper-proto.h"
#include "exec/log.h"
#include "exec/exec-all.h"
#include "exec/helper-arch.h"

bool HELPER(vcpu_is_userland)(CPUState *cpu) { 
    return ENV(cpu)->priv == 0; 
}

uint16_t HELPER(vcpu_get_asid)(CPUState *cpu) { 
    uint64_t asid_hi_bits = ENV(cpu)->satp & SATP64_ASID;
    uint64_t asid = asid_hi_bits >> 48;
    return asid;
}
