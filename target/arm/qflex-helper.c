//  DO-NOT-REMOVE begin-copyright-block
// QFlex consists of several software components that are governed by various
// licensing terms, in addition to software that was developed internally.
// Anyone interested in using QFlex needs to fully understand and abide by the
// licenses governing all the software components.
// 
// ### Software developed externally (not by the QFlex group)
// 
//     * [NS-3] (https://www.gnu.org/copyleft/gpl.html)
//     * [QEMU] (http://wiki.qemu.org/License)
//     * [SimFlex] (http://parsa.epfl.ch/simflex/)
//     * [GNU PTH] (https://www.gnu.org/software/pth/)
// 
// ### Software developed internally (by the QFlex group)
// **QFlex License**
// 
// QFlex
// Copyright (c) 2020, Parallel Systems Architecture Lab, EPFL
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
// 
//     * Redistributions of source code must retain the above copyright notice,
//       this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright notice,
//       this list of conditions and the following disclaimer in the documentation
//       and/or other materials provided with the distribution.
//     * Neither the name of the Parallel Systems Architecture Laboratory, EPFL,
//       nor the names of its contributors may be used to endorse or promote
//       products derived from this software without specific prior written
//       permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE PARALLEL SYSTEMS ARCHITECTURE LABORATORY,
// EPFL BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
// GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//  DO-NOT-REMOVE end-copyright-block
#include "qemu/osdep.h"
#include "cpu.h"
#include "exec/helper-proto.h"
#include "exec/log.h"

#include "qemu/typedefs.h"
#include "qflex-helper.h"
#include "qflex/qflex-log.h"
#include "qflex/qflex.h"

#include "../libqflex/api.h"
#include "disas/disas.h"
#include "qmp-commands.h"
#include "qom/cpu.h"
#include "sysemu/sysemu.h"
#include "exec/exec-all.h"

/* TCG helper functions. (See exec/helper-proto.h  and target/arch/helper.h)
 * This one expands prototypes for the helper functions.
 * They get executed in the TB
 * To use them: in translate.c or translate-a64.c
 * ex: HELPER(qflex_func)(arg1, arg2, ..., argn)
 * gen_helper_qflex_func(arg1, arg2, ..., argn)
 */

/**
 * @brief HELPER(qflex_executed_instruction)
 * location: location of the gen_helper_ in the transalation.
 *           EXEC_IN : Started executing a TB
 *           EXEC_OUT: Done executing a TB, NOTE: Branches don't trigger this helper.
 */
void HELPER(qflex_executed_instruction)(CPUARMState* env, uint64_t pc, int flags, int location) {
    CPUState *cs = CPU(arm_env_get_cpu(env));
    //int cur_el = arm_current_el(env);

    switch(location) {
    case QFLEX_EXEC_IN:
        if(unlikely(qflex_loglevel_mask(QFLEX_LOG_TB_EXEC))) {
            qemu_log_lock();
            qemu_log("IN[%d]  :", cs->cpu_index);
            log_target_disas(cs, pc, 4, flags);
            qemu_log_unlock();
        }
        qflex_update_inst_done(true);
        break;
    default: break;
    }
}

/**
 * @brief HELPER(qflex_magic_insn)
 * In ARM, hint instruction (which is like a NOP) comes with an int with range 0-127
 * Big part of this range is defined as a normal NOP.
 * Too see which indexes are already used ref (curently 39-127 is free) :
 * https://developer.arm.com/docs/ddi0596/a/a64-base-instructions-alphabetic-order/hint-hint-instruction
 *
 * This function is called when a HINT n (90 < n < 127) TB is executed
 * nop_op: in HINT n, it's the selected n.
 *
 */
void HELPER(qflex_magic_insn)(int nop_op) {
    switch(nop_op) {
    case 100: qflex_log_mask_enable(QFLEX_LOG_INTERRUPT); break;
    case 101: qflex_log_mask_disable(QFLEX_LOG_INTERRUPT); break;
    case 102: qflex_log_mask_enable(QFLEX_LOG_MAGIC_INSN); break;
    case 103: qflex_log_mask_disable(QFLEX_LOG_MAGIC_INSN); break;
    default: break;
    }
    qflex_log_mask(QFLEX_LOG_MAGIC_INSN,"MAGIC_INST:%u\n", nop_op);
}

/**
 * @brief HELPER(qflex_exception_return)
 * This helper gets called after a ERET TB execution is done.
 * The env passed as argument has already changed EL and jumped to the ELR.
 * For the moment not needed.
 */
void HELPER(qflex_exception_return)(CPUARMState *env) { return; }


static void qemu_dump_cpu(CPUState *cs, char **str) {
  char *f = *str;
  ARMCPU *cpu = ARM_CPU(cs);
  CPUARMState *env = &cpu->env;
  uint32_t psr = pstate_read(env);
  int i;
  int el = arm_current_el(env);
  const char *ns_status;

  f += sprintf(f, "PC=%016" PRIx64 "  SP=%016" PRIx64 "\n", env->pc,
               env->xregs[31]);
  for (i = 0; i < 31; i++) {
    f += sprintf(f, "X%02d=%016" PRIx64, i, env->xregs[i]);
    if ((i % 4) == 3) {
      f += sprintf(f, "\n");
    } else {
      f += sprintf(f, " ");
    }
  }

  ns_status = "";

  f += sprintf(f, "\nPSTATE=%08x %c%c%c%c %sEL%d%c\n", psr,
               psr & PSTATE_N ? 'N' : '-', psr & PSTATE_Z ? 'Z' : '-',
               psr & PSTATE_C ? 'C' : '-', psr & PSTATE_V ? 'V' : '-',
               ns_status, el, psr & PSTATE_SP ? 'h' : 't');
}

void qemu_dump_state(void *obj, char **buf) {
  CPUState *cpu = (CPUState *)obj;
  qemu_dump_cpu(cpu, buf);
}

char *disassemble(void *cpu, uint64_t pc) {
  CPUState *cs = (CPUState *)cpu;

  if (pc == 0) {
    CPUARMState *env = cs->env_ptr;
    pc = env->pc;
  }
  FILE *fp;
  fp = fopen("disas-temp.txt", "w+");
  target_disas(fp, cs, pc, 4, 2);
  fclose(fp);

  char *buffer = 0;
  long length;
  FILE *f = fopen("disas-temp.txt", "rb");

  if (f) {
    fseek(f, 0, SEEK_END);
    length = ftell(f);
    fseek(f, 0, SEEK_SET);
    buffer = malloc(length);
    if (buffer) {
      size_t bytes = fread(buffer, 1, length, f);
      if (bytes != length) {
        int errcode = ferror(f);
        fprintf(stderr,
                "ERROR in disassemble. Tried to read %lu bytes into buffer "
                "addr 0x%p."
                "Only successfully read %lu. Error from \'ferror\' is: %d\n",
                length, buffer, bytes, errcode);
        assert(false);
      }
    }
    fclose(f);
  }

  if (buffer) {
    return buffer;
  } else {
    assert(false);
  }

  int r = remove("disas-temp.txt");
  if (r != 0) {
    assert(false);
  }
}

// uint64_t cpu_get_pending_interrupt( void *obj) {
//     CPUState *cs = (CPUState*)obj;
//     uint64_t ret = 0;

//    ARMCPU *cpu = ARM_CPU(cs);

//    ret = (cpu->power_state != PSCI_OFF)
//            && (  CPU_INTERRUPT_FIQ | CPU_INTERRUPT_HARD
//                | CPU_INTERRUPT_VFIQ | CPU_INTERRUPT_VIRQ
//                | CPU_INTERRUPT_EXITTB);
//    /* External aborts are not possible in QEMU so A bit is always clear */

//    if (! arm_excp_unmasked(cs, EXCP_IRQ, 1)){
//        ret = 0;
//    }
//    return ret;
//}

uint64_t cpu_get_pending_interrupt(void *obj) {

  CPUState *cs = (CPUState *)obj;
  int interrupt_request = cs->interrupt_request;
  CPUARMState *env = cs->env_ptr;
  uint32_t cur_el = arm_current_el(env);
  bool secure = arm_is_secure(env);
  uint32_t target_el;
  uint32_t excp_idx;
  int ret = 0;

  if (interrupt_request & CPU_INTERRUPT_FIQ) {
    excp_idx = EXCP_FIQ;
    target_el = arm_phys_excp_target_el(cs, excp_idx, cur_el, secure);
    if (arm_excp_unmasked(cs, excp_idx, target_el)) {
      ret = 1;
    }
  }
  if (interrupt_request & CPU_INTERRUPT_HARD) {
    excp_idx = EXCP_IRQ;
    target_el = arm_phys_excp_target_el(cs, excp_idx, cur_el, secure);
    if (arm_excp_unmasked(cs, excp_idx, target_el)) {
      ret = 2;
    }
  }
  if (interrupt_request & CPU_INTERRUPT_VIRQ) {
    excp_idx = EXCP_VIRQ;
    target_el = 1;
    if (arm_excp_unmasked(cs, excp_idx, target_el)) {
      ret = 3;
    }
  }
  if (interrupt_request & CPU_INTERRUPT_VFIQ) {
    excp_idx = EXCP_VFIQ;
    target_el = 1;
    if (arm_excp_unmasked(cs, excp_idx, target_el)) {
      ret = 4;
    }
  }

  return ret;
}

uint64_t cpu_get_program_counter(void *cs_) {
  CPUState *cs = (CPUState *)cs_;
  CPUARMState *env = cs->env_ptr;
  return env->pc;
}

// void cpu_set_program_counter(void *cs_, uint64_t aVal) {
//     CPUARMState* env = (CPUARMState*)cs_;
//     env->pc = aVal;
//     printf("AFTER PC = %ul\n",env->pc);
// }

bool cpu_is_idle(void *obj) {
  CPUState *cs = (CPUState *)obj;

  return cs->halted;
}

void cpu_write_register(void *cpu, arm_register_t reg_type, int reg_index,
                        uint64_t value) {
  CPUARMState *env = ((CPUState *)cpu)->env_ptr;

  switch (reg_type) {
  case kGENERAL:
    assert(reg_index <= 31 && reg_index >= 0);
    env->xregs[reg_index] = value;
    break;
  default:
    qflex_log_mask(QFLEX_LOG_GENERAL,
                   "Non-implemented case: reg_index: %d, reg_type: %d\n",
                   reg_index, reg_type);
    assert(false);
    break;
  }
}

/* Aarch 64 helpers */
void helper_flexus_insn_fetch_aa64(CPUARMState *env, target_ulong pc,
                                   uint64_t targ_addr, int ins_size,
                                   int is_user, int cond, int annul) {
  helper_flexus_insn_fetch(env, pc, targ_addr, ins_size, is_user, cond, annul);
}

void helper_flexus_ld_aa64(CPUARMState *env, uint64_t addr, int size,
                           int is_user, target_ulong pc, int is_atomic) {
  helper_flexus_ld(env, addr, size, is_user, pc, is_atomic);
}

void helper_flexus_st_aa64(CPUARMState *env, uint64_t addr, int size,
                           int is_user, target_ulong pc, int is_atomic) {
  helper_flexus_st(env, addr, size, is_user, pc, is_atomic);
}

void flexus_cache_op_transaction(CPUARMState *env, target_ulong pc, int is_user,
                                 cache_type_t cache_type,
                                 cache_maintenance_op_t op, int line,
                                 int data_is_set_and_way, uint64_t data);

void flexus_cache_op_transaction(CPUARMState *env, target_ulong pc, int is_user,
                                 cache_type_t cache_type,
                                 cache_maintenance_op_t op, int line,
                                 int data_is_set_and_way, uint64_t data) {

  ARMCPU *cpu = arm_env_get_cpu(env);
  CPUState *cs = CPU(cpu);

  // In Qemu, PhysicalIO address space and PhysicalMemory address
  // space are combined into one (the cpu address space)
  // Operations on this address space may lead to I/O and Physical Memory
  conf_object_t *space = malloc(sizeof(conf_object_t));
  space->type = QEMU_AddressSpace;
  space->object = cs->as;
  memory_transaction_t *mem_trans = malloc(sizeof(memory_transaction_t));
  mem_trans->s.cpu_state = cs;
  mem_trans->s.ini_ptr = space;
  mem_trans->s.pc = pc;
  mem_trans->s.logical_address = 0;
  mem_trans->s.physical_address = 0;
  mem_trans->s.type = QEMU_Trans_Cache;
  mem_trans->s.size = 0;
  mem_trans->s.atomic = 0;
  mem_trans->arm_specific.user = is_user;
  mem_trans->io = 0;
  mem_trans->cache = cache_type;
  mem_trans->cache_op = op;
  mem_trans->line = line;
  mem_trans->data_is_set_and_way = data_is_set_and_way;

  if (data_is_set_and_way) {
    // FLEXUS TODO: handle it correctly
    mem_trans->set_and_way.set = 0;
    mem_trans->set_and_way.way = 0;
  } else {
    // FLEXUS TODO: handle it correctly
    mem_trans->addr_range.start_paddr = data & 0x0F; // last 4 bits are ignored
    mem_trans->addr_range.end_paddr = data & 0x0F;   // last 4  bits are ignored
  }

  QEMU_callback_args_t *event_data = malloc(sizeof(QEMU_callback_args_t));
  event_data->ncm = malloc(sizeof(QEMU_ncm));
  event_data->ncm->space = space;
  event_data->ncm->trans = mem_trans;

#ifdef CONFIG_DEBUG_LIBQFLEX
  if (is_user)
    QEMU_increment_debug_stat(CACHEOPS_USER_CNT);
  else
    QEMU_increment_debug_stat(CACHEOPS_OS_CNT);

  QEMU_increment_debug_stat(CACHEOPS_ALL_CNT);
  QEMU_increment_debug_stat(QEMU_CALLBACK_CNT);
#endif

  QEMU_execute_callbacks(cs->cpu_index, QEMU_cpu_mem_trans, event_data);

  free(space);
  free(mem_trans);
  free(event_data->ncm);
  free(event_data);
}

/* FIXME:
     Prototypes for cache management operation handlers
     These are not really implemented, and they seem to be used
     in a non-negligible manner in some parts of the guest kernel.
     Therefore we should support them correctly... */
void flexus_cp15_inv_icache(CPUARMState *env, const ARMCPRegInfo *opaque,
                            uint64_t value);

void flexus_cp15_inv_icache_line_addr(CPUARMState *env,
                                      const ARMCPRegInfo *opaque,
                                      uint64_t value);

void flexus_cp15_inv_icache_line_setway(CPUARMState *env,
                                        const ARMCPRegInfo *opaque,
                                        uint64_t value);

void flexus_cp15_flush_prefetch_buffer(CPUARMState *env,
                                       const ARMCPRegInfo *opaque,
                                       uint64_t value);

void flexus_cp15_inv_dcache(CPUARMState *env, const ARMCPRegInfo *opaque,
                            uint64_t value);

void flexus_cp15_clean_entire_dcache(CPUARMState *env,
                                     const ARMCPRegInfo *opaque,
                                     uint64_t value);

void flexus_cp15_clean_dcache(CPUARMState *env, const ARMCPRegInfo *opaque,
                              uint64_t value);

/* stub implementations */
void flexus_cp15_inv_icache(CPUARMState *env, const ARMCPRegInfo *opaque,
                            uint64_t value) {
  printf("\n\n\nInvalidating ICACHE\n\n\n");
  ARMCPU *cpu = arm_env_get_cpu(env);
  CPUState *cs = CPU(cpu);

  int is_user = (arm_current_el(env) == 0);
  flexus_cache_op_transaction(env, cpu_get_program_counter(cs), is_user,
                              QEMU_Instruction_Cache, QEMU_Invalidate_Cache,
                              0 /* whole cache */, 0, 0);
}

void flexus_cp15_inv_icache_line_addr(CPUARMState *env,
                                      const ARMCPRegInfo *opaque,
                                      uint64_t value) {
  printf("\n\n\nInvalidating ICACHE line (addr)\n\n\n");
}

void flexus_cp15_inv_icache_line_setway(CPUARMState *env,
                                        const ARMCPRegInfo *opaque,
                                        uint64_t value) {
  printf("\n\n\nInvalidating ICACHE line (addr)\n\n\n");
}

void flexus_cp15_flush_prefetch_buffer(CPUARMState *env,
                                       const ARMCPRegInfo *opaque,
                                       uint64_t value) {
  printf("\n\n\nFlushing prefetch buffer\n\n\n");
}

void flexus_cp15_inv_dcache(CPUARMState *env, const ARMCPRegInfo *opaque,
                            uint64_t value) {
  printf("\n\n\nInvalidating DCACHE\n\n\n");
  ARMCPU *cpu = arm_env_get_cpu(env);
  CPUState *cs = CPU(cpu);

  int is_user = (arm_current_el(env) == 0);
  flexus_cache_op_transaction(env, cpu_get_program_counter(cs), is_user,
                              QEMU_Data_Cache, QEMU_Invalidate_Cache,
                              0 /* whole cache */, 0, 0);
}

void flexus_cp15_clean_entire_dcache(CPUARMState *env,
                                     const ARMCPRegInfo *opaque,
                                     uint64_t value) {
  printf("\n\n\nCleaning DCACHE\n\n\n");
  ARMCPU *cpu = arm_env_get_cpu(env);
  CPUState *cs = CPU(cpu);

  int is_user = (arm_current_el(env) == 0);
  flexus_cache_op_transaction(env, cpu_get_program_counter(cs), is_user,
                              QEMU_Data_Cache, QEMU_Clean_Cache,
                              0 /* whole cache */, 0, 0);
}

void flexus_cp15_clean_dcache(CPUARMState *env, const ARMCPRegInfo *opaque,
                              uint64_t value) {
  printf("\n\n\nCleaning DCACHE\n\n\n");
}

#ifdef CONFIG_EXTSNAP
static uint64_t num_inst;
static bool phases_init = false;

void helper_phases(CPUARMState *env) {
  if (is_phases_enabled()) {
    if (!phases_init) {
      phases_init = true;
    }
    if (phase_is_valid()) {
      if (++num_inst == get_phase_value()) {
        vm_stop(RUN_STATE_PAUSED);
        save_phase();
        pop_phase();
        num_inst = 0;
      }
    } else if (!save_request_pending()) {
      fprintf(stderr, "done creating phases.");
      toggle_phases_creation();
      request_quit();
    }
  } else if (is_ckpt_enabled()) {
    if (++num_inst % get_ckpt_interval() == 0) {
      vm_stop(RUN_STATE_PAUSED);
      save_ckpt();
    }

    if (num_inst >= get_ckpt_end()) {
      toggle_ckpt_creation();
      fprintf(stderr, "done creating checkpoints.");
      request_quit();
    }
  }
}
#endif

/* cached variables */
conf_object_t space_cached;
memory_transaction_t mem_trans_cached;
QEMU_callback_args_t event_data_cached;
QEMU_ncm ncm_cached;

/* Generic Flexus helper functions */
void flexus_insn_fetch_transaction(CPUARMState *env,
                                   logical_address_t target_vaddr,
                                   physical_address_t target_phys_address,
                                   logical_address_t pc, mem_op_type_t type,
                                   int ins_size, int is_user, int cond,
                                   int annul);

void flexus_insn_fetch_transaction(CPUARMState *env,
                                   logical_address_t target_vaddr,
                                   physical_address_t paddr,
                                   logical_address_t pc, mem_op_type_t type,
                                   int ins_size, int is_user, int cond,
                                   int annul) {
#if defined(CONFIG_TEST_TIME) && defined(CONFIG_TEST_PROP)
  test_brif_ins++;
#endif

#if (defined(CONFIG_TEST_TIME) && defined(CONFIG_TEST_FETCH)) ||               \
    !defined(CONFIG_TEST_TIME)
#if defined(CONFIG_TEST_TIME) && defined(CONFIG_TEST_FETCH_TIME)
  int64_t start_time = clock_get_current_time_us();
#endif /* CONFIG_TEST_TIME */

  ARMCPU *cpu = arm_env_get_cpu(env);
  CPUState *cs = CPU(cpu);

  // In Qemu, PhysicalIO address space and PhysicalMemory address
  // space are combined into one (the cpu address space)
  // Operations on this address space may lead to I/O and Physical Memory
  conf_object_t *space = &space_cached;
  space->type = QEMU_AddressSpace;
  space->object = cs->as;
  memory_transaction_t *mem_trans = &mem_trans_cached;
  mem_trans->s.cpu_state = cs;
  mem_trans->s.ini_ptr = space;
  mem_trans->s.pc = pc;
  // the "logical_address" must be the PC for Flexus
  mem_trans->s.logical_address = pc;
  // the "physical_address" is the physical target of the branch.
  mem_trans->s.physical_address = paddr;
  mem_trans->s.type = type;
  mem_trans->s.size = ins_size;
  mem_trans->s.branch_type = cond;
  mem_trans->s.annul = annul;
  mem_trans->arm_specific.user = is_user;
  QEMU_callback_args_t *event_data = &event_data_cached;
  event_data->ncm = &ncm_cached;
  event_data->ncm->space = space;
  event_data->ncm->trans = mem_trans;

#ifdef CONFIG_DEBUG_LIBQFLEX
  if (is_user == 1)
    QEMU_increment_debug_stat(USER_FETCH_CNT);
  else
    QEMU_increment_debug_stat(OS_FETCH_CNT);

  QEMU_increment_debug_stat(ALL_FETCH_CNT);

  QEMU_increment_debug_stat(QEMU_CALLBACK_CNT);
#endif

#ifdef CONFIG_QFLEX_LIB
  sInstructionData *insdata = malloc(sizeof(sInstructionData));
  insdata->cpu_index = cs->cpu_index;
  insdata->mem_trans = mem_trans;
  insdata->type = QEMU_cpu_mem_trans;
  QFLEX_SendInstruction(insdata);
  free(insdata);

#endif

  QEMU_execute_callbacks(cs->cpu_index, QEMU_cpu_mem_trans, event_data);

#if defined(CONFIG_TEST_TIME) && defined(CONFIG_TEST_FETCH_TIME)
  int64_t end_time = clock_get_current_time_us();
  test_cumulative_brif_time += (end_time - start_time);
#endif /* CONFIG_TEST_TIME */
#endif
}

void flexus_transaction(CPUARMState *env, logical_address_t vaddr,
                        physical_address_t paddr, logical_address_t pc,
                        mem_op_type_t type, int size, int is_user, int atomic,
                        int asi, int prefetch_fcn, int io, uint8_t cache_bits);

void flexus_transaction(CPUARMState *env, logical_address_t vaddr,
                        physical_address_t paddr, logical_address_t pc,
                        mem_op_type_t type, int size, int is_user, int atomic,
                        int asi, int prefetch_fcn, int io, uint8_t cache_bits) {
#if defined(CONFIG_TEST_TIME) && defined(CONFIG_TEST_PROP)
  test_ls_ins++;
#endif

#if (defined(CONFIG_TEST_TIME) && defined(CONFIG_TEST_LS)) ||                  \
    !defined(CONFIG_TEST_TIME)
#if defined(CONFIG_TEST_TIME) && defined(CONFIG_TEST_LS_TIME)
  int64_t start_time = clock_get_current_time_us();
#endif /* CONFIG_TEST_TIME */

  ARMCPU *cpu = arm_env_get_cpu(env);
  CPUState *cs = CPU(cpu);

  // In Qemu, PhysicalIO address space and PhysicalMemory address
  // space are combined into one (the cpu address space)
  // Operations on this address space may lead to I/O and Physical Memory
  conf_object_t *space = &space_cached;
  space->type = QEMU_AddressSpace;
  space->object = cs->as;
  memory_transaction_t *mem_trans = &mem_trans_cached;
  mem_trans->s.cpu_state = cs;
  mem_trans->s.ini_ptr = space;
  mem_trans->s.pc = pc;
  mem_trans->s.logical_address = vaddr;
  mem_trans->s.physical_address = paddr;
  mem_trans->s.type = type;
  mem_trans->s.size = size;
  mem_trans->s.atomic = atomic;
  // TODO what to do here?
  /*
  // Cache_Bits: Cache Physical Bit 0
  //		   Cache Virtual Bit 1
  mem_trans->sparc_specific.cache_virtual  = (cache_bits >> 1);
  mem_trans->sparc_specific.cache_physical = (cache_bits & 1);
  // Check to see if CPU in privileged mode (PSTATE.PRIV bit)
  mem_trans->sparc_specific.priv = (env->pstate & PS_PRIV);
  mem_trans->sparc_specific.address_space = asi;
  if(type == QEMU_Trans_Prefetch){
      mem_trans->sparc_specific.prefetch_fcn = prefetch_fcn;
  }
  //to see what is different I am going to print out the mem_trans info
//    print_mem(mem_trans);
*/
  mem_trans->arm_specific.user = is_user;
  mem_trans->io = io;
  QEMU_callback_args_t *event_data = &event_data_cached;
  event_data->ncm = &ncm_cached;
  event_data->ncm->space = space;
  event_data->ncm->trans = mem_trans;

#ifdef CONFIG_DEBUG_LIBQFLEX
  if (is_user)
    QEMU_increment_debug_stat(NUM_TRANS_USER);
  else
    QEMU_increment_debug_stat(NUM_TRANS_OS);

  QEMU_increment_debug_stat(NUM_TRANS_ALL);
  QEMU_increment_debug_stat(QEMU_CALLBACK_CNT);
#endif

  QEMU_execute_callbacks(cs->cpu_index, QEMU_cpu_mem_trans, event_data);

#if defined(CONFIG_TEST_TIME) && defined(CONFIG_TEST_LS_TIME)
  int64_t end_time = clock_get_current_time_us();
  test_cumulative_ls_time += (end_time - start_time);
#endif /* CONFIG_TEST_TIME */
#endif
}

/*
 * Arguments: magic instruction's reg id, params coming through GP regs:
 * m0: cmd_id (pause QEMU is 999)
 * m1, m2: user defined
 */
void helper_flexus_magic_ins(CPUARMState *env, int trig_reg, uint64_t cmd_id,
                             uint64_t user_v1, uint64_t user_v2) {
  ARMCPU *arm_cpu = arm_env_get_cpu(env);
  CPUState *cpu = CPU(arm_cpu);
  qflex_log_mask(QFLEX_LOG_MAGIC_INSN,
                 "Received magic instruction: %d, and 0x%" PRId64 ", 0x%" PRIx64
                 ", 0x%" PRIx64 ", Core: %d\n",
                 trig_reg, cmd_id, user_v1, user_v2, cpu->cpu_index);

  /* Msutherl: If in simulation mode, execute magic_insn callback types. */
  if ((flexus_in_timing() && qflex_control_with_flexus) || flexus_in_trace()) {
    QEMU_callback_args_t *event_data = malloc(sizeof(QEMU_callback_args_t));
    event_data->nocI = malloc(sizeof(QEMU_nocI));
    event_data->nocI->bigint = cpu->cpu_index;
    QEMU_execute_callbacks(cpu->cpu_index, QEMU_magic_instruction, event_data);
    free(event_data->nocI);
    free(event_data);
  }

  if (cmd_id == 999) {
    printf("QEMU stopped by a magic instruction\n");
    vm_stop(RUN_STATE_PAUSED);
  }
}

void finish_performance(void);

void helper_flexus_periodic(CPUARMState *env, int isUser) {

  if (flexus_in_trace() && qflex_trace_enabled) {
    ARMCPU *arm_cpu = arm_env_get_cpu(env);
    CPUState *cpu = CPU(arm_cpu);

    static uint64_t instCnt = 0;

    int64_t simulation_length = QEMU_getCyclesLeft();
    if (simulation_length >= 0 && instCnt >= simulation_length) {

      qflex_trace_enabled = false;
      static bool exited = false;
      exited = QEMU_quit_simulation("Reached the end of the simulation");

      if (exited) {
        cpu->exit_request = 1;
        cpu_loop_exit(cpu);
        return;
      }
    }

#ifdef CONFIG_DEBUG_LIBQFLEX
    if (isUser == 1)
      QEMU_increment_debug_stat(USER_INSTR_CNT);
    else
      QEMU_increment_debug_stat(OS_INSTR_CNT);

    QEMU_increment_debug_stat(ALL_INSTR_CNT);

    QEMU_increment_debug_stat(QEMU_CALLBACK_CNT);
#endif

    QEMU_increment_instruction_count(cpu->cpu_index, isUser);

    instCnt++;

    uint64_t eventDelay = 1000;
    if ((instCnt % eventDelay) == 0) {
      QEMU_callback_args_t *event_data = &event_data_cached;
      event_data->ncm = &ncm_cached;

      QEMU_execute_callbacks(QEMUFLEX_GENERIC_CALLBACK, QEMU_periodic_event,
                             event_data);
    }
  }
}

/* QFlex generic API functions */
int cpu_proc_num(void *cs_) {
  CPUState *cs = (CPUState *)cs_;
  return cs->cpu_index;
}

physical_address_t mmu_logical_to_physical(void *cs_, logical_address_t va) {
  //physical_address_t pa = cpu_get_phys_page_debug(cs, va);
  CPUState *cs = (CPUState *) cs_;
  return va_to_phys_arch(cs, va, MMU_DATA_LOAD);
}

void cpu_read_exception(void *obj, exception_t *exp) {
  CPUState *cs = (CPUState *)obj;
  ARMCPU *cpu = ARM_CPU(cs);
  CPUARMState *env = &cpu->env;

  exp->fsr = env->exception.fsr;
  exp->syndrome = env->exception.syndrome;
  exp->target_el = env->exception.target_el;
  exp->vaddress = env->exception.vaddress;
}

uint64_t cpu_read_hcr_el2(void *obj) {

  CPUState *cs = (CPUState *)obj;
  ARMCPU *cpu = ARM_CPU(cs);
  CPUARMState *env = &cpu->env;
  return env->cp15.hcr_el2;
}

bool cpu_read_AARCH64(void *obj) {

  CPUState *cs = (CPUState *)obj;
  ARMCPU *cpu = ARM_CPU(cs);
  CPUARMState *env = &cpu->env;
  return env->aarch64;
}

uint64_t cpu_read_sp_el(uint8_t id, void *obj) {
  CPUState *cs = (CPUState *)obj;
  ARMCPU *cpu = ARM_CPU(cs);
  CPUARMState *env = &cpu->env;

  return env->sp_el[id];
}

uint32_t cpu_read_DCZID_EL0(void *obj) {

  CPUState *cs = (CPUState *)obj;
  ARMCPU *cpu = ARM_CPU(cs);
  return cpu->dcz_blocksize;
}

uint64_t cpu_read_sctlr(uint8_t id, void *obj) {
  CPUState *cs = (CPUState *)obj;
  ARMCPU *cpu = ARM_CPU(cs);
  CPUARMState *env = &cpu->env;

  return env->cp15.sctlr_el[id];
}

uint64_t cpu_read_tpidr(uint8_t id, void *obj) {
  CPUState *cs = (CPUState *)obj;
  ARMCPU *cpu = ARM_CPU(cs);
  CPUARMState *env = &cpu->env;

  return env->cp15.tpidr_el[id];
}

uint32_t cpu_read_pstate(void *obj) {

  CPUState *cs = (CPUState *)obj;
  ARMCPU *cpu = ARM_CPU(cs);
  CPUARMState *env = &cpu->env;

  return pstate_read(env);
}
uint32_t cpu_read_fpcr(void *obj) {

  CPUState *cs = (CPUState *)obj;
  ARMCPU *cpu = ARM_CPU(cs);
  CPUARMState *env = &cpu->env;

  return vfp_get_fpcr(env);
}

uint32_t cpu_read_fpsr(void *obj) {

  CPUState *cs = (CPUState *)obj;
  ARMCPU *cpu = ARM_CPU(cs);
  CPUARMState *env = &cpu->env;

  return vfp_get_fpsr(env);
}

// int cpu_read_el(void* obj){

//    CPUState *cs = (CPUState*)obj;
//    ARMCPU *cpu = ARM_CPU(cs);
//    CPUARMState *env = &cpu->env;

//    int ret = arm_current_el(env);

//    if(ret >= 0 && ret < 2)
//        return ret;
//    else
//        assert(false);

//}

uint64_t cpu_read_register(void *cpu, arm_register_t reg_type, int reg_idx) {

  CPUState *cs = (CPUState *)cpu;
  CPUARMState *env = cs->env_ptr;

  switch (reg_type) {
  case kGENERAL:
    assert(reg_idx <= 31 && reg_idx >= 0);
    return env->xregs[reg_idx];
    break;
  case kFLOATING_POINT:
    return env->vfp.regs[reg_idx];
    break;
  case kMMU_TCR: {
    int arm_el = reg_idx; // reg_idx is a misnomer here
    return env->cp15.tcr_el[arm_el].raw_tcr;
  }
  case kMMU_SCTLR: {
    int arm_el = reg_idx; // reg_idx is a misnomer here
    return env->cp15.sctlr_el[arm_el];
  }
  case kMMU_TTBR0: {
    int arm_el = reg_idx; // reg_idx is a misnomer here
    return env->cp15.ttbr0_el[arm_el];
  }
  case kMMU_TTBR1: {
    int arm_el = reg_idx; // reg_idx is a misnomer here
    return env->cp15.ttbr1_el[arm_el];
  }
  case kMMU_ID_AA64MMFR0_EL1: {
    ARMCPU *armState = arm_env_get_cpu(env);
    return armState->id_aa64mmfr1;
  }
  default:
    fprintf(stderr,
            "ERROR case triggered in readReg. reg_idx: %d, reg_type: %d\n",
            reg_idx, reg_type);
    assert(false);
    break;
  }
}
// prototype to suppress warning

/* ARM specific helpers */
// TODO FLEXUS: check if we must use addr_read or addr_code
void helper_flexus_insn_fetch(CPUARMState *env, target_ulong pc,
                              target_ulong targ_addr, int ins_size, int is_user,
                              int cond, int annul) {
  if (flexus_in_trace() && qflex_trace_enabled) {
    ARMCPU *arm_cpu = arm_env_get_cpu(env);
    /*
     * MARK: Removed old code which was accessing the TCG TLBs.
     *  - This code generates hVAddrs, which is not what Flexus should model.
     *  - Fix: Use mmu_logical_to_physical to generate gPAddrs.
     */

    physical_address_t phys_addr_functional =
        mmu_logical_to_physical((void *)arm_cpu, targ_addr);
    flexus_insn_fetch_transaction(env, targ_addr, phys_addr_functional, pc,
                                  QEMU_Trans_Instr_Fetch, ins_size, is_user,
                                  cond, annul);
  }
}

void helper_flexus_ld(CPUARMState *env, target_ulong addr, int size,
                      int is_user, target_ulong pc, int is_atomic) {
  if (flexus_in_trace() && qflex_trace_enabled) {
    ARMCPU *arm_cpu = arm_env_get_cpu(env);
    int mmu_idx = cpu_mmu_index(
        env, false); // Flexus Change made since function definition has changed
    int index = ((target_ulong)addr >> TARGET_PAGE_BITS) & (CPU_TLB_SIZE - 1);
    target_ulong tlb_addr = env->tlb_table[mmu_idx][index].addr_read;

    if ((addr & TARGET_PAGE_MASK) !=
        (tlb_addr & (TARGET_PAGE_MASK | TLB_INVALID_MASK))) {
      // Given that a previous load instruction happened, we are sure that
      // that the TLB entry is still in the CPU TLB, if not then the previous
      // instruction caused an error, so we just return with no tlb_fill() call
      return;
    }

    int io = 0;
    if (unlikely(tlb_addr & ~TARGET_PAGE_MASK)) {
      // I/O space
      io = 1;
    }
    // Otherwise, RAM/ROM , physical memory space, io = 0

    /*
     * MARK: Removed old code which was accessing the TCG TLBs.
     *  - This code generates hVAddrs, which is not what Flexus should model.
     *  - Fix: Use mmu_logical_to_physical to generate gPAddrs.
     */
    int asi = 0;
    physical_address_t phys_addr_functional =
        mmu_logical_to_physical((void *)arm_cpu, addr);
#ifdef CONFIG_DEBUG_LIBQFLEX
    if (is_user)
      QEMU_increment_debug_stat(LD_USER_CNT);
    else
      QEMU_increment_debug_stat(LD_OS_CNT);
    QEMU_increment_debug_stat(LD_ALL_CNT);
#endif

    // Here, prefetch_fcn is just a dummy argument since type is not prefetch
    flexus_transaction(env, addr, phys_addr_functional, pc, QEMU_Trans_Load,
                       size, is_user, is_atomic, asi, 0, io, 0);
  }
}

void helper_flexus_st(CPUARMState *env, target_ulong addr, int size,
                      int is_user, target_ulong pc, int is_atomic) {
  if (flexus_in_trace() && qflex_trace_enabled) {
    ARMCPU *arm_cpu = arm_env_get_cpu(env);
    int mmu_idx = cpu_mmu_index(
        env, false); // Flexus Change made since function definition has changed
    int index = ((target_ulong)addr >> TARGET_PAGE_BITS) & (CPU_TLB_SIZE - 1);
    target_ulong tlb_addr = env->tlb_table[mmu_idx][index].addr_write;

    if ((addr & TARGET_PAGE_MASK) !=
        (tlb_addr & (TARGET_PAGE_MASK | TLB_INVALID_MASK))) {
      // Given that a previous load instruction happened, we are sure that
      // that the TLB entry is still in the CPU TLB, if not then the previous
      // instruction caused an error, so we just return with no tlb_fill() call
      return;
    }

    int io = 0;
    if (unlikely(tlb_addr & ~TARGET_PAGE_MASK)) {
      // I/O space
      io = 1;
    }
    // Otherwise, RAM/ROM , physical memory space, io = 0

    /*
     * MARK: Removed old code which was accessing the TCG TLBs.
     *  - This code generates hVAddrs, which is not what Flexus should model.
     *  - Fix: Use mmu_logical_to_physical to generate gPAddrs.
     */
    int asi = 0;
#ifdef CONFIG_DEBUG_LIBQFLEX
    if (is_user)
      QEMU_increment_debug_stat(ST_USER_CNT);
    else
      QEMU_increment_debug_stat(ST_OS_CNT);

    QEMU_increment_debug_stat(ST_ALL_CNT);
#endif

    physical_address_t phys_addr_functional =
        mmu_logical_to_physical((void *)arm_cpu, addr);
    // Here, prefetch_fcn is just a dummy argument since type is not prefetch
    flexus_transaction(env, addr, phys_addr_functional, pc, QEMU_Trans_Store,
                       size, is_user, is_atomic, asi, 0, io, 0);
  }
}

/* Aarch 32 helpers */

void helper_flexus_insn_fetch_aa32(CPUARMState *env, target_ulong pc,
                                   uint32_t targ_addr, int ins_size,
                                   int is_user, int cond, int annul) {
  helper_flexus_insn_fetch(env, pc, targ_addr, ins_size, is_user, cond, annul);
}

void helper_flexus_ld_aa32(CPUARMState *env, uint32_t addr, int size,
                           int is_user, target_ulong pc, int is_atomic) {
  helper_flexus_ld(env, addr, size, is_user, pc, is_atomic);
}

void helper_flexus_st_aa32(CPUARMState *env, uint32_t addr, int size,
                           int is_user, target_ulong pc, int is_atomic) {
  helper_flexus_st(env, addr, size, is_user, pc, is_atomic);
}
