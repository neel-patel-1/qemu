#include <stdbool.h>
#include <assert.h>

typedef struct {
    uint64_t addr;
    uint8_t type;
    bool is_user;
    unsigned int vcpu_idx;
} AccessReq;

typedef struct {
    size_t size;
    size_t enq_ptr;
    size_t deq_ptr;
    AccessReq *fifo;
} FifoQueue;

enum {DATA_LOAD, DATA_STORE, INSN_FETCH};

typedef struct {
    int cores; 
    char *filename;
    int l1_iassoc;
    int l1_iblksize;
    int l1_icachesize;
    int l1_dassoc; 
    int l1_dblksize;
    int l1_dcachesize;
    int l2_assoc;
    int l2_blksize;
    int l2_cachesize;
    FifoQueue *queue;
} CacheConfig;

void    *sim_init(void *config_);
void     queue_init(FifoQueue *queue, size_t size);
bool     queue_can_push(FifoQueue *queue);
bool     queue_can_pop(FifoQueue *queue);
void     queue_push(FifoQueue *queue, AccessReq addr);
AccessReq queue_pop(FifoQueue *queue);

