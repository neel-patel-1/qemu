#include "devteroflex.h"

typedef struct DevteroflexGen_t {
    bool example;
    uint64_t max_calls;
    uint64_t nb_calls;
} DevteroflexGen_t;

extern DevteroflexGen_t devteroflexGen;

void devteroflex_gen_example_set(bool set, uint64_t nb_insn);
void devteroflex_example_callback(uint64_t cpu_index, uint64_t arg1, uint64_t arg2);
#define TAG_INSTRUCTION_DECODED 0x100
#define TAG_EXCEPTION_RETURN    0x200
#define TAG_MTE_OPERATION       0x400
