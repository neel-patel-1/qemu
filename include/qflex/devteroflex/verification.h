#ifndef DEVTEROFLEX_VERIFICATION_H
#define DEVTEROFLEX_VERIFICATION_H

#include "devteroflex.h"
#include "devteroflex.pb-c.h"

#define MAX_MEM_INSTS                   (2)     // Pair instructions
#define CACHE_BLOCK_SIZE    (64)

typedef struct CommitTrace {
    DevteroflexArchState state;
    uint32_t inst;
    uint64_t mem_addr[MAX_MEM_INSTS];
    uint64_t mem_data[MAX_MEM_INSTS];
    uint8_t inst_block_data[CACHE_BLOCK_SIZE];
    uint8_t mem_block_data[MAX_MEM_INSTS][CACHE_BLOCK_SIZE];
} CommitTrace;

bool gen_verification(void);
void gen_verification_start(size_t nb_insn);
void gen_verification_end(void);

void gen_verification_inst_cancelled(void);

void gen_verification_add_state(CPUState* cpu, uint64_t addr);
void gen_verification_add_mem(CPUState* cpu, uint64_t addr);

/* ------ Protobuf ------ */
void *devteroflex_pack_protobuf(DevteroflexArchState *devteroflex, size_t *size);
void devteroflex_unpack_protobuf(DevteroflexArchState *devteroflex, void *stream, size_t size);

void devteroflex_trace_protobuf_open(DevteroflexCommitTraceP *traceP);
void devteroflex_trace_protobuf_close(DevteroflexCommitTraceP *traceP);
void devteroflex_trace_protobuf_pack(CommitTrace *trace, DevteroflexCommitTraceP *traceP, uint8_t *stream, int *size);

#endif
