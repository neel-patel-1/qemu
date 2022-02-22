#include <bits/stdint-uintn.h>
#include <stdbool.h>
#include <stdlib.h>
#include "util/circular-buffer.h"
#include "qflex/qflex.h"
#include "qflex/devteroflex/devteroflex.h"
#include "qflex/devteroflex/demand-paging.h"
#ifdef AWS_FPGA
#include "qflex/devteroflex/aws/fpga_interface.h"
#else
#include "qflex/devteroflex/simulation/fpga_interface.h"
#endif

#include <glib.h>

// TODO: Rewrite this file with C++.

static GHashTable *ipt;

#define ASID_ENTRIES (1 << 16) // Particular to architecture
static uint64_t ttbr_list[ASID_ENTRIES] = {0}; // Used to retranslate GVA to HVA

static void printIPT(void){
    GHashTableIter out;
    gpointer hvp;
    gpointer set;
    g_hash_table_iter_init(&out, ipt);
    while(g_hash_table_iter_next(&out, &hvp, &set)){
        uint64_t u64_hvp = *(uint64_t *)hvp;
        GHashTable *hash_set = set;

        qemu_log("HVP[%lu]: ", u64_hvp);

        if(hash_set == NULL) {
            qemu_log("\n");
            continue;
        };

        GHashTableIter sub_iter;
        g_hash_table_iter_init(&sub_iter, hash_set);
        gpointer ipt_bits_ptr;
        gpointer ipt_bits_ptr_unused;

        while(g_hash_table_iter_next(&sub_iter, &ipt_bits_ptr, &ipt_bits_ptr_unused)){
            uint64_t ipt = *(uint64_t *)ipt_bits_ptr;
            qemu_log("%lu ", ipt);
        }

        qemu_log("\n");
    }
}

void ipt_register_asid(uint64_t asid, uint64_t asid_reg) {
    ttbr_list[asid] = asid_reg;
}

int ipt_evict(uint64_t hvp, uint64_t ipt_bits) {
    GHashTable *set = g_hash_table_lookup(ipt, &hvp);
    if (set == NULL){
        qemu_log("Warning: Try to evict a non-existed hvp from the IPT. \n");
        qemu_log("Information: The HVP is %lu, and the ipt_bits is %lu. \n", hvp, ipt_bits);
        printIPT();
        abort();
    }
    uint64_t *target_ipt = g_hash_table_lookup(set, &ipt_bits);
    if (target_ipt == NULL) {
        qemu_log("Warning: Try to evict a non-existed ipt_bits from the IPT. \n");
        qemu_log("Information: The HVP is %lu, and the ipt_bits is %lu. \n", hvp, ipt_bits);
        printIPT();
        abort();
    }

    if(*target_ipt != ipt_bits) {
        qemu_log("Warning: Sainity check in IPT failed. The required ipt_btis is %lu. \n", ipt_bits);
        abort();
    }

    // evict the element
    g_hash_table_remove(set, &ipt_bits);
    // if the set is empty, also remove the set
    if(g_hash_table_size(set) == 0){
        g_hash_table_destroy(set);
        g_hash_table_remove(ipt, &hvp);
    }
    return 0;
}

PageTypes ipt_add_entry(uint64_t hvp, uint64_t ipt_bits) {
    GHashTable *set = g_hash_table_lookup(ipt, &hvp);
    if(set == NULL){
        // OK, so it's the first element.
        // create the set
        GHashTable *synonyms_set = g_hash_table_new_full(g_int64_hash, g_int64_equal, free, NULL);
        // append the element into the set
        uint64_t *ipt_bits_index = calloc(1, sizeof(uint64_t));
        *ipt_bits_index = ipt_bits;
        g_hash_table_insert(synonyms_set, ipt_bits_index, ipt_bits_index);

        // insert the set to the ipt
        uint64_t *hvp_index = calloc(1, sizeof(uint64_t));
        *hvp_index = hvp;
        g_hash_table_insert(ipt, hvp_index, synonyms_set);

        return PAGE;
    } else {
        // append the element into the set
        uint64_t *ipt_bits_index = calloc(1, sizeof(uint64_t));
        *ipt_bits_index = ipt_bits;
        g_hash_table_insert(set, ipt_bits_index, ipt_bits_index);
        return SYNONYM;
    }
}

void ipt_init(void) {
    ipt = g_hash_table_new_full(g_int64_hash, g_int64_equal, free, NULL);
}

/**
 * @brief fetch all synonyms mapped to one specifc hvp
 * @param hvp the hvp as index
 * @param ipt_chain the array. The caller takes the responsibility to release it.
 * 
 * @return count of ipt_chain
 */
int ipt_check_synonyms(uint64_t hvp, uint64_t **ipt_chain) {
    GHashTable *set = g_hash_table_lookup(ipt, &hvp);
    if(set == NULL){
        return 0;
    }
    unsigned int res = 0;
    gpointer *array = g_hash_table_get_keys_as_array(set, &res);

    if(res == 0){
        g_free(array);
        return 0;
    }

    *ipt_chain = calloc(res, sizeof(uint64_t));
    for(int i = 0; i < res; ++i){
        (*ipt_chain)[i] = *(uint64_t *)array[i];
    }

    g_free(array);

    return res;
}

/* Evictions are done lazely, and translation from GVA to HVA require the CPUState, which 
 * might already have changed. We must store the previously translated gva2hva somewhere.
 */
uint64_t page_table_get_hvp(uint64_t ipt_bits, int perm) {
    uint64_t asid_reg = ttbr_list[IPT_GET_ASID(ipt_bits)];
    assert(asid_reg >> 48 == IPT_GET_ASID(ipt_bits));
    // TODO: This function doesn't work, QEMU uses more than asid register to translate
    uint64_t gva = gva_to_hva_with_asid(asid_reg, IPT_GET_VA(ipt_bits), perm);
    if(gva != -1) {
        perror("DevteroFlex: Failed retranslating previously translated host virtual address.\n");
        exit(EXIT_FAILURE);
    } 
    return gva;
}