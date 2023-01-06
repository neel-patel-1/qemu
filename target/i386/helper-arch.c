#include "qemu/osdep.h"
#include "cpu.h"
#include "exec/helper-proto.h"
#include "exec/log.h"
#include "exec/exec-all.h"
#include "exec/helper-arch-tcg.h"
#include "exec/helper-arch.h"

static uint64_t curr_op = 9;
void HELPER(vcpu_magic_inst)(uint64_t op) { 
    if (curr_op == 1) {
       switch(op) {
        case 4: 
            arch_magic_inst(OP_START_ACTION); 
            break;
        case 5:
            arch_magic_inst(OP_STOP_ACTION); 
            break;
        default:
            arch_magic_inst(-1);
        }
    } else if (curr_op == op) {
        curr_op--;
    } else {
        curr_op = 9;
    }
}

bool HELPER(vcpu_is_userland)(CPUState *cpu) { 
    int cpl = ENV(cpu)->hflags & HF_CPL_MASK;
    return cpl == 3; 
}

uint16_t HELPER(vcpu_get_asid)(CPUState *cpu) { 
    uint16_t pcid = ENV(cpu)->cr[3] & 0xfff;
    return pcid;
}
