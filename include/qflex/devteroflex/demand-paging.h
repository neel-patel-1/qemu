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

/* If an evicted page is modified, save pending eviction while waiting for writeback
 */
void evict_notify_pending_add(uint64_t ipt_bits, uint64_t hvp);
/* If an evicted page is modified, clear pending eviction once writeback has completed
 */
void evict_notfiy_pending_clear(uint64_t ipt_bits);

// These three functions are there to manage page fault, and address conflicts
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

struct CPUState;

/* Call this function before a tcg_gen_qemu_ld/st is executed
 * to make sure that QEMU has the latest page modifications
 * NOTE: Uses 'qflex_mem_trace' helper as trigger which is 
 * generated before memory instructions
 *
 * @return true if page in FPGA detected
 */ 
bool devteroflex_synchronize_page(struct CPUState *cpu, uint64_t vaddr, int type);

bool insert_entry_get_ppn(uint64_t hvp, uint64_t ipt_bits, uint64_t *ppn);
  
void run_request_pending_messages(void);

#endif
