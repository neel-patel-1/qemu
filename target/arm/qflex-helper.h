#ifndef QFLEX_HELPER_H
#define QFLEX_HELPER_H

#include "cpu.h"
#include "internals.h"

/* Functions that are only present as static functions in target/arch files
 * Make them available to get called by QFLEX
 */

#define QFLEX_GET_F(func) glue(qflex_get_, func)

void qflex_print_state_asid_tid(CPUState* cs);

/* helper.c
 */

/*
 * returns '-1' on permission fault or on failure
 */
uint64_t gva_to_hva_arch(CPUState *cs, uint64_t vaddr, MMUAccessType access_type);
uint64_t gva_to_hva_arch_with_asid(CPUState *cs, uint64_t vaddr, MMUAccessType access_type, uint64_t asid_reg);

#endif /* QFLEX_HELPER_H */
