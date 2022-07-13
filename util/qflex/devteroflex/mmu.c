#include <stdbool.h>
#include <stdio.h>

#include "qemu/osdep.h"
#include "qemu/thread.h"
#include "hw/core/cpu.h"

#include "qflex/qflex.h"
#include "qflex/devteroflex/devteroflex.h"
#include "qflex/devteroflex/devteroflex-mmu.h"
#include "qflex/devteroflex/demand-paging.h"

static uint8_t page_buffer[PAGE_SIZE];
void mmu_message_run(MessageFPGA message) {
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

bool handle_evict_notify(MessageFPGA *message) {
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

void handle_evict_writeback(MessageFPGA* message) {
    uint64_t gvp = message->vpn << 12;
    uint64_t perm = message->EvictNotif.permission;
    uint32_t asid = message->asid;
    uint64_t ppn = ((uint64_t) message->EvictNotif.ppn) << 12;

    uint64_t ipt_bits = IPT_COMPRESS(gvp, asid, perm);

    uint64_t hvp = tpt_lookup(ipt_bits);
 
    qemu_log("DevteroFlex:MMU:ASID[%x]:VA[0x%016lx]:PERM[%lu]:PPN[%08lx]:WRITE BACK\n", asid, gvp, perm, ppn);
    if(debug_cmp_mem_sync() || debug_cmp_no_mem_sync()) {
        assert(!(devteroflexConfig.debug_mode == no_mem_sync && devteroflexConfig.transplant_type == TRANS_DEBUG));
        qemu_log("      - Comparing page\n");
        // Compare DevteroFlex modified page with QEMU
        dramPagePull(&c, ppn, (void *)&page_buffer);
        uint8_t *page_in_qemu = (uint8_t *) hvp;
        bool mismatched = false;
        for (int i = 0; i < PAGE_SIZE; ++i) {
            if(page_in_qemu[i] != page_buffer[i]) {
                qemu_log("BYTE[%d]:QEMU[%x] =/= FPGA[%x] \n", i, page_in_qemu[i], page_buffer[i]);
                mismatched = true;
            }
        }
        if(mismatched) {
            qemu_log("ERROR:Page mismatch\n");
            qflex_dump_archstate_log(qemu_get_cpu(0)); // TODO Get correct PC in care of multithreading
            abort();
        }
    }

    ipt_evict(hvp, ipt_bits);
    // We have to check whether there are other synonyms referring that page.
    uint64_t *ipt_chain;
    if(ipt_check_synonyms(hvp, &ipt_chain) != 0) {
        free(ipt_chain);
    } else {
        // no other synonyms is mapped to that page. We can delete the page from FPGA.
        fpga_paddr_push(ppn);
        spt_remove_entry(hvp);
        if(!devteroflexConfig.debug_mode){
            // We can fetch the page now.
            dramPagePull(&c, ppn, (void *) hvp);
        }
    }
    page_fault_pending_run(hvp);
    evict_notfiy_pending_clear(ipt_bits);
    tpt_remove_entry(ipt_bits);
}

void handle_page_fault(MessageFPGA *message) {
    uint64_t gvp = message->vpn << 12;
    uint64_t perm = message->PageFaultNotif.permission;
    uint32_t thid = message->PageFaultNotif.thid;
    uint32_t asid = message->asid;

    CPUState *cpu = qemu_get_cpu(thid);
    // find the highest permission of a data page
    uint64_t hvp = -1;
    if(perm == MMU_INST_FETCH) {
      perm = MMU_INST_FETCH;
      hvp = gva_to_hva(cpu, gvp, MMU_INST_FETCH);
    } else if (perm == MMU_DATA_STORE) {
      perm = MMU_DATA_STORE;
      hvp = gva_to_hva(cpu, gvp, MMU_DATA_STORE);
    } else {
      perm = MMU_DATA_STORE; // give R/W permission
      hvp = gva_to_hva(cpu, gvp, MMU_DATA_STORE);
      if (hvp == -1) {
        perm = MMU_INST_FETCH; // give R/X permission
        hvp = gva_to_hva(cpu, gvp, MMU_INST_FETCH);
        if (hvp == -1) {
          perm = MMU_DATA_LOAD; // give R permission
          hvp = gva_to_hva(cpu, gvp, MMU_DATA_LOAD);
        }
      }
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
        send_page_fault_return(ipt_bits, hvp, thid);
        bool has_pending = page_fault_pending_run(hvp);
        assert(!has_pending);
    }
}

void send_page_fault_return(uint64_t ipt_bits, uint64_t hvp, uint32_t thid) {
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

void send_page_evict_req(uint64_t ipt_bits, bool flush_instruction_cache) {
    uint64_t gvp = IPT_GET_VA(ipt_bits);
    uint64_t asid = IPT_GET_ASID(ipt_bits);

    qemu_log("DevteroFlex:MMU:ASID[%lx]:VA[0x%016lx]:PAGE FORCE EVICTION\n", asid, gvp);
    MessageFPGA evictRequest;
    makeEvictRequest(asid, gvp, flush_instruction_cache, true, &evictRequest);
    mmuMsgSend(&c, &evictRequest);
}

bool mmu_has_pending(MessageFPGA *msg) {
    if(mmuMsgHasPending(&c)) {
        mmuMsgGet(&c, msg);
        return true;
    }
    return false;
}

void wait_evict_req_complete(uint64_t *ipt_list, int count) {
    MessageFPGA msg;
    bool matched = false;
    int left = count;
    while(left > 0) {
        if(mmu_has_pending(&msg)) {
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

void devteroflex_mmu_flush_by_va_asid(uint64_t va, uint64_t asid) {
  if(!devteroflexConfig.enabled) return;

  qemu_log("Devteroflex:QEMU MMU:FLUSH:ASID[%lx]:VA[%016lx]\n", asid, va);
  for(int i = 0; i < 3; ++i){
    uint64_t to_evicted = IPT_COMPRESS(va, asid, i);
    if(tpt_is_entry_exists(to_evicted)) {
        // do simple page eviction.
        // both TLBs are shotdown because the TLB flushing is rare.
      send_page_evict_req(to_evicted, true);
      wait_evict_req_complete(&to_evicted, 1);

      break;
    }
  }
}

void devteroflex_mmu_flush_by_asid(uint64_t asid) {
  if(!devteroflexConfig.enabled) return;

  qemu_log("Devteroflex:QEMU MMU:FLUSH:ASID[%lx]\n", asid);
  // 1. get all entries in the tpt.
  uint64_t ele = 0;
  uint64_t *keys = tpt_all_keys(&ele);

  if(ele == 0){
    return;
  }

  uint64_t number_of_match = 0;
  // 2. query the number of matches.
  for(int i = 0; i < ele; ++i){
    if(IPT_GET_ASID(keys[i]) == asid) {
      number_of_match++;
    }
  }

  if(number_of_match == 0){
    free(keys);
    return;
  }

  // 3. allocate memory to keep them.
  uint64_t *matched = calloc(number_of_match, sizeof(uint64_t));
  uint64_t matched_key_index = 0;
  for(int i = 0; i < ele; ++i){
    if(IPT_GET_ASID(keys[i]) == asid) {
      matched[matched_key_index++] = keys[i];
    }
  }

  free(keys);

  // 4. do flushing.
  for(int i = 0; i < number_of_match; i++) {
      send_page_evict_req(matched[i], true);
      wait_evict_req_complete(&matched[i], 1);
  }

  free(matched);
}

void devteroflex_mmu_flush_by_va(uint64_t va) {
  if(!devteroflexConfig.enabled) return;

  qemu_log("Devteroflex:QEMU MMU:TLB FLUSH:VA[%016lx]\n", va);
  // 1. get all entries in the tpt.
  uint64_t ele = 0;
  uint64_t *keys = tpt_all_keys(&ele);

  if(ele == 0){
    return;
  }

  uint64_t number_of_match = 0;
  // 2. query the number of matches.
  for(int i = 0; i < ele; ++i){
    if(IPT_GET_VA(keys[i]) == va) {
      number_of_match++;
    }
  }

  if(number_of_match == 0){
    free(keys);
    return;
  }

  // 3. allocate memory to keep them.
  uint64_t *matched = calloc(number_of_match, sizeof(uint64_t));
  uint64_t matched_key_index = 0;
  for(int i = 0; i < ele; ++i){
    if(IPT_GET_VA(keys[i]) == va) {
      matched[matched_key_index++] = keys[i];
    }
  }

  free(keys);

  // 4. do flushing.
  for(int i = 0; i < number_of_match; i++) {
      send_page_evict_req(matched[i], true);
      wait_evict_req_complete(&matched[i], 1);
  }
  
  free(matched);
}

void devteroflex_mmu_flush_by_hva_asid(uint64_t hva, uint64_t asid) {
  if(!devteroflexConfig.enabled) return;

  qemu_log("Devteroflex:QEMU MMU:TLB FLUSH:ASID[%016lx]:HVA[%016lx]\n", asid, hva);

  // 1. query the IPT.
  uint64_t *ipt_synonyms;
  uint64_t ele = ipt_check_synonyms(hva, &ipt_synonyms);

  if(ele == 0){
    return;
  }

  uint64_t number_of_match = 0;
  // 2. filter
  for(int i = 0; i < ele; ++i){
    if(IPT_GET_ASID(ipt_synonyms[i]) == asid) {
      number_of_match++;
    }
  }

  if(number_of_match == 0){
    free(ipt_synonyms);
    return;
  }

  // 3. allocate memory to keep them.
  uint64_t *matched = calloc(number_of_match, sizeof(uint64_t));
  uint64_t matched_key_index = 0;
  for(int i = 0; i < ele; ++i){
    if(IPT_GET_ASID(ipt_synonyms[i]) == asid) {
      matched[matched_key_index++] = ipt_synonyms[i];
    }
  }

  free(ipt_synonyms);

  // 4. do flushing.
  for(int i = 0; i < number_of_match; i++) {
      send_page_evict_req(matched[i], true);
      wait_evict_req_complete(&matched[i], 1);
  }

  free(matched);
}


void devteroflex_mmu_flush_by_hva(uint64_t hva) {
  if(!devteroflexConfig.enabled) return;

  qemu_log("Devteroflex:QEMU MMU:TLB FLUSH:HVA[%016lx]\n", hva);

  // 1. query the IPT.
  uint64_t *ipt_synonyms;
  uint64_t ele = ipt_check_synonyms(hva, &ipt_synonyms);

  if(ele == 0){
    return;
  }

  // 4. do flushing.
  for(int i = 0; i < ele; i++) {
      send_page_evict_req(ipt_synonyms[i], true);
      wait_evict_req_complete(&ipt_synonyms[i], 1);
  }

  free(ipt_synonyms);
}


void devteroflex_mmu_flush_all(void) {
  if(!devteroflexConfig.enabled) return;

  qemu_log("Devteroflex:QEMU MMU:TLB FLUSH:ALL\n");

  // 1. get all entries in the tpt.
  uint64_t ele = 0;
  uint64_t *keys = tpt_all_keys(&ele);

  if(ele == 0){
    return;
  }

  // 4. do flushing.
  for(int i = 0; i < ele; i++) {
      send_page_evict_req(keys[i], true);
      wait_evict_req_complete(&keys[i], 1);
  }

  free(keys);
}
