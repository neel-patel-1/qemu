#ifndef QFLEX_HELPER_H
#define QFLEX_HELPER_H

#include "cpu.h"
#include "internals.h"

/* Functions that are only present as static functions in target/arch files
 * Make them available to get called by QFLEX
 */

#define QFLEX_GET_F(func) glue(qflex_get_, func)

/* helper.c
 */

/*
 * returns '-1' on permission fault or on failure
 */
uint64_t va_to_phys_arch(CPUState *cs, uint64_t vaddr, MMUAccessType access_type);

#endif /* QFLEX_HELPER_H */