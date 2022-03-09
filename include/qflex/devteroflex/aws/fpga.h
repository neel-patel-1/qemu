#ifndef FPGA_H
#define FPGA_H

#include <stdint.h>

/**
 * @file the interface for manipulating FPGA.
 */

struct FPGAContext;
typedef struct FPGAContext FPGAContext;


int initFPGAContext(FPGAContext *c);
int readAXIL(const FPGAContext *c, uint32_t addr, uint32_t *data);
int writeAXIL(const FPGAContext *c, uint32_t addr, uint32_t data);
int readAXI(const FPGAContext *c, uint64_t addr, void *data);
int writeAXI(const FPGAContext *c, uint64_t addr, void *data);
int releaseFPGAContext(FPGAContext *c);

#endif