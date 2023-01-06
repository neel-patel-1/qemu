#include "qemu/osdep.h"
#include "cpu.h"
#include "exec/helper-proto.h"
#include "exec/log.h"
#include "exec/exec-all.h"
#include "exec/helper-arch-tcg.h"
#include "exec/helper-arch.h"

// For magic insts, see `exec/helper-arch-qemu-calls.h`
static uint64_t curr_op = 127;
void HELPER(vcpu_magic_inst)(uint64_t op) { 
    if (curr_op == 124) {
       switch(op) {
        case 94: 
            arch_magic_inst(OP_START_ACTION); 
            break;
        case 95:
            arch_magic_inst(OP_STOP_ACTION); 
            break;
        default:
            arch_magic_inst(-1);
        }
    } else if (curr_op == op) {
        curr_op--;
    } else {
        curr_op = 127;
    }
}

bool HELPER(vcpu_is_userland)(CPUState *cpu) { return arm_current_el(ENV(cpu)) == 0; }
uint16_t HELPER(vcpu_get_asid)(CPUState *cpu) { return ENV(cpu)->cp15.ttbr1_ns >> 48; }
