//  DO-NOT-REMOVE begin-copyright-block
// QFlex consists of several software components that are governed by various
// licensing terms, in addition to software that was developed internally.
// Anyone interested in using QFlex needs to fully understand and abide by the
// licenses governing all the software components.
// 
// ### Software developed externally (not by the QFlex group)
// 
//     * [NS-3] (https://www.gnu.org/copyleft/gpl.html)
//     * [QEMU] (http://wiki.qemu.org/License)
//     * [SimFlex] (http://parsa.epfl.ch/simflex/)
//     * [GNU PTH] (https://www.gnu.org/software/pth/)
// 
// ### Software developed internally (by the QFlex group)
// **QFlex License**
// 
// QFlex
// Copyright (c) 2020, Parallel Systems Architecture Lab, EPFL
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
// 
//     * Redistributions of source code must retain the above copyright notice,
//       this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright notice,
//       this list of conditions and the following disclaimer in the documentation
//       and/or other materials provided with the distribution.
//     * Neither the name of the Parallel Systems Architecture Laboratory, EPFL,
//       nor the names of its contributors may be used to endorse or promote
//       products derived from this software without specific prior written
//       permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE PARALLEL SYSTEMS ARCHITECTURE LABORATORY,
// EPFL BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
// GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//  DO-NOT-REMOVE end-copyright-block
/*
 * DMA helper functions
 *
 * Copyright (c) 2009 Red Hat
 *
 * This work is licensed under the terms of the GNU General Public License
 * (GNU GPL), version 2 or later.
 */

#ifndef DMA_H
#define DMA_H

#include "exec/memory.h"
#include "exec/address-spaces.h"
#include "hw/hw.h"
#include "block/block.h"
#include "block/accounting.h"

#ifdef CONFIG_FLEXUS
#include "../libqflex/api.h"
#include "include/sysemu/sysemu.h"
#include "qflex/qflex.h"
#endif

typedef struct ScatterGatherEntry ScatterGatherEntry;

typedef enum {
    DMA_DIRECTION_TO_DEVICE = 0,
    DMA_DIRECTION_FROM_DEVICE = 1,
} DMADirection;

struct QEMUSGList {
    ScatterGatherEntry *sg;
    int nsg;
    int nalloc;
    size_t size;
    DeviceState *dev;
    AddressSpace *as;
};

#ifndef CONFIG_USER_ONLY

/*
 * When an IOMMU is present, bus addresses become distinct from
 * CPU/memory physical addresses and may be a different size.  Because
 * the IOVA size depends more on the bus than on the platform, we more
 * or less have to treat these as 64-bit always to cover all (or at
 * least most) cases.
 */
typedef uint64_t dma_addr_t;

#define DMA_ADDR_BITS 64
#define DMA_ADDR_FMT "%" PRIx64

static inline void dma_barrier(AddressSpace *as, DMADirection dir)
{
    /*
     * This is called before DMA read and write operations
     * unless the _relaxed form is used and is responsible
     * for providing some sane ordering of accesses vs
     * concurrently running VCPUs.
     *
     * Users of map(), unmap() or lower level st/ld_*
     * operations are responsible for providing their own
     * ordering via barriers.
     *
     * This primitive implementation does a simple smp_mb()
     * before each operation which provides pretty much full
     * ordering.
     *
     * A smarter implementation can be devised if needed to
     * use lighter barriers based on the direction of the
     * transfer, the DMA context, etc...
     */
    smp_mb();
}

/* Checks that the given range of addresses is valid for DMA.  This is
 * useful for certain cases, but usually you should just use
 * dma_memory_{read,write}() and check for errors */
static inline bool dma_memory_valid(AddressSpace *as,
                                    dma_addr_t addr, dma_addr_t len,
                                    DMADirection dir)
{
    return address_space_access_valid(as, addr, len,
                                      dir == DMA_DIRECTION_FROM_DEVICE);
}

static inline int dma_memory_rw_relaxed(AddressSpace *as, dma_addr_t addr,
                                        void *buf, dma_addr_t len,
                                        DMADirection dir)
{
   
#if defined(CONFIG_FLEXUS)
  //FIXME need to build memory transaction here
  memory_transaction_t* mem_trans = malloc(sizeof(memory_transaction_t));
  mem_trans->s.physical_address = addr;
  if(dir == DMA_DIRECTION_FROM_DEVICE){
    mem_trans->s.type = QEMU_Trans_Store;
  } else {
    mem_trans->s.type = QEMU_Trans_Load;
  }

  mem_trans->s.size = len;

#ifdef CONFIG_DEBUG_LIBQFLEX
  QEMU_increment_debug_stat(NUM_DMA_ALL);
#endif
  if (flexus_in_trace()) {
    flexus_dynlib_fns.qflex_sim_callbacks.trace_mem_dma(mem_trans);
  }
  free(mem_trans);
#endif

    return (bool)address_space_rw(as, addr, MEMTXATTRS_UNSPECIFIED,
                                  buf, len, dir == DMA_DIRECTION_FROM_DEVICE);
}

static inline int dma_memory_read_relaxed(AddressSpace *as, dma_addr_t addr,
                                          void *buf, dma_addr_t len)
{
    return dma_memory_rw_relaxed(as, addr, buf, len, DMA_DIRECTION_TO_DEVICE);
}

static inline int dma_memory_write_relaxed(AddressSpace *as, dma_addr_t addr,
                                           const void *buf, dma_addr_t len)
{
    return dma_memory_rw_relaxed(as, addr, (void *)buf, len,
                                 DMA_DIRECTION_FROM_DEVICE);
}

static inline int dma_memory_rw(AddressSpace *as, dma_addr_t addr,
                                void *buf, dma_addr_t len,
                                DMADirection dir)
{
    dma_barrier(as, dir);

    return dma_memory_rw_relaxed(as, addr, buf, len, dir);
}

static inline int dma_memory_read(AddressSpace *as, dma_addr_t addr,
                                  void *buf, dma_addr_t len)
{
    return dma_memory_rw(as, addr, buf, len, DMA_DIRECTION_TO_DEVICE);
}

static inline int dma_memory_write(AddressSpace *as, dma_addr_t addr,
                                   const void *buf, dma_addr_t len)
{
    return dma_memory_rw(as, addr, (void *)buf, len,
                         DMA_DIRECTION_FROM_DEVICE);
}

int dma_memory_set(AddressSpace *as, dma_addr_t addr, uint8_t c, dma_addr_t len);

static inline void *dma_memory_map(AddressSpace *as,
                                   dma_addr_t addr, dma_addr_t *len,
                                   DMADirection dir)
{
    hwaddr xlen = *len;
    void *p;

    p = address_space_map(as, addr, &xlen, dir == DMA_DIRECTION_FROM_DEVICE);
    *len = xlen;
    return p;
}

static inline void dma_memory_unmap(AddressSpace *as,
                                    void *buffer, dma_addr_t len,
                                    DMADirection dir, dma_addr_t access_len)
{
    address_space_unmap(as, buffer, (hwaddr)len,
                        dir == DMA_DIRECTION_FROM_DEVICE, access_len);
}

#define DEFINE_LDST_DMA(_lname, _sname, _bits, _end) \
    static inline uint##_bits##_t ld##_lname##_##_end##_dma(AddressSpace *as, \
                                                            dma_addr_t addr) \
    {                                                                   \
        uint##_bits##_t val;                                            \
        dma_memory_read(as, addr, &val, (_bits) / 8);                   \
        return _end##_bits##_to_cpu(val);                               \
    }                                                                   \
    static inline void st##_sname##_##_end##_dma(AddressSpace *as,      \
                                                 dma_addr_t addr,       \
                                                 uint##_bits##_t val)   \
    {                                                                   \
        val = cpu_to_##_end##_bits(val);                                \
        dma_memory_write(as, addr, &val, (_bits) / 8);                  \
    }

static inline uint8_t ldub_dma(AddressSpace *as, dma_addr_t addr)
{
    uint8_t val;

    dma_memory_read(as, addr, &val, 1);
    return val;
}

static inline void stb_dma(AddressSpace *as, dma_addr_t addr, uint8_t val)
{
    dma_memory_write(as, addr, &val, 1);
}

DEFINE_LDST_DMA(uw, w, 16, le);
DEFINE_LDST_DMA(l, l, 32, le);
DEFINE_LDST_DMA(q, q, 64, le);
DEFINE_LDST_DMA(uw, w, 16, be);
DEFINE_LDST_DMA(l, l, 32, be);
DEFINE_LDST_DMA(q, q, 64, be);

#undef DEFINE_LDST_DMA

struct ScatterGatherEntry {
    dma_addr_t base;
    dma_addr_t len;
};

void qemu_sglist_init(QEMUSGList *qsg, DeviceState *dev, int alloc_hint,
                      AddressSpace *as);
void qemu_sglist_add(QEMUSGList *qsg, dma_addr_t base, dma_addr_t len);
void qemu_sglist_destroy(QEMUSGList *qsg);
#endif

typedef BlockAIOCB *DMAIOFunc(int64_t offset, QEMUIOVector *iov,
                              BlockCompletionFunc *cb, void *cb_opaque,
                              void *opaque);

BlockAIOCB *dma_blk_io(AioContext *ctx,
                       QEMUSGList *sg, uint64_t offset, uint32_t align,
                       DMAIOFunc *io_func, void *io_func_opaque,
                       BlockCompletionFunc *cb, void *opaque, DMADirection dir);
BlockAIOCB *dma_blk_read(BlockBackend *blk,
                         QEMUSGList *sg, uint64_t offset, uint32_t align,
                         BlockCompletionFunc *cb, void *opaque);
BlockAIOCB *dma_blk_write(BlockBackend *blk,
                          QEMUSGList *sg, uint64_t offset, uint32_t align,
                          BlockCompletionFunc *cb, void *opaque);
uint64_t dma_buf_read(uint8_t *ptr, int32_t len, QEMUSGList *sg);
uint64_t dma_buf_write(uint8_t *ptr, int32_t len, QEMUSGList *sg);

void dma_acct_start(BlockBackend *blk, BlockAcctCookie *cookie,
                    QEMUSGList *sg, enum BlockAcctType type);

#endif
