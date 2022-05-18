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
#include "qflex/qflex-traces.h"
#include "qflex/devteroflex/verification.h"

#include <glib.h>

DevteroflexConfig devteroflexConfig = { 
    .is_debug = false,
    .enabled = false,
    .running = false,
};

static FPGAContext c;
static DevteroflexArchState state;



// List of cpus in the FPGA
static uint32_t running_cpus;
#define cpu_in_fpga(cpu) (running_cpus & 1 << cpu)
#define cpu_push_fpga(cpu) (running_cpus |= 1 << cpu)
#define cpu_pull_fpga(cpu) (running_cpus &= ~(1 << cpu))

// Close result comparison for some cores when they run transplant.
static bool disable_cpu_comparison = false;


static bool run_debug(CPUState *cpu) {
    if (FLAGS_GET_IS_EXCEPTION(state.flags) | FLAGS_GET_IS_UNDEF(state.flags)) {
        // Singlestep like normal execution
        return false;
    } 
    qemu_log("DevteroFlex:CPU[%i]:Running debug check\n", cpu->cpu_index);
    // Singlestep and compare
    uint64_t pc_before_singlestep = QFLEX_GET_ARCH(pc)(cpu);
    qflex_singlestep(cpu);
    // continue until supervised instructions are runned.
    while(QFLEX_GET_ARCH(el)(cpu) != 0){
        qflex_singlestep(cpu);
        if(QFLEX_GET_ARCH(pc)(cpu) == pc_before_singlestep) {
            qflex_singlestep(cpu);
        }
    }

    if(devteroflex_compare_archstate(cpu, &state)) {
        // Dangerous!!!
        qemu_log("WARNING:DevteroFlex:CPU[%i]:An architecture state mismatch has been detected. Quitting QEMU now. \n", cpu->cpu_index);
        abort();
    }
    if(devteroflex_is_running()) {
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
    if(devteroflexConfig.is_debug) {
        if(run_debug(cpu)) {
            // Succesfully ran the debug routine
            return; 
        }
        // Need to run normal transplant
    } else {
        devteroflex_unpack_archstate(cpu, &state);
    }

    disable_cpu_comparison = true; // Disable comparison, because these instructions are not executed on DevteroFlex.
    // Execute the exception instruction
    uint64_t pc_before_singlestep = QFLEX_GET_ARCH(pc)(cpu);
    qflex_singlestep(cpu);
    // continue until supervised instructions are runned.
    while(QFLEX_GET_ARCH(el)(cpu) != 0){
        qflex_singlestep(cpu);
        if(QFLEX_GET_ARCH(pc)(cpu) == pc_before_singlestep) {
            qflex_singlestep(cpu);
        }
    }
 
    disable_cpu_comparison = false;

    // handle exception will change the state, so it
    if(devteroflex_is_running()) {
        cpu_push_fpga(cpu->cpu_index);
        devteroflex_pack_archstate(&state, cpu);
        ipt_register_asid(state.asid, QFLEX_GET_ARCH(asid_reg)(cpu));

        if(devteroflexConfig.is_debug) {
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
            if(devteroflexConfig.is_debug){
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

        if(devteroflexConfig.is_debug) {
            transplantPushAndSinglestep(&c, cpu->cpu_index, &state);
        } else {
            transplantPushAndStart(&c, cpu->cpu_index, &state);
        }
    }
}

/* @return true in case the message has completely evicted, 
 *         false in case we are waiting for clearing eviction.
 */
static bool handle_evict_notify(MessageFPGA *message) {
    uint64_t gva = message->vpn << 12;
    uint64_t perm = message->EvictNotif.permission;
    uint32_t asid = message->asid;
    // uint64_t ppn = message->EvictNotif.ppn << 12;
    // bool modified = message->EvictNotif.modified;

    uint64_t ipt_bits = IPT_COMPRESS(gva, asid, perm);
    uint64_t hvp = tpt_lookup(ipt_bits);

    qemu_log("DevteroFlex:MMU:ASID[%x]:VA[0x%016lx]:PERM[%lu]:EVICT\n", asid, gva, perm);
    evict_notify_pending_add(ipt_bits, hvp);
    return false;
}

static uint8_t page_buffer[PAGE_SIZE];
static void handle_evict_writeback(MessageFPGA * message) {
    uint64_t gvp = message->vpn << 12;
    uint64_t perm = message->EvictNotif.permission;
    uint32_t asid = message->asid;
    uint64_t ppn = message->EvictNotif.ppn << 12;

    uint64_t ipt_bits = IPT_COMPRESS(gvp, asid, perm);

    uint64_t hvp = tpt_lookup(ipt_bits);
 
    qemu_log("DevteroFlex:MMU:ASID[%x]:VA[0x%016lx]:PERM[%lu]:WRITE BACK\n", asid, gvp, perm);
    if(devteroflexConfig.is_debug) {
        if(!disable_cpu_comparison){
            // Compare DevteroFlex modified page with QEMU
            dramPagePull(&c, ppn, (void *)&page_buffer);
            uint8_t *page_in_qemu = (uint8_t *)hvp;
            bool mismatched = false;
            for (int i = 0; i < PAGE_SIZE; ++i) {
                if(page_in_qemu[i] != page_buffer[i]){
                    qemu_log("BYTE[%d]:QEMU[%x] =/= FPGA[%x] \n", i, page_in_qemu[i], page_buffer[i]);
                    mismatched = true;
                }
            }
            if(mismatched) {
                qemu_log("ERROR:Page mismatch\n");
                abort();
            }
        } else {
            // simply do nothing. 
            // However, we should never write the FPGA.
        }
    }

    ipt_evict(hvp, ipt_bits);
    // We have to check whether there are other synonyms referring that page.
    uint64_t *ipt_chain;
    if(ipt_check_synonyms(hvp, &ipt_chain) != 0){
        free(ipt_chain);
    } else {
        // no other synonyms is mapped to that page. We can delete the page from FPGA.
        fpga_paddr_push(ppn);
        spt_remove_entry(hvp);
        if(!devteroflexConfig.is_debug){
            // We can fetch the page now.
            dramPagePull(&c, ppn, (void *) hvp);
        }
    }
    page_fault_pending_run(hvp);
    evict_notfiy_pending_clear(ipt_bits);
    tpt_remove_entry(ipt_bits);
}

static void handle_page_fault(MessageFPGA *message) {
    uint64_t gvp = message->vpn << 12;
    uint64_t perm = message->PageFaultNotif.permission;
    uint32_t thid = message->PageFaultNotif.thid;
    uint32_t asid = message->asid;

    CPUState *cpu = qemu_get_cpu(thid);
    // find the highest permission of a data page
    uint64_t hvp = -1;
    if (perm == MMU_DATA_LOAD) {
        hvp = gva_to_hva(cpu, gvp, MMU_DATA_STORE);
        if(hvp == -1) 
            hvp = gva_to_hva(cpu, gvp, MMU_DATA_LOAD);
        else
            perm = MMU_DATA_STORE; // give R/W permission
    } else {
        hvp = gva_to_hva(cpu, gvp, perm);
    }
    uint64_t ipt_bits = IPT_COMPRESS(gvp, asid, perm);
    qemu_log("DevteroFlex:MMU:CPU[%i]:ASID[%x]:VA[0x%016lx]:PERM[%lu]:PAGE FAULT\n", thid, asid, gvp, perm);
    if(hvp == -1) {
        qemu_log("   ---- PAGE FAULT translation miss, request transplant\n");
        transplantForceTransplant(&c, thid);
        return;
    }

    if(page_fault_pending_eviction_has_hvp(hvp)) {
        // FPGA is evicting that hvp, wait till completed before handling the page
        qemu_log("   ---- PAGE FAULT SYNONYM: Address matched pending evicted physical page, wait for synonym to complete writeback\n");
        page_fault_pending_add(ipt_bits, hvp, thid);
    } else {
        // now this page is pushed to the FPGA, we also put the mapping in the tpt.
        tpt_add_entry(ipt_bits, hvp);
        page_fault_return(ipt_bits, hvp, thid);
        bool has_pending = page_fault_pending_run(hvp);
        assert(!has_pending);
    }
}

static bool message_has_pending(MessageFPGA *msg) {
    if(mmuMsgHasPending(&c)) {
        mmuMsgGet(&c, msg);
        return true;
    }
    return false;
}

static void message_run(MessageFPGA message) {
    switch (message.type)
    {
    case sPageFaultNotify:
        handle_page_fault(&message);
        break;
    case sEvictNotify:
        handle_evict_notify(&message);
        break;
    case sEvictDone:
        handle_evict_writeback(&message);
        break;
    default:
        perror("Message type received by FPGA doesn't match any of the existing types.\n");
        abort();
        break;
    }
}

void page_fault_return(uint64_t ipt_bits, uint64_t hvp, uint32_t thid) {
    uint64_t gvpa = IPT_GET_VA(ipt_bits);
    uint64_t asid = IPT_GET_ASID(ipt_bits);
    uint32_t perm = IPT_GET_PERM(ipt_bits);
    uint64_t ppa = -1; // physical page address
    bool pushPage = insert_entry_get_ppn(hvp, ipt_bits, &ppa);
    if(pushPage) {
        // No synonym
        dramPagePush(&c, ppa, (void*) hvp);
    }
    qemu_log("DevteroFlex:MMU:ASID[%lx]:GVA[0x%016lx]:HVA[0x%016lx]:PPN[0x%08lx]:PERM[%u]:PAGE FAULT RESPONSE\n", asid, gvpa, hvp, ppa, perm);

    MessageFPGA missReply;
    makeMissReply(perm, thid, asid, gvpa, ppa, &missReply);
    mmuMsgSend(&c, &missReply);
}

void page_eviction_request(uint64_t ipt_bits) {
    uint64_t gvp = IPT_GET_VA(ipt_bits);
    uint64_t asid = IPT_GET_ASID(ipt_bits);

    qemu_log("DevteroFlex:MMU:ASID[%lx]:VA[0x%016lx]:PAGE FORCE EVICTION\n", asid, gvp);
    MessageFPGA evictRequest;
    makeEvictRequest(asid, gvp, &evictRequest);
    mmuMsgSend(&c, &evictRequest);
}

static MessageFPGA message_buffer[256] = {0};
static uint64_t message_buffer_curr_entry = 0;

void page_eviction_wait_complete(uint64_t *ipt_list, int count) {
    MessageFPGA msg;
    bool matched = false;
    int left = count;
    while(left > 0) {
        if(message_has_pending(&msg)) {
            uint32_t asid = msg.asid;
            uint64_t gvp = msg.vpn << 12;
            matched = false;
            for (int entry = 0; entry < count; entry++) {
                uint64_t ipt_bits = ipt_list[entry];
                uint64_t entry_gvp = IPT_GET_VA(ipt_bits);
                uint32_t entry_asid = IPT_GET_ASID(ipt_bits);
                if(entry_gvp == gvp && entry_asid == asid) {
                    matched = true;
                    // This message is one of the messages we were waiting for
                    if(msg.type == sEvictNotify) {
                        handle_evict_notify(&msg);
                    } else if (msg.type == sEvictDone) {
                        handle_evict_writeback(&msg);
                        left--;
                    } else {
                        perror("DevteroFlex: Message should have been an evict response while synchronizing page\n.");
                        abort();
                    }

                    break; // Break search for matching entry
                }
            }
            if(!matched) {
                // Buffer messages that do not concern page synchronization.
                // We do this to serialise requests and remove potential dependencies,
                // we delay all messages till the synchronization is completed
                message_buffer[message_buffer_curr_entry] = msg;
                message_buffer_curr_entry++;
                if(message_buffer_curr_entry>256) {
                    perror("DevteroFlex: Ran out of message entries.\n");
                    abort();
                }
            }
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
            if(devteroflex_is_running()) {
                transplantsRun(pending);
            } else {
                transplantBringBack(pending);
            }
        }
        // Run all pending messages from a synchronization
        if(message_buffer_curr_entry > 0) {
            for(int message_idx = 0; message_idx < message_buffer_curr_entry; message_idx++) {
                message_run(message_buffer[message_idx]);
            }
            message_buffer_curr_entry = 0;
        }
        // Check and run all pending messages
        while(message_has_pending(&msg)) {
            message_run(msg);
        }

        // If DevteroFlex stopped executing, pull all cpu's back
        if(!devteroflex_is_running()) {
            CPU_FOREACH(cpu) {
                if(!cpu_in_fpga(cpu->cpu_index)) {
                    transplantStopCPU(&c, cpu->cpu_index);
                }
            }
        }
        if(!devteroflex_is_running() && running_cpus == 0) {
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
    while(1) {
        CPU_FOREACH(cpu) {
            qflex_singlestep(cpu);
        }
        if(!devteroflex_is_running()) {
            break;
        }
    }

    return 0;
}

static void devteroflex_prepare_singlestepping(void) {
    CPUState *cpu;
    qflex_update_exit_main_loop(false);
    qflex_singlestep_start();
    qflex_update_skip_interrupts(true);
    qemu_loglevel |= CPU_LOG_INT; 
    qflex_mem_trace_start(-1, -1);
    CPU_FOREACH(cpu) {
        cpu_single_step(cpu, SSTEP_ENABLE);
        qatomic_mb_set(&cpu->exit_request, 0);
    }
}

int devteroflex_singlestepping_flow(void) {
    qemu_log("DEVTEROFLEX: FPGA START\n");
    qflexState.log_inst = true;
    devteroflex_prepare_singlestepping();
    if(!devteroflexConfig.pure_singlestep) {
        devteroflex_execution_flow();
    } else {
        qflex_singlestep_flow();
    }
    qemu_log("DEVTEROFLEX: FPGA EXIT\n");
    qflexState.log_inst = false;
    devteroflex_stop_full();
    return 0;
}

void devteroflex_stop_full(void) {
    CPUState *cpu;
    qflex_singlestep_stop();
    qflex_update_skip_interrupts(false);
    CPU_FOREACH(cpu) {
        cpu_single_step(cpu, 0);
    }
    qemu_loglevel &= ~CPU_LOG_TB_IN_ASM;
    qemu_loglevel &= ~CPU_LOG_INT;
    qemu_log("DEVTEROFLEX: Stopped fully\n");

    // TODO: When to close FPGA and stop generating helper memory?
    //releaseFPGAContext(&c);
    //qflex_mem_trace_stop();
}

void devteroflex_init(bool enabled, bool run, size_t fpga_physical_pages, bool is_debug, bool pure_singlestep) {
    devteroflexConfig.enabled = enabled;
    devteroflexConfig.running = run;
    devteroflexConfig.is_debug = is_debug;
    devteroflexConfig.pure_singlestep = pure_singlestep;

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
