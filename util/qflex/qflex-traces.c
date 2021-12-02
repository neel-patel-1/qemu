#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>

#include "qemu/osdep.h"
#include "qemu/thread.h"

#include "qemu-common.h"
#include "qemu/error-report.h"
#include "qapi/qmp/qerror.h"
#include "qemu/option_int.h"
#include "qemu/main-loop.h"

#include "hw/core/cpu.h"

#include "qflex/qflex.h"
#include "qflex/qflex-log.h"
#include "qflex/qflex-traces.h"

#ifdef CONFIG_DEVTEROFLEX
#include "qflex/devteroflex/file-ops.h"
#endif

// ------ TRACE --------
QflexTraceState_t qflexTraceState = {
#ifdef CONFIG_DEVTEROFLEX
    .traceFiles = NULL,
    .instFiles = NULL,
    .fileInit = false,
#endif
    .total_insts = 0,
    .total_mem = 0,
    .total_ld = 0,
    .total_st = 0,
    .total_trace_insts = 0,
    .gen_helper = false,
    .gen_trace = false,
    .gen_inst_trace_type = 0
};

void qflex_mem_trace_init(int core_count) {
    qflexTraceState.total_insts = 0;
    qflexTraceState.total_mem = 0;
    qflexTraceState.gen_helper = false;
    qflexTraceState.gen_trace = false;
    qflexTraceState.gen_inst_trace_type = false;

#ifdef CONFIG_DEVTEROFLEX
    qflexTraceState.traceFiles = calloc(core_count, sizeof(FILE*));
    qflexTraceState.fileInit = true;
    char filename[sizeof "mem_trace_00"];
    for(int i = 0; i < core_count; i++) {
        if(i > 64) exit(EXIT_FAILURE);
        sprintf(filename, "mem_trace_%02d", i);
        qemu_log("Filename %s\n", filename);
        file_stream_open(&qflexTraceState.traceFiles[i], filename);
    }

    // Instruction trace
    qflexTraceState.instFiles = calloc(core_count, sizeof(FILE*));

    char filenameInst[sizeof "inst_trace_00"];
    for(int i = 0; i < core_count; i++) {
         if(i > 64) exit(EXIT_FAILURE);
        sprintf(filenameInst, "inst_trace_%02d", i);
        qemu_log("Filename %s\n", filename);
        file_stream_open(&qflexTraceState.instFiles[i], filenameInst);
    }
#endif
}

void qflex_inst_trace(uint32_t cpu_index, uint64_t asid, uint32_t inst) {
#ifdef CONFIG_DEVTEROFLEX
    file_stream_write(qflexTraceState.instFiles[cpu_index], &inst, sizeof(inst));
#endif
    qflex_log_mask(QFLEX_LOG_MEM_TRACE_INST, "CPU[%"PRIu32"]:ASID[%"PRIu64"]:INST[0x%016"PRIx32"]\n", cpu_index, asid, inst);
}

void qflex_inst_trace_medium(uint32_t cpu_index, QflexInstTraceMedium_t *trace) {
#ifdef CONFIG_DEVTEROFLEX
    file_stream_write(qflexTraceState.instFiles[cpu_index], trace, sizeof(QflexInstTraceMedium_t));
#endif
}

void qflex_inst_trace_full(QflexInstTraceFull_t *trace) {
#ifdef CONFIG_DEVTEROFLEX
    file_stream_write(qflexTraceState.instFiles[trace->cpu_index], trace, sizeof(QflexInstTraceFull_t));
#endif
    qflex_log_mask(QFLEX_LOG_MEM_TRACE_INST, "CPU[%"PRIu32"]:ASID[%04"PRIu64"]:TID[%04"PRIu64"]:PC[0x%016"PRIx64"]:INST[0x%08"PRIx32"]\n", trace->cpu_index, trace->asid, trace->tid, trace->pc, trace->inst);
}

typedef struct MemTraceReq_t {
    uint32_t type;
    uint64_t vaddr;
    uint64_t hwaddr;
} MemTraceReq_t ;

void qflex_mem_trace_memaccess(uint64_t vaddr, uint64_t hwaddr, uint64_t cpu_index, uint64_t type, int el) {
    qflex_log_mask(QFLEX_LOG_LDST, "CPU[%"PRIu64"]:MEM[%"PRIu64"]:VADDR[0x%016"PRIx64"]:PADDR[0x%016"PRIx64"]\n", cpu_index, type, vaddr, hwaddr);

#ifdef CONFIG_DEVTEROFLEX
    MemTraceReq_t trace = {.vaddr = vaddr, .hwaddr = hwaddr, .type = type};
    file_stream_write(qflexTraceState.traceFiles[cpu_index], &trace, sizeof(trace));
#endif

    switch (type) {
    case MMU_DATA_LOAD:
        qflexTraceState.total_ld++;
        qflexTraceState.total_mem++;
        break;
    case MMU_DATA_STORE:
        qflexTraceState.total_st++;
        qflexTraceState.total_mem++;
        break;
    case MMU_INST_FETCH:
        qflexTraceState.total_insts++;
        break;
    }

    if (qflexTraceState.total_insts >= qflexTraceState.total_trace_insts) {
        qflex_mem_trace_log_direct();
        qflex_mem_trace_end();
        printf("QFLEX: Memory Trace DONE\n");
    }
}

void qflex_mem_trace_gen_helper_start(void) {
    qflexTraceState.gen_helper = true;
    qflex_tb_flush();
}

void qflex_mem_trace_gen_helper_stop(void) {
    qflexTraceState.gen_helper = false;
    qflex_tb_flush();
}

void qflex_mem_trace_start(size_t nb_insn, int trace_type) {
    qflexTraceState.total_trace_insts = nb_insn;
    qflexTraceState.gen_helper = true;
    qflexTraceState.gen_trace = true;
    qflexTraceState.gen_inst_trace_type = trace_type;
    qflex_tb_flush();
}

void qflex_mem_trace_stop(void) {
    qflexTraceState.gen_helper = false;
    qflexTraceState.gen_trace = false;
    qflex_tb_flush();
}

void qflex_mem_trace_end(void) {
    qflexTraceState.total_insts = 0;
    qflexTraceState.total_mem = 0;
    qflexTraceState.gen_helper = false;
    qflexTraceState.gen_trace = false;
    qflex_tb_flush();
}

bool qflex_mem_trace_gen_helper(void) { return qflexTraceState.gen_helper; }
bool qflex_mem_trace_gen_trace(void) { return qflexTraceState.gen_trace; }

void qflex_mem_trace_log_stats(char *buffer, size_t max_size) {
    size_t tot_chars;
    tot_chars = snprintf(buffer, max_size,
        "Memory System statistics:\n"
        "  TOT Fetch Insts: %zu\n"
        "  TOT LD/ST Insnts: %zu\n",
    qflexTraceState.total_insts, qflexTraceState.total_mem);

    assert(tot_chars < max_size);
}

void qflex_mem_trace_log_direct(void) {
    char str[256];
    qflex_mem_trace_log_stats(str, 256);
    qemu_log("%s", str);
}

// ------ END TRACE --------
