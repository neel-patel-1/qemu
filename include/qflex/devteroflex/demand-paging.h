#ifndef DEVTEROFLEX_PAGE_DEMANDER_H
#define DEVTEROFLEX_PAGE_DEMANDER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* 
 * Layout of Guest VA (GVA)
 * GVA |  isKernel(K)  | PAGE_NUMBER | PAGE_OFFST |
 * bits| 63         48 | 47       12 | 11       0 |
 * val | 0xFFFF/0x0000 |  Any Number | Don't Care |
 *
 * NOTE: PID in linux is usually from 0 to 0x0FFF
 *
 * Compress Guest VA, PID, Permission (P)
 *     | K  |   PID   | PAGE_NUMBER | Don't Care |  P  |
 * bits| 63 | 62   48 | 47       12 | 11      10 | 1 0 |
 */

#define IPT_MASK_PID        (0x7fffULL << 48)
#define IPT_MASK_PAGE_NB    (0xfffffffffULL << 12)
#define IPT_MASK_isKernel   (1ULL << 63)
#define IPT_MASK_P          (0b11ULL)
#define PAGE_MASK           (0xfffULL)

#define IPT_GET_ASID(bits)          ((bits & ~IPT_MASK_isKernel) >> 48)
#define IPT_GET_VA_BITS(bits)       (bits & IPT_MASK_PAGE_NB)
#define IPT_GET_KERNEL_BITS(bits)   ((bits & IPT_MASK_isKernel) ? (0xffffULL << 48) : 0x0)
#define IPT_GET_PERM(bits)           (bits & IPT_MASK_P)
#define IPT_GET_VA(bits)            (IPT_GET_KERNEL_BITS(bits) | IPT_GET_VA_BITS(bits))
#define IPT_GET_CMP(bits)           (bits & ~PAGE_MASK) // Drop Permission
#define IPT_ASSEMBLE_64(hi, lo)     ((uint64_t) hi << 32 | (uint64_t) lo)

#define IPT_SET_KERNEL(va)  (va & (1ULL << 63))
#define IPT_SET_PID(pid)    ((unsigned long long) pid << 48)
#define IPT_SET_VA_BITS(va) (va & IPT_MASK_PAGE_NB)
#define IPT_SET_PER(p)      (p  & IPT_MASK_P)
#define IPT_COMPRESS(va, pid, p) \
    (IPT_SET_KERNEL(va) | IPT_SET_PID(pid) | IPT_SET_VA_BITS(va) | IPT_SET_PER(p))

#define GET_PAGE(bits)      (bits & ~0xfffULL)

typedef enum PageTypes {
    PAGE = 0,
    SYNONYM = 1
} PageTypes;

/* Add entry to inverted page table, this is used to detect synonyms
 */
int ipt_add_entry(uint64_t hvp, uint64_t ipt_bits);
/* Clear entry of inverted page table after bringing back the page from the FPGA
 */
int ipt_evict(uint64_t hvp, uint64_t ipt_bits);
/* Check whenever a physical address has synonyms
 */
int ipt_check_synonyms(uint64_t hvp, uint64_t **ipt_chain);
/* Init inverted page table
 */
void ipt_init(void);

/**
 * @brief insert an entry to the temporal page table.
 * 
 * @param ipt_bits the VA+ASID+Permission
 * @param hvp the host virtual address
 */
void tpt_add_entry(uint64_t ipt_bits, uint64_t hvp);

/**
 * @brief remove an entry from the temporal page table.
 * 
 * @param ipt_bits the VA+ASID+Permission
 */
void tpt_remove_entry(uint64_t ipt_bits);

/**
 * @brief query the temporal page table with the ipt_bits.
 * 
 * @param ipt_bits the VA+ASID+Permission
 * @return the host virtual address
 * 
 * @note abort will be called in case no such an element is found in the table.
 */
uint64_t tpt_lookup(uint64_t ipt_bits);

/**
 * @brief initilize the temporal page table.
 */
void tpt_init(void);


/* Store architectural register of ASID and Base Address of physical page for
 * retranslating address later.
 */
void ipt_register_asid(uint64_t asid, uint64_t asid_reg);

/* If an evicted page is modified, save pending eviction while waiting for writeback
 */
void evict_notify_pending_add(uint64_t ipt_bits, uint64_t hvp);
/* If an evicted page is modified, clear pending eviction once writeback has completed
 */
void evict_notfiy_pending_clear(uint64_t ipt_bits);

// These three functions are there to manage page fault, and address conflicts
/* Send a page fault response, if thid = -1, then no fpga core will restart executing
 */
void page_fault_return(uint64_t ipt_bits, uint64_t hvp, uint32_t thid);
/* Check whenever the target physical address is waiting for an eviction
 * writeback
 */
bool page_fault_pending_eviction_has_hvp(uint64_t hvp);
/* Add page fault to list waiting for evictions writeback
 */
void page_fault_pending_add(uint64_t ipt_bits, uint64_t hvp, uint32_t thid);
/* Run all pending page fault after completing eviction writeback
 * @return true if there was pending requests mapping to that hvp
 */
bool page_fault_pending_run(uint64_t hvp);

/* Evict page from the FPGA for synchronizing
 */
void page_eviction_request(uint64_t ipt_bits);
/* Wait for all evictions in the list to complete
 */
void page_eviction_wait_complete(uint64_t *ipt_list, int count);

/**
 * FPGA free physical addreses are managed by the host.
 * These functions keep track of which physical addresses 
 * are free.
 */
int fpga_paddr_init_manager(size_t tot_physical_pages, uint64_t data_base_addr);
bool fpga_paddr_get(uint64_t *paddr);
void fpga_paddr_push(uint64_t paddr);
void fpga_paddr_free_stack(void);

struct CPUState;

/* Call this function before a tcg_gen_qemu_ld/st is executed
 * to make sure that QEMU has the latest page modifications
 * NOTE: Uses 'qflex_mem_trace' helper as trigger which is 
 * generated before memory instructions
 */ 
void devteroflex_synchronize_page(struct CPUState *cpu, uint64_t vaddr, int type);

bool insert_entry_get_ppn(uint64_t hvp, uint64_t ipt_bits, uint64_t *ppn);
 
uint64_t page_table_get_hvp(uint64_t ipt_bits, int perm);
 
void run_request_pending_messages(void);

#endif
