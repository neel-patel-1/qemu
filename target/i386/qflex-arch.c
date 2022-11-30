#include "qemu/osdep.h"
#include "cpu.h"
#include "internals.h"

#include "qflex-helper.h"
#include "qflex/qflex.h"
#include "exec/log.h"

/* qflex/qflex-arch.h
 */
#define ENV(cpu) ((CPUARMState *) cpu->env_ptr)

uint64_t QFLEX_GET_ARCH(pc)(CPUState *cs) { abort(); }
int      QFLEX_GET_ARCH(el)(CPUState *cs) { abort(); }
uint64_t QFLEX_GET_ARCH(reg)(CPUState *cs, int reg_index) { abort(); }

uint64_t QFLEX_GET_ARCH(asid)(CPUState *cs) { printf("Function not supported for x86 architecture"); abort(); }
uint64_t QFLEX_GET_ARCH(tid)(CPUState *cs) { printf("Function not supported for x86 architecture"); abort(); }
void QFLEX_GET_ARCH(log_inst)(CPUState *cs) { printf("Function not supported for x86 architecture"); abort(); }
uint64_t gva_to_hva(CPUState *cs, uint64_t addr, int access_type) { printf("Function not supported for x86 architecture"); abort(); }
void qflex_dump_archstate_log(CPUState *cpu) { printf("Function not supported for x86 architecture"); abort(); }
