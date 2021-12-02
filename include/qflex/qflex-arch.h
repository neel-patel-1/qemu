#ifndef QFLEX_ARCH_H
#define QFLEX_ARCH_H

#include "qemu/osdep.h"

/* This file contains functions specific to architectural state.
 * These functions should be defined in target/arch/qflex-helper.c in
 * order to access architecture specific values
 */

// functions define to get CPUArchState specific values
#define QFLEX_GET_ARCH(name) glue(qflex_get_arch_, name)

uint64_t QFLEX_GET_ARCH(pc)(CPUState *cs);
uint64_t QFLEX_GET_ARCH(asid)(CPUState *cs);
uint64_t QFLEX_GET_ARCH(asid_reg)(CPUState *cs);
uint64_t QFLEX_GET_ARCH(tid)(CPUState *cs);
int      QFLEX_GET_ARCH(el)(CPUState *cs);
int      QFLEX_GET_ARCH(reg)(CPUState *cs, int reg_index);

uint64_t gva_to_hva(CPUState *cs, uint64_t addr, int access_type);
uint64_t gva_to_hva_with_asid(uint64_t asid_reg, uint64_t vaddr, int access_type);

#endif /* QFLEX_ARCH_H */
