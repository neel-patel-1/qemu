#include <stdbool.h>
#include <stdlib.h>
#include "util/circular-buffer.h"
#include "qflex/qflex.h"
#include "qflex/devteroflex/devteroflex.h"
#include "qflex/devteroflex/demand-paging.h"
#include "qflex/devteroflex/fpga/fpga_interface.h"

#include <glib.h>

// TODO: Rewrite this file with C++.

// Shadow page table. 
static GHashTable *spt = NULL;

void spt_init(void) {
  spt = g_hash_table_new_full(g_int64_hash, g_int64_equal, free, free);
}

bool spt_is_entry_exists(uint64_t hvp) {
  return g_hash_table_lookup(spt, &hvp) != NULL;
}

uint64_t spt_lookup(uint64_t hvp) { // return the FPGA PPN given a hypervisor VA.
  gpointer lookup_res = g_hash_table_lookup(spt, &hvp);
  if (lookup_res == NULL) {
    // dump the message info.
    qemu_log("The generated key for hash table is %lu \n", hvp);
    qemu_log("Now backtrace the content in the HVM page table... \n");

    GHashTableIter iter;
    gpointer ipt_bits_ptr;
    gpointer hvp_ptr;
    g_hash_table_iter_init(&iter, spt);
    while(g_hash_table_iter_next(&iter, &ipt_bits_ptr, &hvp_ptr)){
        qemu_log("Key: %lu, HVP: %lu \n", *(uint64_t *)ipt_bits_ptr, *(uint64_t *)hvp_ptr);
    }
    
    abort();
  }

  return *(uint64_t *)lookup_res;
}

void spt_remove_entry(uint64_t hvp) {
  g_hash_table_remove(spt, &hvp);
}

void spt_add_entry(uint64_t hvp, uint64_t ppn) {
  assert(g_hash_table_lookup(spt, &hvp) == NULL);

  uint64_t *hvp_bits_index = calloc(1, sizeof(uint64_t));
  *hvp_bits_index = hvp;
  uint64_t *ppn_value = calloc(1, sizeof(uint64_t));
  *ppn_value = ppn;
  
  g_hash_table_insert(spt, hvp_bits_index, ppn_value);
}

uint64_t *spt_all_keys(uint64_t *count) {
  *count = g_hash_table_size(spt);

  if(g_hash_table_size(spt) == 0) {
    return NULL;
  }

  uint64_t *result = calloc(g_hash_table_size(spt), sizeof(uint64_t));

  GHashTableIter iter;
  gpointer ipt_bits_ptr;
  gpointer hvp_ptr;
  g_hash_table_iter_init(&iter, spt);

  int index = 0;
  while(g_hash_table_iter_next(&iter, &ipt_bits_ptr, &hvp_ptr)){
      result[index] = *(uint64_t *)ipt_bits_ptr;
      index += 1;
  }


  return result;
}