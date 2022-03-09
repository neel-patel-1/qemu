#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>
#include <string.h>

#include "linux/limits.h"

#include "qflex/qflex-log.h"

#include "qflex/devteroflex/file-ops.h"
#include "qflex/devteroflex/devteroflex.h"
#include "qflex/devteroflex/verification.h"
#include "qflex/devteroflex/devteroflex.pb-c.h"

int file_stream_open(FILE **fp, const char *filename) {
    char filepath[PATH_MAX];

    qflex_log_mask(QFLEX_LOG_FILE_ACCESS,
                   "Writing file : "FILE_ROOT_DIR"/%s\n", filename);
    if (mkdir(FILE_ROOT_DIR, 0777) && errno != EEXIST) {
        qflex_log_mask(QFLEX_LOG_FILE_ACCESS,
                       "'mkdir "FILE_ROOT_DIR"' failed\n");
        exit(EXIT_FAILURE);
    }

    snprintf(filepath, PATH_MAX, FILE_ROOT_DIR"/%s", filename);
    *fp = fopen(filepath, "w");
    if(!fp) {
        qflex_log_mask(QFLEX_LOG_FILE_ACCESS,
                       "ERROR: File stream open failed\n"
                       "    filepath:%s\n", filepath);
        exit(EXIT_FAILURE);
    }

    return 0;
}

int file_stream_write(FILE *fp, void *stream, size_t size) {
    if(fwrite(stream, 1, size, fp) != size) {
        fclose(fp);
        qflex_log_mask(QFLEX_LOG_FILE_ACCESS,
                       "Error writing stream to file\n");
        return 1;
    }
    return 0;
}

int file_region_open(const char *filename, size_t size, File_t *file) {
    char filepath[PATH_MAX];
    int fd = -1;
    void *region;
    qflex_log_mask(QFLEX_LOG_FILE_ACCESS,
                   "Writing file : "FILE_ROOT_DIR"/%s\n", filename);
    if (mkdir(FILE_ROOT_DIR, 0777) && errno != EEXIST) {
        qflex_log_mask(QFLEX_LOG_FILE_ACCESS,
                       "'mkdir "FILE_ROOT_DIR"' failed\n");
        return 1;
    }
    snprintf(filepath, PATH_MAX, FILE_ROOT_DIR"/%s", filename);
    if((fd = open(filepath, O_RDWR | O_CREAT | O_TRUNC, 0666)) == -1) {
        qflex_log_mask(QFLEX_LOG_FILE_ACCESS,
                       "Program Page dest file: open failed\n"
                       "    filepath:%s\n", filepath);
        return 1;
    }
    if (lseek(fd, size-1, SEEK_SET) == -1) {
        close(fd);
        qflex_log_mask(QFLEX_LOG_FILE_ACCESS,
                       "Error calling lseek() to 'stretch' the file\n");
        return 1;
    }
    if (write(fd, "", 1) != 1) {
        close(fd);
        qflex_log_mask(QFLEX_LOG_FILE_ACCESS,
                       "Error writing last byte of the file\n");
        return 1;
    }

    region = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if(region == MAP_FAILED) {
        close(fd);
        qflex_log_mask(QFLEX_LOG_FILE_ACCESS,
                       "Error dest file: mmap failed");
        return 1;
    }

    file->fd=fd;
    file->region = region;
    file->size = size;
    return 0;
}

void file_region_write(File_t *file, void* buffer) {
    memcpy(file->region, buffer, file->size);
    msync(file->region, file->size, MS_SYNC);
}

void file_region_close(File_t *file) {
    munmap(file->region, file->size);
    close(file->fd);
}

void* open_cmd_shm(const char* name, size_t struct_size){
    int shm_fd; 
    shm_fd = shm_open(name, O_CREAT | O_RDWR, 0666); 
    if (ftruncate(shm_fd, struct_size) < 0) {
        qflex_log_mask(QFLEX_LOG_FILE_ACCESS,"ftruncate for '%s' failed\n",name);
    }
    void* cmd =  mmap(0, struct_size, PROT_READ|PROT_WRITE, MAP_SHARED, shm_fd, 0); 
    return cmd;
}

/* ------ Protobuf ------ */
void *devteroflex_pack_protobuf(DevteroflexArchState *devteroflex, size_t *size) {
    void *stream;
    DevteroflexArchStateP state = 
        DEVTEROFLEX_ARCH_STATE_P__INIT;

    // Init fields
    state.n_xregs = 32;
    state.xregs = malloc(sizeof(uint64_t) * 32); // Allocate memory to store xregs
    assert(state.xregs);

    // devteroflex struct -> protobuf struct
    memcpy(state.xregs, devteroflex->xregs, sizeof(uint64_t) * 32);
    state.pc = devteroflex->pc;
    state.sp = devteroflex->sp;
    state.nzcv = devteroflex->flags;

    // protobuf struct -> protobuf stream
    size_t len = devteroflex_arch_state_p__get_packed_size (&state); // This is calculated packing length
    stream = malloc(len);
    devteroflex_arch_state_p__pack (&state, stream);     // Pack the data

    // Free allocated fields
    free (state.xregs);

    // Return stream
    *size = len;
    return stream;
}

void devteroflex_unpack_protobuf(DevteroflexArchState *devteroflex, void *stream, size_t size) {
    if(devteroflex == NULL) {
        fprintf(stderr, "Alloc Devteroflex state ptr before passing it as argument\n");
        exit(EXIT_FAILURE);
    }

    // protobuf stream -> protobuf struct
    DevteroflexArchStateP *state;
    state = devteroflex_arch_state_p__unpack(NULL, size, stream); // Deserialize the serialized input
    if (state == NULL) {
        fprintf(stderr, "Error unpacking DEVTEROFLEX state protobuf message\n");
        exit(EXIT_FAILURE);
    }

    // protobuf struct -> devteroflex struct
    memcpy(&devteroflex->xregs, &state->xregs, sizeof(uint64_t) * 32);
    devteroflex->pc = state->pc;
    devteroflex->sp = state->sp;
    devteroflex->flags = state->nzcv;

    // Free protobuf struct
    devteroflex_arch_state_p__free_unpacked(state, NULL); // Free the message from unpack()
}


void devteroflex_trace_protobuf_open(DevteroflexCommitTraceP *traceP) {
    // Init fields
    traceP->n_mem_addr = MAX_MEM_INSTS;
    traceP->n_mem_data = MAX_MEM_INSTS;
    traceP->mem_addr = malloc (sizeof (uint64_t) * traceP->n_mem_addr);
    traceP->mem_data = malloc (sizeof (uint64_t) * traceP->n_mem_data);
    assert(traceP->mem_addr);
    assert(traceP->mem_data);

    traceP->inst_block_data.len = CACHE_BLOCK_SIZE;
    traceP->inst_block_data.data = malloc(CACHE_BLOCK_SIZE);

    traceP->n_mem_block_data = MAX_MEM_INSTS;
    traceP->mem_block_data = malloc(sizeof(ProtobufCBinaryData)* traceP->n_mem_block_data);
    for(int i = 0; i < MAX_MEM_INSTS; i++) {
      traceP->mem_block_data[i].len = CACHE_BLOCK_SIZE;
      traceP->mem_block_data[i].data = malloc(CACHE_BLOCK_SIZE);
    }

    // Init State
    DevteroflexArchStateP *state = traceP->state;
    state->n_xregs = 32;
    state->xregs = malloc(sizeof(uint64_t) * state->n_xregs);
    assert(state->xregs);
}

void devteroflex_trace_protobuf_close(DevteroflexCommitTraceP *traceP) {
    // Free allocated fields
    free(traceP->mem_addr);
    free(traceP->mem_data);
    free(traceP->state->xregs);
}

void devteroflex_trace_protobuf_pack(CommitTrace *trace,
                                 DevteroflexCommitTraceP *traceP, 
                                 uint8_t *stream,
                                 int *size) {
    // Pack Commit Trace
    traceP->inst = trace->inst;
    memcpy(traceP->mem_addr, trace->mem_addr, sizeof(uint64_t) * traceP->n_mem_addr);
    memcpy(traceP->mem_data, trace->mem_data, sizeof(uint64_t) * traceP->n_mem_data);

    // Pack Devteroflex State
    DevteroflexArchState *state = &trace->state;
    DevteroflexArchStateP *stateP = traceP->state;
    memcpy(stateP->xregs, state->xregs, sizeof(uint64_t) * stateP->n_xregs);
    stateP->pc = state->pc;
    stateP->sp = state->sp;
    stateP->nzcv = state->flags;

    memcpy(traceP->inst_block_data.data, &(trace->inst_block_data), CACHE_BLOCK_SIZE);
    for (int i = 0; i < traceP->n_mem_block_data; i++) {
      memcpy(traceP->mem_block_data[i].data, &(trace->mem_block_data[i]), CACHE_BLOCK_SIZE);
    }

    // protobuf struct -> protobuf stream
    *size = devteroflex_commit_trace_p__get_packed_size (traceP); // This is calculated packing length
    assert(*size <= 1024);
    devteroflex_commit_trace_p__pack (traceP, stream); // Pack the data
}
