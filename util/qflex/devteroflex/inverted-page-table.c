#include <stdbool.h>
#include "util/circular-buffer.h"
#include "qflex/qflex.h"
#include "qflex/devteroflex/devteroflex.h"
#include "qflex/devteroflex/demand-paging.h"
#ifdef AWS_FPGA
#include "qflex/devteroflex/aws/fpga_interface.h"
#else
#include "qflex/devteroflex/simulation/fpga_interface.h"
#endif

typedef struct IPTGvpList {
    uint64_t ipt_bits;    // PID, Guest VA, Permission
    struct IPTGvpList *next; // Extra synonyms list
} IPTGvpList;

typedef struct IPTHvp {
    int           cnt;   // Count of elements in IPTGvpList
    uint64_t      hvp;   // Host Virtual Address (HVP)
    IPTGvpList    *head; // GVP mapping to this HVP present in the FPGA
    struct IPTHvp *next; // In case multiple Host VA hash to same spot
} IPTHvp;

typedef IPTHvp *    IPTHvpPtr;

typedef struct InvertedPageTable {
    IPTHvpPtr *entries;
} InvertedPageTable;

#define NB_ENTRIES (1 << 10)
static inline size_t IPT_hash(uint64_t hvp) {
    return (hvp % NB_ENTRIES);
}

static InvertedPageTable ipt;

static void IPTGvpList_free(IPTGvpList **base) {
    IPTGvpList *curr = *base;
    while(curr) {
        free(curr);
        curr = curr->next;
    };
}

static void IPTHvp_evict(IPTHvp **entry, IPTHvpPtr *entryPtr) {
    IPTGvpList_free(&((*entry)->head));
    *entryPtr = (*entry)->next;
    free(*entry);
}

static void IPTGvpList_insert(IPTHvp *entry, uint64_t ipt_bits) {
    IPTGvpList **ptr = &entry->head;
    while(*ptr) {
        // Go to last entry
        ptr = &(*ptr)->next;	
    }
    // Allocate entry
    *ptr = malloc(sizeof(IPTGvpList));
    (*ptr)->ipt_bits = ipt_bits;
}

static void IPTGvpList_get_chain(IPTHvp *entry, uint64_t *ipt_chain) {
    IPTGvpList **ptr = &entry->head;
    int gvpEntry = 0;
    while(*ptr) {
        ipt_chain[gvpEntry] = (*ptr)->ipt_bits; gvpEntry++;
        ptr = &(*ptr)->next;	
    }
}

static void IPTGvpList_del(IPTHvp *base, uint64_t ipt_bits) {
    IPTGvpList **ptr = &base->head;
    IPTGvpList *ele = base->head;
    IPTGvpList *new_next;

    while(ele != NULL && IPT_GET_CMP(ele->ipt_bits) != IPT_GET_CMP(ipt_bits)) {
        ptr = &ele->next;
        ele = ele->next;
    }

    if(ele) {
        // Found matching entry
        new_next = ele->next;   // Save ptr to next element in linked-list
        free(*ptr);             // Delete entry
        *ptr = new_next;        // Linked-list skips deleted element
    }
}


/**
 * IPTHvp_get: Get IPT entry from the HVP
 *
 * Returns 'entry' which points to the IPT entry and 
 * 'entryPtr' which stores the ptr that points to 'entry' location
 *
 * If *entry is NULL then the HVP is not present in the FPGA
 *
 * @hvp: Host Virtual Page of IPT entry to find
 * @entry:    ret pointer to the matching hvp entry 
 * @entryPtr: ret pointer to the 'entry' ptr
 */
static void IPTHvp_get(uint64_t hvp, IPTHvp **entry, IPTHvpPtr **entryPtr) {
    size_t entry_idx = IPT_hash(hvp);
    *entry = ipt.entries[entry_idx];
    *entryPtr = &ipt.entries[entry_idx];
    while((*entry) != NULL && (*entry)->hvp != hvp) {
        *entryPtr = &((*entry)->next);
        *entry = (*entry)->next;
    }
}

static bool IPTHvp_insert_Gvp(uint64_t hvp, uint64_t ipt_bits) {
    IPTHvp *entry;
    IPTHvpPtr *entryPtr;
    bool has_synonyms = true;

    IPTHvp_get(hvp, &entry, &entryPtr);
    if(!entry) {
        // Create HVP entry if not present
        *entryPtr = malloc(sizeof(IPTHvp));
        entry = *entryPtr;
        entry->hvp = hvp;
        entry->cnt = 0;
        has_synonyms = false;
    }

    IPTGvpList_insert(entry, ipt_bits);
    entry->cnt++;
    return has_synonyms;
}

static int IPTHvp_get_Gvp_synonyms(uint64_t hvp, uint64_t **ipt_chain) {
    int count = 0;
    IPTHvp *entry;
    IPTHvpPtr *entryPtr;

    IPTHvp_get(hvp, &entry, &entryPtr);
    if(entry) {
        // Has synonyms, return synonym list
        count = entry->cnt;
        *ipt_chain = calloc(entry->cnt, sizeof(uint64_t));
    }

    IPTGvpList_get_chain(entry, *ipt_chain);
    return count;
}

static void IPTHvp_evict_Gvp(uint64_t hvp, uint64_t ipt_bits) {
    IPTHvp *entry;
    IPTHvpPtr *entryPtr;
    
    IPTHvp_get(hvp, &entry, &entryPtr);
    IPTGvpList_del(entry, ipt_bits);
    entry->cnt--;
    if(entry->head == NULL) {
        // There's no more gvp mapped to this hvp after eviction
        IPTHvp_evict(&entry, entryPtr);
    }
}

#define ASID_ENTRIES (1 << 16) // Particular to architecture
static uint64_t ttbr_list[ASID_ENTRIES] = {0}; // Used to retranslate GVA to HVA

void register_asid(uint64_t asid, uint64_t asid_reg) {
    ttbr_list[asid] = asid_reg;
}

int ipt_evict(uint64_t hvp, uint64_t ipt_bits) {
    IPTHvp_evict_Gvp(hvp, ipt_bits);
    return 0;
}

int ipt_add_entry(uint64_t hvp, uint64_t ipt_bits) {
    bool has_synonyms = IPTHvp_insert_Gvp(hvp, ipt_bits);
    if(has_synonyms) {
        return SYNONYM;
    } else {
        return PAGE;
    }
}

int ipt_check_synonyms(uint64_t hvp, uint64_t **ipt_chain) {
    int count = IPTHvp_get_Gvp_synonyms(hvp, ipt_chain);
    return count;
}

/* Evictions are done lazely, and translation from GVA to HVA require the CPUState, which 
 * might already have changed. We must store the previously translated gva2hva somewhere.
 */
uint64_t page_table_get_hvp(uint64_t ipt_bits, int perm) {
    uint64_t asid_reg = ttbr_list[IPT_GET_ASID(ipt_bits)];
    assert(asid_reg >> 48 == IPT_GET_ASID(ipt_bits));
    uint64_t gva = gva_to_hva_with_asid(asid_reg, IPT_GET_VA(ipt_bits), perm);
    if(gva != -1) {
        perror("DevteroFlex: Failed retranslating previously translated host virtual address.\n");
        exit(EXIT_FAILURE);
    } 
    return gva;
}