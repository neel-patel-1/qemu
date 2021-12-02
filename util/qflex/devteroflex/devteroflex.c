#include <stdbool.h>

#include "qemu/osdep.h"
#include "qemu/thread.h"
#include "hw/core/cpu.h"

#include "qflex/qflex.h"
#include "qflex/qflex-arch.h"

#include "qflex/devteroflex/devteroflex.h"
#include "qflex/devteroflex/demand-paging.h"
#include "qflex/qflex-traces.h"

#ifdef AWS_FPGA
#include "qflex/devteroflex/aws/fpga.h"
#include "qflex/devteroflex/aws/fpga_interface.h"
#else
#include "qflex/devteroflex/simulation/fpga.h"
#include "qflex/devteroflex/simulation/fpga_interface.h"
#endif

DevteroflexConfig devteroflexConfig = { false, false };
static FPGAContext c;
static DevteroflexArchState state;

// List of cpus in the FPGA
static uint32_t running_cpus;
#define cpu_in_fpga(cpu) (running_cpus & 1 << cpu)
#define cpu_push_fpga(cpu) (running_cpus |= 1 << cpu)
#define cpu_pull_fpga(cpu) (running_cpus &= ~(1 << cpu))

static void run_transplant(CPUState *cpu, uint32_t thread) {
    assert(cpu->cpu_index == thread);
    assert(cpu_in_fpga(thread));
    cpu_pull_fpga(cpu->cpu_index);
    transplant_getState(&c, thread, (uint64_t *) &state, DEVTEROFLEX_TOT_REGS);
    devteroflex_unpack_archstate(cpu, &state);
    if(devteroflex_is_running()) {
        qflex_singlestep(cpu);
        // Singlestepping might update DevteroFlex
        if(devteroflex_is_running()) {
            cpu_push_fpga(cpu->cpu_index);
            register_asid(QFLEX_GET_ARCH(asid)(cpu), QFLEX_GET_ARCH(asid_reg)(cpu));
            devteroflex_pack_archstate(&state, cpu);
            transplant_pushState(&c, thread, (uint64_t *) &state, DEVTEROFLEX_TOT_REGS);
            transplant_start(&c, thread);
        }
    }
}

static void transplants_run(uint32_t pending) {
    CPUState *cpu;
    CPU_FOREACH(cpu)
    {
        if(pending & (1 << cpu->cpu_index)) {
            run_transplant(cpu, cpu->cpu_index);
        }
    }
}

static void transplant_push_all_cpus(void) {
    CPUState *cpu;
    CPU_FOREACH(cpu) {
        cpu_push_fpga(cpu->cpu_index);
        registerThreadWithProcess(&c, cpu->cpu_index, QFLEX_GET_ARCH(asid)(cpu));
        register_asid(QFLEX_GET_ARCH(asid)(cpu), QFLEX_GET_ARCH(asid_reg)(cpu));
        devteroflex_pack_archstate(&state, cpu);
        transplant_pushState(&c, cpu->cpu_index, (uint64_t *)&state, DEVTEROFLEX_TOT_REGS);
        transplant_start(&c, cpu->cpu_index);
    }
}

static void transplant_pull_all_cpus(void) {
    CPUState *cpu;
    CPU_FOREACH(cpu) {
        // transplant_stop(c) // TODO: Enable mechanism to stop execution in the FPGA and request state back
        if(!cpu_in_fpga(cpu->cpu_index)) {
            qemu_log("DevteroFlex: WARNING: Forcing transplants back doesn't stop execution yet.\n");
            cpu_pull_fpga(cpu->cpu_index);
            transplant_getState(&c, cpu->cpu_index, (uint64_t *) &state, DEVTEROFLEX_TOT_REGS);
            devteroflex_unpack_archstate(cpu, &state);
        }
    }
}

/* @return true in case the message has completely evicted, 
 *         false in case we are waiting for clearing eviction.
 */
static bool handle_evict_notify(MessageFPGA *message) {
    uint64_t gvp = IPT_ASSEMBLE_64(message->vpn_hi, message->vpn_lo) & PAGE_MASK;
    uint64_t perm = message->EvictNotif.permission;
    uint32_t asid = message->asid;
    uint64_t ppn = message->EvictNotif.ppn;
    bool modified = message->EvictNotif.modified;

    uint64_t ipt_bits = IPT_COMPRESS(gvp, asid, perm);

    uint64_t hvp = page_table_get_hvp(ipt_bits, perm);
    if(modified) {
        // Wait for EvictDone, eviction underprogress
        evict_notify_pending_add(ipt_bits, hvp);
        return false;
    } else {
        // Evict entry directly as there wont be any future evict message
        ipt_evict(hvp, ipt_bits);
        fpga_paddr_push(ppn);
        return true;
    }
}

static void handle_evict_writeback(MessageFPGA * message) {
    uint64_t gvp = IPT_ASSEMBLE_64(message->vpn_hi, message->vpn_lo) & PAGE_MASK;
    uint64_t perm = message->EvictNotif.permission;
    uint32_t asid = message->asid;
    uint64_t ppn = message->EvictNotif.ppn;
    assert(message->EvictNotif.modified); // No writeback notif should have not modified flag

    uint64_t ipt_bits = IPT_COMPRESS(gvp, asid, perm);

    uint64_t hvp = page_table_get_hvp(ipt_bits, perm);
 
    fetchPageFromFPGA(&c, ppn, (void *) hvp);
    page_fault_pending_run(hvp);
    evict_notfiy_pending_clear(ipt_bits);
    ipt_evict(hvp, ipt_bits);
    fpga_paddr_push(ppn);
}

static void handle_page_fault(MessageFPGA *message) {
    uint64_t gvp = IPT_ASSEMBLE_64(message->vpn_hi, message->vpn_lo) << 12;
    uint64_t perm = message->PageFaultNotif.permission;
    uint32_t thid = message->PageFaultNotif.thid;
    uint32_t asid = message->asid;

    CPUState *cpu = qemu_get_cpu(thid);
    uint64_t hvp = gva_to_hva(cpu, gvp, message->PageFaultNotif.permission);
    uint64_t ipt_bits = IPT_COMPRESS(gvp, asid, perm);
    if(hvp == -1) {
        qemu_log("DevteroFlex:thid[%i]:asid[%"PRIx32"]:addr[0x%"PRIx64"]:page fault translation miss.\n", thid, asid, gvp);
        run_transplant(cpu, thid);
        return;
    }

    if(page_fault_pending_eviction_has_hvp(hvp)) {
        // FPGA is evicting that hvp, wait till completed before handling the page
        qemu_log("DevteroFlex: Page Fault address matched currently evicting phyisical page.\n");
        page_fault_pending_add(ipt_bits, hvp, thid);
    } else {
        page_fault_return(ipt_bits, hvp, thid);
        bool has_pending = page_fault_pending_run(hvp);
        assert(!has_pending);
    }
}

static bool message_has_pending(MessageFPGA *msg) {
    if(hasMessagePending(&c)) {
        getMessagePending(&c, (uint8_t *) msg);
        return true;
    }
    return false;
}

static bool transplants_has_pending(uint32_t *pending) {
    transplant_pending(&c, pending);
    return (pending != 0);
}

static void message_run(MessageFPGA message) {
    switch (message.type)
    {
    case sPageFault:
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
        exit(EXIT_FAILURE);
        break;
    }
}

void page_fault_return(uint64_t ipt_bits, uint64_t hvp, uint32_t thid) {
    uint64_t gvp = IPT_GET_VA(ipt_bits);
    uint64_t asid = IPT_GET_ASID(ipt_bits);
    uint32_t perm = IPT_GET_PERM(ipt_bits);
    uint64_t ppn = -1;
    bool pushPage = insert_entry_get_ppn(hvp, ipt_bits, &ppn);
    if(pushPage) {
        // No synonym
        pushPageToFPGA(&c, ppn, (void*) hvp);
    }

    MessageFPGA missReply;
    makeMissReply(perm, thid, asid, gvp, ppn, &missReply);
    sendMessageToFPGA(&c, &missReply, sizeof(MessageFPGA));
}

void page_eviction_request(uint64_t ipt_bits) {
    uint64_t gvp = IPT_GET_VA(ipt_bits);
    uint64_t asid = IPT_GET_ASID(ipt_bits);

    MessageFPGA evictRequest;
    makeEvictRequest(asid, gvp, &evictRequest);
    sendMessageToFPGA(&c, &evictRequest, sizeof(MessageFPGA));
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
            uint64_t gvp = IPT_ASSEMBLE_64(msg.vpn_hi, msg.vpn_lo) & PAGE_MASK;
            matched = false;
            for (int entry = 0; entry < count; entry++) {
                uint64_t ipt_bits = ipt_list[entry];
                if(IPT_GET_VA(ipt_bits) == gvp && IPT_GET_ASID(ipt_bits) == asid) {
                    matched = true;
                    // This message is one of the messages we were waiting for
                    if(msg.type == sEvictNotify) {
                        bool noWriteback = handle_evict_notify(&msg);
                        if (noWriteback) { 
                            left--;
                        }
                    } else if (msg.type == sEvictDone) {
                        handle_evict_writeback(&msg);
                        left--;
                    } else {
                        perror("DevteroFlex: Message should have been an evict response while synchronizing page\n.");
                        exit(EXIT_FAILURE);
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
                    exit(EXIT_FAILURE);
                }
            }
        }
    }
}

int devteroflex_execution_flow(void) {
    MessageFPGA msg;
    uint32_t pending;
    initFPGAContext(&c);
    CPUState *cpu;
    CPU_FOREACH(cpu) { }
    transplant_push_all_cpus();
    while(1) {
       // Check and run all pending transplants
       if(transplants_has_pending(&pending)) {
           transplants_run(pending);
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
           transplant_pull_all_cpus();
           break;
       }
    }

    releaseFPGAContext(&c);
    return 0;
}

int devteroflex_singlestepping_flow(void) {
    CPUState *cpu;
    // Preapre DevteroFlex execution flow
    qflex_update_exit_main_loop(false);
    qemu_log("DEVTEROFLEX: START\n");
    qflex_singlestep_start();
    qflex_update_skip_interrupts(true);
    qflex_mem_trace_start(-1, -1);
    CPU_FOREACH(cpu) {
        cpu_single_step(cpu, SSTEP_ENABLE | SSTEP_NOIRQ | SSTEP_NOTIMER);
        qatomic_mb_set(&cpu->exit_request, 0);
    }

    devteroflex_execution_flow();

    // Prepare to rexecute normally
    qflex_singlestep_stop();
    qflex_update_skip_interrupts(false);
    qflex_mem_trace_stop();
    CPU_FOREACH(cpu) {
        cpu_single_step(cpu, 0);
    }
    qemu_log("DEVTEROFLEX: EXIT\n");
    return 0;
}

void devteroflex_init(bool enabled, bool run, size_t fpga_physical_pages) {
    devteroflexConfig.enabled = enabled;
    devteroflexConfig.running = run;
    if(fpga_physical_pages != -1) {
        if (fpga_paddr_init_manager(fpga_physical_pages)) {
            perror("Couldn't init the stack for keepign track of free phyiscal pages in the fpga.\n");
            exit(EXIT_FAILURE);
        }
    }
}
