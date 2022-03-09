#ifndef DEVTEROFLEX_MMU_H
#define DEVTEROFLEX_MMU_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/**
 * @file devteroflex-mmu.h
 * @brief Defines translation-related functions of Devteroflex.
 * 
 * TODO: Merge demand paging functions and PPN allocation logics into this file.
 */

/**
 * @brief Flush a translation retrieved by virtual address and address space id.
 * 
 * @param va the given virtual address
 * @param asid the address space id
 * 
 * @note At present this function will cause page evictions. 
 * 
 * TODO: Modify the page eviction handler so that the handler doesn't have to fetch the page back all the time.
 */
void devteroflex_mmu_flush_by_va_asid(uint64_t va, uint64_t asid);

/**
 * @brief Flush all existing translations belonging to a specific address space.
 * 
 * @param asid the address space id
 * 
 * @note This function will evict all pages in a given address space. It has huge performance problem.
 */
void devteroflex_mmu_flush_by_asid(uint64_t asid);


/**
 * @brief Flush all existing translations by VA, for any ASID.
 * 
 * @param va the given virtual address
 * 
 */
void devteroflex_mmu_flush_by_va(uint64_t va);

/**
 * @brief Flush translations retrieved by host virtual address (QEMU physical address) and the address space id.
 * 
 * @param hva the host virtual address
 * @param asid the address space id
 * 
 * @note This function will cause page evictions.
 */
void devteroflex_mmu_flush_by_hva_asid(uint64_t hva, uint64_t asid);

/**
 * @brief Flush translations retrieved by host virtual address.
 * 
 * @param hva the host virtual address
 * 
 * @note This function will cause page evictions.
 */
void devteroflex_mmu_flush_by_hva(uint64_t hva);

/**
 * @brief Flush all translations in Devteroflex.
 * 
 * @note This function will evict all pages in FPGA. Please use it wisely and carefully.
 */
void devteroflex_mmu_flush_all(void);


#endif
