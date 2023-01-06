#ifndef HELPER_ARCH_H
#define HELPER_ARCH_H

#include <stdbool.h>
#include "exec/helper-head.h"

#ifndef glue
#define xglue(x, y) x ## y
#define glue(x, y) xglue(x, y)
#endif
#define GEN_HELPER(func)  glue(gen_helper_, func)

#define ENV(cpu) ((CPUArchState *) cpu->env_ptr)

void arch_magic_inst(uint64_t op);
bool HELPER(vcpu_is_userland)(CPUState *cpu);
uint16_t HELPER(vcpu_get_asid)(CPUState *cpu);

#define OP_START_ACTION (0)
#define OP_STOP_ACTION  (1)

#endif /* HELPER_ARCH_H */