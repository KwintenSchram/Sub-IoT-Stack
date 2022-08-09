#ifndef PUSH7_STATE_FILE_H
#define PUSH7_STATE_FILE_H

#include "errors.h"
#include "stdint.h"

error_t push7_state_files_initialize();
void push7_state_file_set_measure_state(bool enable);
void push7_state_file_set_test_mode(bool enable);
bool push7_state_file_is_enabled();
void push7_state_file_set_enabled(bool enable);
void push7_state_file_set_interval(uint32_t interval);
bool push7_flash_is_led_enabled();
void push7_flash_set_led_enabled(bool state);

#endif