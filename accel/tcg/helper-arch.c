#include <stdint.h>
#include "qemu/osdep.h"
#include "cpu.h"
#include "exec/helper-arch.h"
#include "exec/log.h"

static bool is_running = false;

void arch_magic_inst(uint64_t op) {
    if (op == OP_START_ACTION) {
        printf("Magic inst: start\n");
        qemu_log("Magic inst: start\n");
        is_running = true;
    } else if (op == OP_STOP_ACTION) {
        printf("Magic inst: stop\n");
        qemu_log("Magic inst: stop\n");
        is_running = false;
    } else {
        printf("Magic inst: UNKNOWN\n");
        qemu_log("Magic inst: UNKNOWN\n");
    }
}

