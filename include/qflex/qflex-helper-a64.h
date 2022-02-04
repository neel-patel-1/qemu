/** 
 * HELPER definitions for TCG code generation.
 */
#if defined(TCG_GEN) && defined(CONFIG_QFLEX)
DEF_HELPER_2(qflex_magic_inst, void, env, i64)
DEF_HELPER_3(qflex_mem_trace, void, env, i64, i64)
DEF_HELPER_3(qflex_post_mem, void, env, i64, i64)
DEF_HELPER_1(qflex_exception_return, void, env)
DEF_HELPER_3(qflex_executed_instruction, void, env, i64, int)

#elif !defined(CONFIG_QFLEX) 
// Empty definitions when disabled
void HELPER(qflex_magic_inst)(CPUARMState *env, uint64_t nop_op);
void HELPER(qflex_mem_trace)(CPUARMState *env, uint64_t addr, uint64_t type);
void HELPER(qflex_post_mem)(CPUARMState *env, uint64_t addr, uint64_t type);
void HELPER(qflex_exception_return)(CPUARMState *env);
void HELPER(qflex_executed_instruction)(CPUARMState* env, uint64_t pc, int location);

#endif

#if defined(TCG_GEN) && defined(CONFIG_DEVTEROFLEX)
DEF_HELPER_3(devteroflex_example_instrumentation, void , env, i64, i64)

#elif !defined(CONFIG_DEVTEROFLEX)
void HELPER(devteroflex_example_instrumentation)(CPUARMState *env, uint64_t arg1, uint64_t arg2));

#endif