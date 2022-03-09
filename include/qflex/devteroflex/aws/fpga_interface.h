#ifndef FPGA_INTERFACE_H
#define FPGA_INTERFACE_H

#include <stddef.h>
#include <stdint.h>

struct FPGAContext;
typedef struct FPGAContext FPGAContext;

/**
 * @file This file defines the software messages for FPGA communication.
 */

typedef enum MessageType {
    // FPGA -> QEMU
    sPageFault = 4,
    sEvictNotify = 5,
    sEvictDone = 6,
    // QEMU -> FPGA
    sPageEvict = 7,
    sMissReply = 2,
    sEvictReply = 3
} MessageType;

// QEMU requests a page eviction.
typedef struct QEMUPageEvictRequest {
    MessageType type; // constant 7
    uint64_t vpn;
    uint32_t pid;
} QEMUPageEvictRequest;

// QEMU resolves a page fault.
typedef struct QEMUMissReply {
    MessageType type; // constant 2
    uint64_t vpn;
    uint32_t pid;
    uint32_t permission;
    uint32_t synonym_v;
    uint64_t s_vpn;
    uint64_t s_pid;
} QEMUMissReply;

// QEMU confirms a page eviction. 
typedef struct QEMUEvictReply {
    MessageType type; // constant 3
    uint64_t vpn;
    uint32_t pid;
    uint32_t old_ppn;
    uint32_t synonym_v;
} QEMUEvictReply;

// FPGA finds a page fault.
typedef struct PageFaultNotification {
    MessageType type; // constant 4
    uint64_t vpn;
    uint32_t pid;
    uint32_t permission;
} PageFaultNotification;

// type = 5: FPGA starts the page eviction. (No page movement if the modified is not dirty).
// type = 6: FPGA finishes the page eviction (The page is available in the page buffer).
typedef struct PageEvictNotification {
    MessageType type; // maybe 5(start) or 6(done)
    uint64_t vpn;
    uint32_t pid;
    uint32_t ppn;
    uint32_t permission;
    uint32_t modified;
} PageEvictNotification;

// Transplant transactions
int transplant_pushState(const FPGAContext *c, uint32_t thread_id, uint64_t *state, size_t regCount);
int transplant_getState(const FPGAContext *c, uint32_t thread_id, uint64_t* state, size_t regCount);
int transplant_start(const FPGAContext *c, uint32_t thread_id);
int transplant_pending(const FPGAContext *c, uint32_t *threads);
 
// Message transactions
int registerThreadWithProcess(const FPGAContext *c, uint32_t thread_id, uint32_t process_id);
int queryMessageFromFPGA(const FPGAContext *c, uint8_t message[64]);
int sendMessageToFPGA(const FPGAContext *c, void *raw_message, size_t message_size);

// Page transactions
int pushPageToPageBuffer(const FPGAContext *c, void *page);
int fetchPageFromPageBuffer(const FPGAContext *c, void *page);

#endif