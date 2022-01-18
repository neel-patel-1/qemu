#ifndef DEVTEROFLEX_H
#define DEVTEROFLEX_H

#include <stdbool.h>

#include "qemu/osdep.h"
#include "qemu/thread.h"
#include "qflex/qflex.h"

#define PAGE_SIZE (4096)

#ifndef MemoryAccessType
// See cpu.h to match MMUAccessType
typedef enum MemoryAccessType {
    DATA_LOAD  = 0,
    DATA_STORE = 1,
    INST_FETCH = 2
} MemoryAccessType;
#define MemoryAccessType
#endif

typedef struct DevteroflexConfig {
    bool enabled;
    bool running;
} DevteroflexConfig;

extern DevteroflexConfig devteroflexConfig;

void devteroflex_init(bool enabled, bool run, size_t fpga_physical_pages);

static inline void devteroflex_start(void) {
    devteroflexConfig.enabled = true;
    devteroflexConfig.running = true;
    qemu_log("DEVTEROFLEX: Start detected.\n");
    qflex_update_exit_main_loop(true);
    qflex_tb_flush();
}

static inline void devteroflex_stop(void) {
    devteroflexConfig.running = false;
    qemu_log("DEVTEROFLEX: Stop detected.\n");
    qflex_tb_flush();
}

void devteroflex_stop_full(void);

static inline bool devteroflex_is_running(void) { return devteroflexConfig.enabled && devteroflexConfig.running; }
static inline bool devteroflex_is_enabled(void) { return devteroflexConfig.enabled; }

/* The following functions are architecture specific, so they are
 * in the architecture target directory.
 * (target/arm/devteroflex-helper.c)
 */

/* This describes layour of arch state elements
 *
 * XREGS: uint64_t
 * PC   : uint64_t
 * SP   : uint64_t
 * CF/VF/NF/ZF : uint64_t
 */
#define ARCH_PSTATE_NF_MASK     (3)    // 64bit 3
#define ARCH_PSTATE_ZF_MASK     (2)    // 64bit 2
#define ARCH_PSTATE_CF_MASK     (1)    // 64bit 1
#define ARCH_PSTATE_VF_MASK     (0)    // 64bit 0
#define DEVTEROFLEX_TOT_REGS    (35)

typedef struct DevteroflexArchState {
	uint64_t xregs[32];
	uint64_t pc;
	uint64_t sp;
	uint64_t nzcv;
} DevteroflexArchState;

/** Serializes the DEVTEROFLEX architectural state to be transfered with protobuf.
 * @brief devteroflex_(un)serialize_archstate
 * @param devteroflex The cpu state
 * @param buffer  The buffer
 * @return        Returns 0 on success
 *
 * NOTE: Don't forget to close the buffer when done
 */
void devteroflex_serialize_archstate(DevteroflexArchState *devteroflex, void *buffer);
void devteroflex_unserialize_archstate(void *buffer, DevteroflexArchState *devteroflex);

/** Packs QEMU CPU architectural state into DEVTEROFLEX CPU architectural state.
 * NOTE: Architecture specific: see target/arch/devteroflex-helper.c
 * @brief devteroflex_(un)pack_archstate
 * @param cpu     The QEMU CPU state
 * @param devteroflex The DEVTEROFLEX CPU state
 * @return        Returns 0 on success
 */
void devteroflex_pack_archstate(DevteroflexArchState *devteroflex, CPUState *cpu);
void devteroflex_unpack_archstate(CPUState *cpu, DevteroflexArchState *devteroflex);

/**
 * @brief devteroflex_get_load_addr
 * Translates from guest virtual address to host virtual address
 * NOTE: In case of FAULT, the caller should:
 *          1. Trigger transplant back from FPGA
 *          2. Reexecute instruction
 *          3. Return to FPGA when exception is done
 * @param cpu               Working CPU
 * @param addr              Guest Virtual Address to translate
 * @param acces_type        Access type: LOAD/STORE/INSTR FETCH
 * @param hpaddr            Return Host virtual address associated
 * @return                  uint64_t of value at guest address
 */
bool devteroflex_get_paddr(CPUState *cpu, uint64_t addr, uint64_t access_type,  uint64_t *hpaddr);

/**
 * @brief devteroflex_get_page Translates from guest virtual address to host virtual address
 * NOTE: In case of FAULT, the caller should:
 *          1. Trigger transplant back from FPGA
 *          2. Reexecute instruction
 *          3. Return to FPGA when exception is done
 * @param cpu               Working CPU
 * @param addr              Guest Virtual Address to translate
 * @param acces_type        Access type: LOAD/STORE/INSTR FETCH
 * @param host_phys_page    Returns Address host virtual page associated
 * @param page_size         Returns page size
 * @return                  If 0: Success, else FAULT was generated
 */
bool devteroflex_get_ppage(CPUState *cpu, uint64_t addr, uint64_t access_type,  uint64_t *host_phys_page, uint64_t *page_size);

int devteroflex_singlestepping_flow(void);

#endif /* DEVTEROFLEX_H */
