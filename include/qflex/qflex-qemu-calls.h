#ifndef QFLEX_QEMU_CALLS_H
#define QFLEX_QEMU_CALLS_H

/** 
 * Here are the constants defined for magic_insts helpers execution that 
 * can be executed by running code from the guest
 * 
 * To perform a command, you need to chaine:d two HINTS
 * 
 * E.g.: 
 * To execute `qflex_singlestep_start` defined in `qflex-helper.c` insert the following lines to a C program:
 * 
 * ....
 * DO_QFLEX_OP(QFLEX_SINGLESTEP_STOP);
 * ....
 * 
 * Which is the same as:
 * ...
 * magic_inst(QFLEX_OP);
 * magic_inst(QFLEX_SINGLESTEP_STOP);
 * ...
 * Or
 * ...
 * __asm__ __volatile__ ("hint 90 \n\t");
 * __asm__ __volatile__ ("hint 91 \n\t");
 * ...
 */

// OP TYPES
#define QFLEX_OP (90)
#define MEM_TRACE_OP      (91)

// QFLEX OPs
#define QFLEX_SINGLESTEP_START (90)
#define QFLEX_SINGLESTEP_STOP  (91)
#define QFLEX_PRINT_ASID_TID   (92)

// MEM_TRACE OPs
#define MEM_TRACE_START   (90)
#define MEM_TRACE_STOP    (91)
#define MEM_TRACE_RESULTS (92)
#define MEM_TRACE_START_INST (93)

// ---- Executing helpers inside QEMU ---- //

/* Include these in a c program inside the guest.
 * Once the helper is executed inside the guest, 
 * qemu will detect the event and trigger the command
 */
#define STR(x)  #x
#define XSTR(s) STR(s)
#define magic_inst(val) __asm__ __volatile__ ( "hint " XSTR(val) " \n\t"  )

#define DO_QFLEX_OP(op) magic_inst(QFLEX_OP); magic_inst(op)

#ifdef CONFIG_DEVTEROFLEX
#define DEVTEROFLEX_OP    (94)

#define DEVTEROFLEX_FLOW_START (90)
#define DEVTEROFLEX_FLOW_STOP  (91)
#define DO_DEVTEROFLEX_OP(op) magic_inst(DEVTEROFLEX_OP); magic_inst(op)
#endif

#endif