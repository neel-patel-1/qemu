#include <stdbool.h>
#include <stdio.h>
#include "util/circular-buffer.h"
#include "qflex/qflex.h"
#include "qflex/devteroflex/devteroflex.h"
#include "qflex/devteroflex/demand-paging.h"
#ifdef AWS_FPGA
#include "qflex/devteroflex/aws/fpga_interface.h"
#else
#include "qflex/devteroflex/simulation/fpga_interface.h"
#endif

typedef struct {
    uint32_t bitmap;
    uint64_t ipt_bits[32];
    uint64_t hvp[32];
    uint32_t thid[32];
} BitmapVector;

static BitmapVector pendingEvictions = {.bitmap = 0};
static BitmapVector pendingRequests = {.bitmap = 0};


void evict_notify_pending_add(uint64_t ipt_bits, uint64_t hvp) {
    int entryMask = 0;
    for (int i = 0; i < 32; i++) {
        // Search for free bucket
        entryMask = 1 << i;
        if (!(pendingEvictions.bitmap & entryMask)) {
            // Store pending transaction
            pendingEvictions.ipt_bits[i] = ipt_bits;
            pendingEvictions.hvp[i] = hvp;
            pendingEvictions.bitmap |= entryMask; // Toggle
            break;
        }
    }
}

void evict_notfiy_pending_clear(uint64_t ipt_bits) {
    int entryMask = 0;
    bool matched = false;
    for(int i = 0; i < 32; i++) {
        // Search for free bucket
        entryMask = 1 << i;
        if(pendingEvictions.bitmap & entryMask) {
            if(ipt_bits == pendingEvictions.ipt_bits[i]) {
                pendingEvictions.bitmap &= ~entryMask; // Un-toggle
                matched = true;
                break;
            }
        }
    }
    assert(matched); // Should always have received a notify before evicting
}

bool page_fault_pending_eviction_has_hvp(uint64_t hvp) {
    int mask = 0;
    for(int i = 0; i < 32; i++) {
        mask = 1 << i;
        if ((pendingEvictions.bitmap & mask) &&
            hvp == pendingEvictions.hvp[i]) {
            return true;
        }
    }
    return false;
}

void page_fault_pending_add(uint64_t ipt_bits, uint64_t hvp, uint32_t thid) {
    int mask = 0;
    for(int i = 0; i < 32; i++) {
        mask = 1 << i;
        if(!(pendingRequests.bitmap & mask)) {
            pendingRequests.bitmap |= mask;
            pendingRequests.ipt_bits[i] = ipt_bits;
            pendingRequests.hvp[i] = hvp;
            pendingRequests.thid[i] = thid;
            break;
        }
    }
}


bool page_fault_pending_run(uint64_t hvp) {
    bool has_pending = false;
    int mask = 0;
    for(int i = 0; i < 32; i++) {
        mask = 1 << i;
        if((pendingRequests.bitmap & mask) && 
            hvp == pendingRequests.hvp[i]) {
            has_pending = true;
            pendingRequests.bitmap &= ~mask;
            qemu_log("DevteroFlex:MMU:PA[0x%016lx]:SYNONYM PENDING\n", hvp);
            tpt_add_entry(pendingRequests.ipt_bits[i], pendingRequests.hvp[i]);
            page_fault_return(pendingRequests.ipt_bits[i], pendingRequests.hvp[i], pendingRequests.thid[i]);
        }
    }
    return has_pending;
}

/**
 *  @return true when no synonym is available.
 */
bool insert_entry_get_ppn(uint64_t hvp, uint64_t ipt_bits, uint64_t *ppn) {
    int ret = ipt_add_entry(hvp, ipt_bits);
    if(ret == SYNONYM) {
        return false;
    } else if (ret == PAGE) {
        if(!fpga_paddr_get(ppn)){
            // TODO support host side page evictions
            perror("Ran out of physical pages in the FPGA, not supported forced evictions yet.");
            exit(EXIT_FAILURE);
        }
        return true;
    } else {
        perror("IPT Table error in adding entry");
        exit(EXIT_FAILURE);
    }
}

void devteroflex_synchronize_page(CPUState *cpu, uint64_t vaddr, int type) {
    uint64_t hvp = gva_to_hva(cpu, vaddr, type) & ~PAGE_MASK;
    if(hvp == -1) {
        // vaddr not mapped, no need to synchronize, might be IO
        // Let instruction execute normally
        return;
    }

    uint64_t *synonyms_list_ipt;
    int list_size = ipt_check_synonyms(hvp, &synonyms_list_ipt);
    if(list_size <= 0) {
        // No synonyms detected
        return;
    }

    // Synchronize if it's a store
    // bool synchronize = (type == DATA_STORE);
    bool synchronize = true;
    if(!synchronize) {
        // Search whenever FPGA might have modified page
        for(int entry = 0; entry < list_size; entry++) {
            if(IPT_GET_PERM(synonyms_list_ipt[entry]) == DATA_STORE) {
                synchronize = true;
                break;
            }
        }
    }

    if(!synchronize) {
        // Neither a store, or any of the FPGA pages has store permissions
        qemu_log("QEMU detected page in FPGA and does not require synchronization.\n");
        return;
    } else {
        qemu_log("QEMU detected page in FPGA and requires synchronization.\n");
    }

    
    for(int entry = 0; entry < list_size; entry++) {
        page_eviction_request(synonyms_list_ipt[entry]);
    }
    page_eviction_wait_complete(synonyms_list_ipt, list_size);

    // Assert all synonyms have been evicted
    free(synonyms_list_ipt);
    list_size = ipt_check_synonyms(hvp, &synonyms_list_ipt);
    assert(list_size <= 0); 
}