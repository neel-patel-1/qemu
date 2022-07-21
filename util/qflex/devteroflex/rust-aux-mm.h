#ifndef DEVTEROFLEX_RUST_AUX_HH
#define DEVTEROFLEX_RUST_AUX_HH

#include <stdint.h>

// Init the Rust library.
void rust_aux_init(uint64_t fppn_total_number, uint64_t data_starting_address);

typedef struct c_array_t {
  uint64_t length;
  const uint64_t *data;
} c_array_t;

// Temporal page table
int tpt_key_exists(uint64_t compressed_ipt_item);
uint64_t tpt_lookup(uint64_t compressed_ipt_item);
void tpt_remove(uint64_t compressed_ipt_item);
void tpt_register(uint64_t compressed_ipt_item, uint64_t hva);
c_array_t tpt_all_keys(void);

// Shadow page table
int spt_key_exists(uint64_t hva);
uint64_t spt_lookup(uint64_t hva);
void spt_remove(uint64_t hva);
void spt_register(uint64_t hva, uint64_t fpga_ppn);
c_array_t spt_all_keys(void);

// Inverted page table
int ipt_register(uint64_t hva, uint64_t compressed_ipt_item);
void ipt_unregister(uint64_t hva, uint64_t compressed_ipt_item);
c_array_t ipt_lookup(uint64_t hva);

// FPGA Free PPN
uint64_t fppn_allocate(void);
void fppn_recycle(uint64_t);


#endif