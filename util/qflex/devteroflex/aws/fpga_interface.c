#include "qflex/devteroflex/aws/fpga_interface.h"
#include "qflex/devteroflex/aws/fpga.h"

#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>

/** 
 * @file Definition of functions to exchange data with FPGA.
 */

/**
 * Bind a thread id with process id.
 * @param thread_id the given thread id.
 * @param process_id the process id.
 * @returns 0 if successful.
 * 
 * @note associate with S_AXIL_TT
 */
int transplant_getState(const FPGAContext *c, uint32_t thread_id, uint64_t *state, size_t regCount)
{
    const uint32_t axi_transplant_base = 0x0;
    int ret = 0;
    uint32_t reg_val1;
    uint32_t reg_val2;
    size_t base_offset = axi_transplant_base + (thread_id << 6);
    for (int reg = 0; reg < regCount; reg++)
    {
        ret = readAXIL(c, base_offset + 2 * reg, &reg_val1);
        if (ret)
            return ret;
        ret = readAXIL(c, base_offset + 2 * reg + 1, &reg_val2);
        if (ret)
            return ret;
        state[reg] = reg_val1 | ((uint64_t)reg_val2) << 32;
    }
    return 0;
}

int transplant_pushState(const FPGAContext *c, uint32_t thread_id, uint64_t *state, size_t regCount)
{
    const uint32_t axi_transplant_base = 0x0;
    int ret = 0;
    uint32_t reg_val1;
    uint32_t reg_val2;
    size_t base_offset = axi_transplant_base + (thread_id << 6);
    for (int reg = 0; reg < regCount; reg++)
    {
        reg_val1 = (uint64_t)state[reg];
        reg_val2 = ((uint64_t)state[reg] >> 32);
        ret = writeAXIL(c, base_offset + 2 * reg, reg_val1);
        if (ret)
            return ret;
        ret = writeAXIL(c, base_offset + 2 * reg + 1, reg_val2);
        if (ret)
            return ret;
    }
    return 0;
}

int transplant_pending(const FPGAContext *c, uint32_t *pending_threads)
{
    const uint32_t axi_transplant_ctrl_base = 0x0;
    *pending_threads = 0;
    return readAXIL(c, axi_transplant_ctrl_base + 0, pending_threads);
}

int transplant_start(const FPGAContext *c, uint32_t thread_id)
{
    const uint32_t axi_transplant_ctrl_base = 0x0;
    return writeAXIL(c, axi_transplant_ctrl_base + 1, 1 << thread_id);
}

/**
 * Bind a thread id with process id.
 * @param thread_id the given thread id.
 * @param process_id the process id.
 * @returns 0 if successful.
 * 
 * @note associate with S_AXIL_TT
 */
int registerThreadWithProcess(const FPGAContext *c, uint32_t thread_id, uint32_t process_id)
{
    const uint32_t axi_tt_base = 0x0;
    return writeAXIL(c, axi_tt_base + thread_id, process_id);
}

/**
 * Check whether there will be a message from FPGA.
 * @param message the buffer for the message.
 * @returns 0 if successful. 1 for empty Tx queue.
 * 
 * @note associate with S_AXI_QEMU_MQ and S_AXIL_QEMU_MQ
 */
int queryMessageFromFPGA(const FPGAContext *c, uint8_t message[64])
{
    const uint32_t query_base = 0x2000;
    const uint64_t data_base = 0x1000010000;
    uint32_t res = -1;
    readAXIL(c, query_base + 0x4, &res);
    if (res != 0) {
        // message here.
        return readAXI(c, data_base, (uint64_t *)message);
    }
    return 1;
}

/**
 * Push a message to FPGA.
 * @param raw_message the pointer pointing to the message.
 * @param message_size the size of the message.
 * @return 0 if successful. 1 for the full Rx queue.
 * 
 * @note associate with S_AXI_QEMU_MQ
 */
int sendMessageToFPGA(const FPGAContext *c, void *raw_message, size_t message_size)
{
    const uint32_t query_base = 0x2000;
    const uint64_t data_base = 0x1000010000;
    uint32_t res = -1;
    readAXIL(c, query_base, &res);
    uint64_t buffer[8];
    bzero(buffer, 64);
    memcpy(buffer, raw_message, message_size);

    if (res != 0) {
        return writeAXI(c, data_base, buffer);
    }
    return 1;
}

/**
 * Push a page to the page buffer in FPGA. 
 * @param page the starting addrss of the page.
 * @return 0 if successful.
 * 
 * @note associate with S_AXI_PAGE
 */
int pushPageToPageBuffer(const FPGAContext *c, void *page)
{
    const uint64_t base = 0x1000000000;
    return writeAXI(c, base + 64, page);
}

/**
 * Fetch a page from the FPGA and copy it to the buffer.
 * @param buffer the target buffer.
 * @return 0 if successful.
 * 
 * @note associate with S_AXI_PAGE
 */
int fetchPageFromPageBuffer(const FPGAContext *c, void *page)
{
    const uint64_t base = 0x1000000000;
    return readAXI(c, base, page);
}

#ifndef CONFIG_AWS
#include "qflex/devteroflex/aws/fpga.h"
// TODO Bind emulator instead of AWS Shell
int initFPGAContext(FPGAContext *c) { return -1; }
int readAXIL(const FPGAContext *c, uint32_t addr, uint32_t *data) { return -1; }
int writeAXIL(const FPGAContext *c, uint32_t addr, uint32_t data) { return -1; }
int readAXI(const FPGAContext *c, uint64_t addr, void *data) { return -1; }
int writeAXI(const FPGAContext *c, uint64_t addr, void *data) { return -1; }
int releaseFPGAContext(FPGAContext *c) { return -1; }
#endif
