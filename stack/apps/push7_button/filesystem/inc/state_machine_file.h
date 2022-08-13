#ifndef STATE_MACHINE_FILE_H
#define STATE_MACHINE_FILE_H

#include "errors.h"
#include "stdint.h"

error_t state_machine_file_initialize();
uint8_t state_machine_file_switch_state(uint8_t state);

#endif