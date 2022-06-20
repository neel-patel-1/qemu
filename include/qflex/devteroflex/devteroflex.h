#ifndef DEVTEROFLEX_H
#define DEVTEROFLEX_H

#include <stdbool.h>

#include "qemu/osdep.h"
#include "qemu/thread.h"
#include "qflex/qflex.h"
#include "qflex/devteroflex/fpga/fpga_interface.h"

#ifndef PAGE_SIZE
#define PAGE_SIZE (4096LLU)
#endif

#ifndef MemoryAccessType
// See cpu.h to match MMUAccessType
typedef enum MemoryAccessType {
    DATA_LOAD  = 0,
    DATA_STORE = 1,
    INST_FETCH = 2
} MemoryAccessType;
#define MemoryAccessType
#endif

typedef struct DevteroflexFastForward {
    bool enabled;
    bool running;
    uint64_t icount_target;
    uint64_t icount_curr;
} DevteroflexFastForward;

typedef struct DevteroflexConfig {
    bool enabled;
    bool running;
    int debug_mode;
    bool pure_singlestep;
    int transplant_type;
    uint64_t icount;
    DevteroflexFastForward fast_forward;
} DevteroflexConfig;

typedef enum Transplant_t {
    TRANS_CLEAR = 0,
    TRANS_EXCP  = 1,
    TRANS_UNDEF = 2,
    TRANS_DEBUG = 3,
    TRANS_ICOUNT = 4,
    TRANS_UNKNOWN
} Transplant_t;

typedef enum DevteroFlexDebugMode_t {
    no_debug    = 0,
    no_mem_sync = 1,
    mem_sync    = 2
} DevteroFlexDebugMode_t;

extern DevteroflexConfig devteroflexConfig;
extern FPGAContext c;
extern MessageFPGA message_buffer[256];
extern uint64_t message_buffer_curr_entry;


void devteroflex_init(bool enabled, bool run, size_t fpga_physical_pages, int debug_mode, bool pure_singlestep);

static inline void devteroflex_start(void) {
    qflex_tb_flush();
    if(devteroflexConfig.enabled){
        if(!devteroflexConfig.fast_forward.enabled) {
            devteroflexConfig.running = true;
            qflexState.exit_main_loop = true;
            qemu_log("DEVTEROFLEX: Start detected.\n");
        } else {
            devteroflexConfig.fast_forward.running = true;
            qemu_log("DEVTEROFLEX: Fast forward: %lu insts\n", devteroflexConfig.fast_forward.icount_target);
        }
    } else {
        qemu_log("Warning: Devteroflex is not enabled. The DEVTEROFLEX_START instruction is ignored. We will still print icount values since start\n");
        devteroflexConfig.fast_forward.running = true;
        qemu_loglevel |= CPU_LOG_TB_IN_ASM;
        qemu_loglevel |= CPU_LOG_INT;
    }
}

static inline void devteroflex_stop(void) {
    qflex_tb_flush();
    if(devteroflexConfig.enabled){
        devteroflexConfig.running = false;
        printf("DEVTEROFLEX: Stop detected.\n");
        qemu_log("DEVTEROFLEX: Stop detected.\n");
    } else {
        qemu_log("Warning: Devteroflex is not enabled. The DEVTEROFLEX_STOP instruction is ignored. \n");
        qemu_loglevel &= ~CPU_LOG_TB_IN_ASM;
        qemu_loglevel &= ~CPU_LOG_INT;
    }
}

void devteroflex_stop_full(void);

/* The following functions are architecture specific, so they are
 * in the architecture target directory.
 * (target/arm/devteroflex-helper.c)
 */


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
 * @brief Compare the QEMU CPUState with Devteroflex arch state.
 * 
 * @param cpu the QEMU CPUState
 * @param devteroflex the Devteroflex arch state
 * 
 * @return true if any register mismatch is detected.
 */
bool devteroflex_compare_archstate(CPUState *cpu, DevteroflexArchState *devteroflex);

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


// Usefull flags on conditions to sync pages:

static inline bool pre_mem_sync_page(void) {
    bool transplant_sync = devteroflexConfig.enabled && devteroflexConfig.running && (devteroflexConfig.transplant_type == TRANS_UNDEF || devteroflexConfig.transplant_type == TRANS_EXCP || devteroflexConfig.transplant_type == TRANS_ICOUNT);
    bool post_run_sync = devteroflexConfig.enabled && !devteroflexConfig.running;
    return transplant_sync || post_run_sync;
}

static inline bool post_mem_sync_page(void) {
    return devteroflexConfig.enabled && devteroflexConfig.running && devteroflexConfig.debug_mode == mem_sync && devteroflexConfig.transplant_type == TRANS_DEBUG;
}

static inline bool debug_cmp_mem_sync(void) {
    return devteroflexConfig.debug_mode == mem_sync && devteroflexConfig.transplant_type == TRANS_DEBUG;
}

static inline bool debug_cmp_no_mem_sync(void) {
    return devteroflexConfig.debug_mode == no_mem_sync && (
        devteroflexConfig.transplant_type == TRANS_UNDEF || 
        devteroflexConfig.transplant_type == TRANS_EXCP ||
        devteroflexConfig.transplant_type == TRANS_ICOUNT ||
        devteroflexConfig.transplant_type == TRANS_CLEAR);
}

void devteroflex_config_fast_forward(uint64_t target);

/**
 * @brief This mirrors `icount_update` but instead of taking the TCG executed, we take Devteroflex counters
 */
void icount_update_devteroflex_executed(CPUState *cpu, uint64_t executed);
void devteroflex_icount_update(uint64_t executed);

#endif /* DEVTEROFLEX_H */
