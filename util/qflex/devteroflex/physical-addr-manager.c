#include <stdlib.h>
#include <stdbool.h>

#include "util/circular-buffer.h"
#include "qflex/devteroflex/devteroflex.h"
#include "qflex/devteroflex/demand-paging.h"

static cbuf_handle_t cbuf;
static uint64_t* paddr_buff = NULL;

int fpga_paddr_init_manager(size_t tot_physical_pages, uint64_t data_base_addr) {
    assert(!paddr_buff);
    paddr_buff = calloc(tot_physical_pages, PAGE_SIZE);
    if(!paddr_buff) { 
        return -1; 
    }
    cbuf = circular_buf_init(paddr_buff, tot_physical_pages);
    for(int page = 0; page < tot_physical_pages; page++) {
        fpga_paddr_push(page * PAGE_SIZE + data_base_addr);
    }
    return 0;
}

bool fpga_paddr_get(uint64_t *paddr) {
    if(circular_buf_get(cbuf, paddr) == 0) {
        return true;
    } else {
        return false;
    }
}

// Should always have a free paddr spot when returning it
void fpga_paddr_push(uint64_t paddr) {
    assert(!circular_buf_try_put(cbuf, paddr));
}

void fpga_paddr_free_stack(void) {
    free(paddr_buff);
    circular_buf_free(cbuf);
}

