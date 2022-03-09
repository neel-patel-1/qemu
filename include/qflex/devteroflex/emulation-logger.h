#ifndef DEVTEROFLEX_EMULATION_LOGGER_H
#define DEVTEROFLEX_EMULATION_LOGGER_H

#include "devteroflex.h"

/**
 * @file emulation-logger.h
 * 
 * @brief defines functions to generate log under FPGA emulation mode.
 */

/**
 * 
 */
void emulation_log_write(CPUState *state);

/**
 * 
 */
void emulation_log_sync(void);



#endif
