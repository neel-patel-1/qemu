#if defined(TCG_GEN)

DEF_HELPER_1(vcpu_magic_inst, void, i64)

#else /* NO TCG_GEN */

// void HELPER(vcpu_magic_inst)(uint64_t op);

#endif /* TCG_GEN */