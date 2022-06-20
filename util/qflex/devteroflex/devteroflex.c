#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "qemu/osdep.h"
#include "qemu/thread.h"
#include "hw/core/cpu.h"

#include "qflex/qflex.h"
#include "qflex/qflex-arch.h"

#include "qflex/devteroflex/devteroflex.h"
#include "qflex/devteroflex/demand-paging.h"
#include "qflex/devteroflex/devteroflex-mmu.h"

#include "qflex/qflex-traces.h"

#include <glib.h>

DevteroflexConfig devteroflexConfig = { 
    .debug_mode = no_debug,
    .enabled = false,
    .running = false,
    .transplant_type = TRANS_CLEAR,
};

FPGAContext c;
MessageFPGA message_buffer[256] = {0};
uint64_t message_buffer_curr_entry = 0;
static DevteroflexArchState state;

// List of cpus in the FPGA
static uint32_t running_cpus;
#define cpu_in_fpga(cpu) (running_cpus & 1 << cpu)
#define cpu_push_fpga(cpu) (running_cpus |= 1 << cpu)
#define cpu_pull_fpga(cpu) (running_cpus &= ~(1 << cpu))

static bool run_debug(CPUState *cpu) {
    if (FLAGS_GET_IS_EXCEPTION(state.flags) | FLAGS_GET_IS_UNDEF(state.flags)) {
        // Singlestep like normal execution
        return false;
    } 

    qemu_log("DevteroFlex:CPU[%i]:Running debug check\n", cpu->cpu_index);
    // Singlestep and compare
    uint64_t pc_before_singlestep = QFLEX_GET_ARCH(pc)(cpu);
    uint64_t asid_before_singlestep = QFLEX_GET_ARCH(asid)(cpu);
    qflex_singlestep(cpu);
    // continue until supervised instructions are runned.
    while(QFLEX_GET_ARCH(el)(cpu) != 0){
        qflex_singlestep(cpu);
        if(QFLEX_GET_ARCH(pc)(cpu) == pc_before_singlestep) {
            // Rexecute instruction after IRQ
            qflex_singlestep(cpu);
        }
    }
    uint32_t asid_after_singlestep = QFLEX_GET_ARCH(asid)(cpu);

    if(asid_before_singlestep != asid_after_singlestep) {
        printf("WARNING:DevteroFlex:Transplant:Something in the execution changed the ASID\n");
        qemu_log("WARNING:DevteroFlex:Transplant:Something in the execution changed the ASID\n");
        // Repack state, IRQ did not return to original program
        devteroflex_pack_archstate(&state, cpu);
        ipt_register_asid(state.asid, QFLEX_GET_ARCH(asid_reg)(cpu));
     } else if(devteroflex_compare_archstate(cpu, &state)) {
        // Dangerous!!!
        qemu_log("WARNING:DevteroFlex:CPU[%i]:An architecture state mismatch has been detected. Quitting QEMU now. \n", cpu->cpu_index);
        abort();
    }

    if(devteroflexConfig.enabled && devteroflexConfig.running) {
        cpu_push_fpga(cpu->cpu_index);
        transplantPushAndSinglestep(&c, cpu->cpu_index, &state);
    }
    // End of debug mode
    return true;
}

static void transplantRun(CPUState *cpu, uint32_t thid) {
    assert(cpu->cpu_index == thid);
    assert(cpu_in_fpga(thid));
    cpu_pull_fpga(cpu->cpu_index);
    transplantGetState(&c, thid, &state);
    transplantFreePending(&c, (1 << thid));

    qemu_log("DevteroFlex:CPU[%i]:PC[0x%016lx]:transplant:EXCP[%i]:UNDEF:[%i]\n", thid, state.pc,
             FLAGS_GET_IS_EXCEPTION(state.flags)?1:0, FLAGS_GET_IS_UNDEF(state.flags)?1:0);
 
    // In debug mode, we should advance the QEMU by one step and do comparison.
    if(devteroflexConfig.debug_mode) {
        devteroflexConfig.transplant_type = TRANS_DEBUG;
        if(run_debug(cpu)) {
            // Succesfully ran the debug routine
            return; 
        }
        // Need to run normal transplant
    } else {
        devteroflex_unpack_archstate(cpu, &state);
    }

    if(FLAGS_GET_IS_EXCEPTION(state.flags)) {
        devteroflexConfig.transplant_type = TRANS_EXCP;
    } else if (FLAGS_GET_IS_UNDEF(state.flags)) {
        devteroflexConfig.transplant_type = TRANS_UNDEF;
    } else if (FLAGS_GET_IS_ICOUNT_DEPLETED(state.flags)) {
        devteroflexConfig.transplant_type = TRANS_ICOUNT;
    } else {
        devteroflexConfig.transplant_type = TRANS_UNKNOWN;
        printf("Unknown reason of transplant");
        exit(1);
    }

    // Execute the exception instruction
    uint64_t pc_before_singlestep = QFLEX_GET_ARCH(pc)(cpu);
    uint64_t asid_before_singlestep = QFLEX_GET_ARCH(asid)(cpu);
    qflex_singlestep(cpu);
    // continue until supervised instructions are runned.
    while(QFLEX_GET_ARCH(el)(cpu) != 0){
        qflex_singlestep(cpu);
        if(QFLEX_GET_ARCH(pc)(cpu) == pc_before_singlestep) {
            qflex_singlestep(cpu);
        }
    }
    uint64_t asid_after_singlestep = QFLEX_GET_ARCH(asid)(cpu);
    if(asid_before_singlestep != asid_after_singlestep) {
        printf("WARNING:DevteroFlex:Transplant:Something in the execution changed the ASID\n");
        qemu_log("WARNING:DevteroFlex:Transplant:Something in the execution changed the ASID\n");
    }

    // handle exception will change the state, so it
    if(devteroflexConfig.enabled && devteroflexConfig.running) {
        cpu_push_fpga(cpu->cpu_index);
        devteroflex_pack_archstate(&state, cpu);
        ipt_register_asid(state.asid, QFLEX_GET_ARCH(asid_reg)(cpu));

        if(devteroflexConfig.debug_mode) {
            // Ensure single instruction gets executed
            transplantPushAndSinglestep(&c, thid, &state);
        } else {
            transplantPushAndStart(&c, thid, &state);
        }
    }

}

static void transplantsRun(uint32_t pending) {
    CPUState *cpu;
    CPU_FOREACH(cpu)
    {
        if(pending & (1 << cpu->cpu_index)) {
            transplantRun(cpu, cpu->cpu_index);
            devteroflexConfig.transplant_type = TRANS_CLEAR;
        }
    }
}

static void transplantBringBack(uint32_t pending) {
    CPUState *cpu;
    uint32_t thid = -1;
    CPU_FOREACH(cpu)
    {
        thid = cpu->cpu_index;
        assert(thid != -1);
        if(pending & (1 << thid)) {
            qemu_log("DevteroFlex:CPU[%i]:Final Transplant FPGA->HOST\n", cpu->cpu_index);
            transplantGetState(&c, thid, &state);
            transplantFreePending(&c, 1 << thid);
            if(devteroflexConfig.debug_mode){
                if(devteroflex_compare_archstate(cpu, &state)) {
                    // Dangerous!!!
                    qemu_log("WARNING:DevteroFlex:CPU[%i]:An architecture state mismatch has been detected. Quitting QEMU now. \n", cpu->cpu_index);
                    abort();
                }
            } else {
                devteroflex_unpack_archstate(cpu, &state);
            }
            cpu_pull_fpga(thid);
        }
    }
}

static void transplantPushAllCpus(void) {
    CPUState *cpu;
    CPU_FOREACH(cpu) {
        cpu_push_fpga(cpu->cpu_index);
        devteroflex_pack_archstate(&state, cpu);
        ipt_register_asid(state.asid, QFLEX_GET_ARCH(asid_reg)(cpu));

        if(devteroflexConfig.debug_mode) {
            transplantPushAndSinglestep(&c, cpu->cpu_index, &state);
        } else {
            transplantPushAndStart(&c, cpu->cpu_index, &state);
        }
    }
}

static void printPMUCounters(const FPGAContext *ctx){
  uint64_t cyc = pmuTotalCycles(ctx);
  qemu_log("Total cycles: %ld \n", cyc);
  uint64_t cmt = pmuTotalCommitInstructions(ctx);
  qemu_log("Total committed instructions: %ld \n", cmt);
  qemu_log("IPC: %lf CPI: %lf \n", (double)(cmt) / cyc, (double)(cyc) / (double)(cmt));
  qemu_log("----------\n");

  const char *names[4] = {
    "DCachePenalty:",
    "TLBPenalty:",
    "TransplantPenalty:",
    "PageFaultPenalty:"
  };

  for(int idx = 0; idx < 4; ++idx){
    uint16_t penalties[16];
    assert(pmuReadCycleCounters(ctx, idx, penalties) == 0);
    qemu_log("%s", names[idx]);
    qemu_log("Raw: ");
    for(int i = 0; i < 16; ++i){
      qemu_log("%d ", penalties[i]);
    }
    qemu_log("\n");
    uint32_t cnt_sum = 0;
    uint32_t cnt_non_zero = 0;

    if(idx == 2) {
      // for the trapsplant back penalty, we should clear the one that is caused by the last transplant back. 
      penalties[0] = 0;
    }

    for(int i = 0; i < 16; ++i){
      if(penalties[i] != 0){
        cnt_sum += penalties[i];
        cnt_non_zero += 1;
      }
    }
    if(cnt_non_zero != 0) qemu_log("Average: %lf \n", (double)(cnt_sum) / cnt_non_zero);
    qemu_log("----------");
  }
}

// Functions that control QEMU execution flow
static int devteroflex_execution_flow(void) {
    MessageFPGA msg;
    uint32_t pending;
    CPUState *cpu;
    transplantPushAllCpus();
    // open the cycle counter on PMU
    pmuStartCounting(&c);
    while(1) {
        // Check and run all pending transplants
        transplantPending(&c, &pending);
        if((pending != 0)) {
            if(devteroflexConfig.enabled && devteroflexConfig.running) {
                transplantsRun(pending);
            } else {
                transplantBringBack(pending);
            }
        }
        // Run all pending messages from a synchronization
        if(message_buffer_curr_entry > 0) {
            for(int message_idx = 0; message_idx < message_buffer_curr_entry; message_idx++) {
                mmu_message_run(message_buffer[message_idx]);
            }
            message_buffer_curr_entry = 0;
        }
        // Check and run all pending messages
        while(mmu_has_pending(&msg)) {
            mmu_message_run(msg);
        }

        // If DevteroFlex stopped executing, pull all cpu's back
        if(!(devteroflexConfig.enabled && devteroflexConfig.running)) {
            CPU_FOREACH(cpu) {
                if(!cpu_in_fpga(cpu->cpu_index)) {
                    transplantStopCPU(&c, cpu->cpu_index);
                }
            }
        }
        if(!(devteroflexConfig.enabled && devteroflexConfig.running) && running_cpus == 0) {
            // Done executing
            break;
        }
    }
    // stop PMU counters
    pmuStopCounting(&c);
    // print the PMU summary
    printPMUCounters(&c);

    return 0;
}

static int qflex_singlestep_flow(void) {
    CPUState *cpu;
    printf("Will execute without attaching any DevteroFlex mechanism, only singlestepping\n");
    qemu_log("Will execute without attaching any DevteroFlex mechanism, only singlestepping\n");
    while(1) {
        CPU_FOREACH(cpu) {
            qflex_singlestep(cpu);
        }
        if(!(devteroflexConfig.enabled && devteroflexConfig.running)) {
            break;
        }
    }

    return 0;
}

static void devteroflex_prepare_singlestepping(void) {
    qflexState.exit_main_loop = false;
    qflexState.singlestep = true;
    qflexState.skip_interrupts = true;
    qemu_loglevel |= CPU_LOG_INT; 
    qflex_mem_trace_start(-1, -1);
    CPUState *cpu;
    CPU_FOREACH(cpu) {
        cpu_single_step(cpu, SSTEP_ENABLE);
        qatomic_mb_set(&cpu->exit_request, 0);
    }
}

int devteroflex_singlestepping_flow(void) {
    qemu_log("DEVTEROFLEX:icount[%09lu]:FPGA START\n", devteroflexConfig.icount);
    qflexState.log_inst = true;
    devteroflex_prepare_singlestepping();

    // If started in kernel mode continue until supervised instructions are runned
    CPUState *cpu;
    CPU_FOREACH(cpu) {
        while(QFLEX_GET_ARCH(el)(cpu) != 0){
            qflex_singlestep(cpu);
        }
    }
    if(!devteroflexConfig.pure_singlestep) {
        devteroflex_execution_flow();
    } else {
        qflex_singlestep_flow();
    }
    qemu_log("DEVTEROFLEX:icount[%09lu]:FPGA EXIT\n", devteroflexConfig.icount);
    qflexState.log_inst = false;
    devteroflex_stop_full();
    return 0;
}

void devteroflex_stop_full(void) {
    CPUState *cpu;
    qflexState.singlestep = false;
    qflexState.skip_interrupts = false;
    CPU_FOREACH(cpu) {
        cpu_single_step(cpu, 0);
    }
    qemu_loglevel &= ~CPU_LOG_TB_IN_ASM;
    qemu_loglevel &= ~CPU_LOG_INT;
    qemu_log("DEVTEROFLEX:icount[%09lu]:STOP FULL\n", devteroflexConfig.icount);

    // TODO: When to close FPGA and stop generating helper memory?
    //releaseFPGAContext(&c);
    //qflex_mem_trace_stop();
}

void devteroflex_init(bool enabled, bool run, size_t fpga_physical_pages, int debug_mode, bool pure_singlestep) {
    devteroflexConfig.enabled = enabled;
    devteroflexConfig.running = run;
    devteroflexConfig.debug_mode = debug_mode;
    devteroflexConfig.pure_singlestep = pure_singlestep;
    printf("DevteroFlex settings: enabled[%i]:run[%i]:fpga_phys_pages[%lu]:debug_mode[%i]:pure_singlestep[%i]\n", enabled, run, fpga_physical_pages, debug_mode, pure_singlestep);
    qemu_log("DevteroFlex settings: enabled[%i]:run[%i]:fpga_phys_pages[%lu]:debug_mode[%i]:pure_singlestep[%i]\n", enabled, run, fpga_physical_pages, debug_mode, pure_singlestep);

    if(fpga_physical_pages != -1) {
        if(!pure_singlestep) {
            initFPGAContext(&c);
            assert(fpga_physical_pages == c.dram_size / 4096 && "Number of DRAM pages provided by the FPGAContext must match the input. ");
        }
        if (fpga_paddr_init_manager(fpga_physical_pages, c.ppage_base_addr)) {
            perror("DevteroFlex: Couldn't init the stack for keepign track of free phyiscal pages in the fpga.\n");
            abort();
        }
        // Initialize the inverted page table.
        ipt_init();
        // Initialize the temporal page table.
        tpt_init();
        // Initialize the shadow page table
        spt_init();

        // In this case, the enable signal must be added.
        assert(devteroflexConfig.enabled && "When the page size is specified, you must enable the devteroflex! by adding `enabled=on` to the command options.");
    }
}

void devteroflex_config_fast_forward(uint64_t target) {
    printf("Will fast forward till target: %lu\n", target);
    devteroflexConfig.fast_forward.enabled = true;;
    devteroflexConfig.fast_forward.running = false;
    devteroflexConfig.fast_forward.icount_target = target;
    devteroflexConfig.fast_forward.icount_curr = 0;
}

void devteroflex_icount_update(uint64_t executed) {
    // Fast forward management
    if(devteroflexConfig.enabled) {
        assert(!(devteroflexConfig.running && (devteroflexConfig.fast_forward.enabled && devteroflexConfig.fast_forward.running)));
        if(devteroflexConfig.fast_forward.enabled && devteroflexConfig.fast_forward.running) {
            qemu_log("Devteroflex:fast_forward:exec[%09lu]\n", devteroflexConfig.fast_forward.icount_curr);
            devteroflexConfig.icount += executed;
            devteroflexConfig.fast_forward.icount_curr += executed;
            if(devteroflexConfig.fast_forward.icount_curr > devteroflexConfig.fast_forward.icount_target) {
                qflex_tb_flush();
                devteroflexConfig.fast_forward.running = false;
                devteroflexConfig.fast_forward.enabled = false;
                devteroflexConfig.running = true;
                qflexState.exit_main_loop = true;
                qemu_log("DEVTEROFLEX:fast_forward:done[%010lu]:target[%010lu]\n", devteroflexConfig.fast_forward.icount_curr, devteroflexConfig.fast_forward.icount_target);
            }
        } else if (devteroflexConfig.running) {
            qemu_log("Devteroflex:icount:exec[%09lu]\n", devteroflexConfig.icount);
            devteroflexConfig.icount += executed;
        }
    } else if (!devteroflexConfig.enabled && devteroflexConfig.fast_forward.running) {
        // Counting when running with normal icount but no DevteroFlex attached
        qemu_log("Devteroflex:icount:exec[%09lu]:no devteroflex\n", devteroflexConfig.icount);
        devteroflexConfig.icount += executed;
    }
}
