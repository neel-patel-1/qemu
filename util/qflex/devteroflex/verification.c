#include <stdio.h>
#include <stdlib.h>

#include "qflex/qflex.h"
#include "qflex/qflex-traces.h"
#include "qflex/qflex-arch.h"
#include "qflex/devteroflex/devteroflex.h"
#include "qflex/devteroflex/devteroflex.pb-c.h"
#include "qflex/devteroflex/file-ops.h"
#include "qflex/devteroflex/verification.h"

static bool gen_trace = false;
bool gen_verification(void) { return gen_trace; }

static bool hasTrace = false;
static int curr_mem = 0;
static size_t inst_count = 0;
static size_t max_inst = 0;

static CommitTrace traceState;
static DevteroflexCommitTraceP traceProbuf = DEVTEROFLEX_COMMIT_TRACE_P__INIT;
static DevteroflexArchStateP stateProbuf = DEVTEROFLEX_ARCH_STATE_P__INIT;
static uint8_t streamProtobuf[1024];
static int streamSize;
static FILE* streamFile;

/* ------ QEMU ------- */
void gen_verification_start(size_t nb_insn) {
    gen_trace = true;
    hasTrace = false;
    curr_mem = 0;
    inst_count = 0;
    max_inst = nb_insn;
    traceProbuf.state = &stateProbuf;

    devteroflex_trace_protobuf_open(&traceProbuf);
    file_stream_open(&streamFile, "DevteroflexVerificionBinary");
    qflex_mem_trace_gen_helper_start();
}


void gen_verification_end(void) {
    gen_trace = false;
    curr_mem = 0;
    inst_count = 0;
    hasTrace = false;
    gen_trace = false;
    devteroflex_trace_protobuf_close(&traceProbuf);
    fclose(streamFile);
    qflex_mem_trace_gen_helper_stop();
}

void gen_verification_inst_cancelled(void) {
    hasTrace = false;
    qemu_log("Trace Cancelled");
}

void gen_verification_add_state(CPUState* cpu, uint64_t addr) {
    if(hasTrace) {
        // When hits next instruction, last instruction is done so 
        // send the full instruction information
        devteroflex_trace_protobuf_pack(&traceState, &traceProbuf, streamProtobuf, &streamSize);
        file_stream_write(streamFile, &streamSize, sizeof(streamSize));
        file_stream_write(streamFile, streamProtobuf, streamSize);
        qemu_log("PC:0x%016"PRIx64"\n", traceProbuf.state->pc);
        //qemu_log("hva_block addr : %08"PRIx64"\n", traceState.inst_block_data);

        // Reset trace values
        curr_mem = 0;
        for(int i = 0; i < MAX_MEM_INSTS; i++) {
            traceState.mem_addr[i] = 0;
            traceState.mem_addr[i] = 0;
        }
        if(inst_count >= max_inst) {
            gen_verification_end();
        }
    }

    devteroflex_pack_archstate(&traceState.state, cpu);
    uint64_t hva = gva_to_hva(cpu, addr, INST_FETCH);
    if(hva == -1) {
        return;
    }
    uint32_t inst = *((int32_t *) hva);
    traceState.inst = inst;

    uint64_t hva_block = hva & ~(CACHE_BLOCK_SIZE-1);
    memcpy(&traceState.inst_block_data[0], (void *) hva_block, CACHE_BLOCK_SIZE);

    hasTrace = true;
    inst_count++;
}

void gen_verification_add_mem(CPUState* cpu, uint64_t addr) {
    if(curr_mem == 2) { 
        // No more mem insts slots left in CommitTrace struct
        // Pipeline doesn't support instructions with more that 2 memory accesses
        return; 
    }

    uint64_t hva = gva_to_hva(cpu, addr, DATA_LOAD);
    if(hva == -1) {
        return;
    }
    uint64_t data = *((uint64_t *) hva);
    traceState.mem_addr[curr_mem] = addr;
    traceState.mem_data[curr_mem] = data;

    uint64_t hva_block = hva & ~(CACHE_BLOCK_SIZE-1);

    memcpy(&traceState.mem_block_data[curr_mem], (void *) hva_block, CACHE_BLOCK_SIZE);


    curr_mem++;
}
