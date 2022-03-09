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

// Temporal page table. 
static GHashTable *tpt = NULL;

void tpt_init(void) {
  tpt = g_hash_table_new_full(g_int64_hash, g_int64_equal, free, free);
}

bool tpt_is_entry_exists(uint64_t ipt_bits) {
  return g_hash_table_lookup(tpt, &ipt_bits) != NULL;
}

uint64_t tpt_lookup(uint64_t ipt_bits) {
  gpointer lookup_res = g_hash_table_lookup(tpt, &ipt_bits);
  if (lookup_res == NULL) {
    // dump the message info.
    qemu_log("The generated key for hash table is %lu \n", ipt_bits);
    qemu_log("Now backtrace the content in the temporal page table... \n");

    GHashTableIter iter;
    gpointer ipt_bits_ptr;
    gpointer hvp_ptr;
    g_hash_table_iter_init(&iter, tpt);
    while(g_hash_table_iter_next(&iter, &ipt_bits_ptr, &hvp_ptr)){
        qemu_log("Key: %lu, HVP: %lu \n", *(uint64_t *)ipt_bits_ptr, *(uint64_t *)hvp_ptr);
    }
    
    abort();
  }

  return *(uint64_t *)lookup_res;
}

void tpt_remove_entry(uint64_t ipt_bits) {
  g_hash_table_remove(tpt, &ipt_bits);
}

void tpt_add_entry(uint64_t ipt_bits, uint64_t hvp) {
  assert(g_hash_table_lookup(tpt, &ipt_bits) == NULL);

  uint64_t *ipt_bits_index = calloc(1, sizeof(uint64_t));
  *ipt_bits_index = ipt_bits;
  uint64_t *hvp_value = calloc(1, sizeof(uint64_t));
  *hvp_value = hvp;
  
  g_hash_table_insert(tpt, ipt_bits_index, hvp_value);
}

uint64_t *tpt_all_keys(uint64_t *count) {
  *count = g_hash_table_size(tpt);

  if(g_hash_table_size(tpt) == 0) {
    return NULL;
  }

  uint64_t *result = calloc(g_hash_table_size(tpt), sizeof(uint64_t));

  GHashTableIter iter;
  gpointer ipt_bits_ptr;
  gpointer hvp_ptr;
  g_hash_table_iter_init(&iter, tpt);

  int index = 0;
  while(g_hash_table_iter_next(&iter, &ipt_bits_ptr, &hvp_ptr)){
      result[index] = *(uint64_t *)ipt_bits_ptr;
      index += 1;
  }


  return result;
}