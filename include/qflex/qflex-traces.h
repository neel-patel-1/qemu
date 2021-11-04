#ifndef QFLEX_MODELS_H
#define QFLEX_MODELS_H

// Helper insertion locations
#define QFLEX_EXEC_IN           (0)
#define QFLEX_EXCEPTION         (1)

// Flags for gen_trace
#define QFLEX_GEN_TRACE_TINY_INST (0)
#define QFLEX_GEN_TRACE_MEDIUM    (1)
#define QFLEX_GEN_TRACE_FULL_INST (2)

typedef struct QflexTraceState_t {
#ifdef CONFIG_DEVTEROFLEX
    FILE** traceFiles;
    FILE** instFiles;
    bool fileInit;
#endif
    size_t total_insts;
    size_t total_mem;
    size_t total_ld;
    size_t total_st;
    size_t total_trace_insts;
    bool gen_helper;
    bool gen_trace;
    int gen_inst_trace_type;
} QflexTraceState_t;

extern QflexTraceState_t qflexTraceState;

typedef struct QflexInstTraceFull_t {
    uint32_t cpu_index;
    uint32_t inst;
    uint64_t asid;
    uint64_t tid; 
    uint64_t pc;
} QflexInstTraceFull_t;

typedef struct QflexInstTraceMedium_t {
    uint64_t tid;
    uint64_t tid2;
    uint16_t asid;
    uint16_t el;
} QflexInstTraceMedium_t;

// ------ TRACE Memory Requests --------
void qflex_inst_trace(uint32_t cpu_index, uint64_t asid, uint32_t inst);
void qflex_inst_trace_medium(uint32_t cpu_index, QflexInstTraceMedium_t trace);
void qflex_inst_trace_full(QflexInstTraceFull_t trace);

void qflex_mem_trace_init(int core_count);
void qflex_mem_trace_start(size_t nb_insn, int trace_type);
void qflex_mem_trace_stop(void);
void qflex_mem_trace_end(void);
void qflex_mem_trace_memaccess(uint64_t vaddr, uint64_t hwaddr, 
                               uint64_t cpu_index, uint64_t type, int el);

void qflex_mem_trace_gen_helper_start(void);
void qflex_mem_trace_gen_helper_stop(void);
bool qflex_mem_trace_gen_helper(void);
bool qflex_mem_trace_gen_trace(void);

void qflex_mem_trace_log_stats(char* buffer, size_t max_size);
void qflex_mem_trace_log_direct(void);

#endif /* QFLEX_MODELS_H */
