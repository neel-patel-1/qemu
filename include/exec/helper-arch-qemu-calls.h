/** 
 * Here are the constants defined for magic_insts helpers execution that 
 * can be executed by running code from the guest
 * 
 * To perform a command, you need to chain commands.
 * 
 * E.g.: 
 * To execute `qflex_singlestep_start` defined in `qflex-helper.c` insert the following lines to a C program:
 * 
 * ....
 * QEMU_INSERT_OP(QEMU_START_OP);
 * ....
 *
 * That command will chain the required series of NOP that will kickstart the command or setup a flag.
 */

#ifdef ARCH_X86

#define QEMU_START_OP __asm__(".nops 4\n\t"); // "nopl 0(%[re]ax)\n\t");
#define QEMU_STOP_OP  __asm__(".nops 5\n\t"); // "nopl 0(%[re]ax,%[re]ax,1)\n\t");

/*  .nops 10 :662e0f1f840000000000
 *  .nops 9  :660f1f840000000000
 *  .nops 8  :0f1f840000000000
 *  .nops 7  :0f1f8000000000
 *  .nops 6  :660f1f440000
 *  .nops 5  :0f1f440000
 *  .nops 4  :0f1f4000
 *  .nops 3  :0f1f00
 *  .nops 2  :6690                            
 *  .nop     :90                              
 */

#define QEMU_INSERT_OP(asm_op) __asm__( \
    ".nops 9\n\t" \
    ".nops 8\n\t" \
    ".nops 7\n\t" \
    ".nops 6\n\t" \
    ".nops 5\n\t" \
    ".nops 4\n\t" \
    ".nops 3\n\t" \
    ".nops 2\n\t" \
    ); asm_op;

int main() {
    QEMU_INSERT_OP(QEMU_START_OP);
    QEMU_INSERT_OP(QEMU_STOP_OP);
    return 0;
}


#elif ARCH_ARM

#define STR(x)  #x
#define XSTR(s) STR(s)
#define magic_inst(val) __asm__ __volatile__ ( "hint " XSTR(val) " \n\t"  )

#define QEMU_START_OP magic_inst(94);
#define QEMU_STOP_OP  magic_inst(95);

#define QEMU_INSERT_OP(asm_op) __asm__(          \
    "hint 127\n\t"  \
    "hint 126\n\t" \
    "hint 125\n\t" \
    "hint 124\n\t" \
    ); asm_op;

#endif /* ARCH_TYPE */