#ifndef LIGHT_FILE_H
#define LIGHT_FILE_H

#include "errors.h"
#include "stdint.h"


error_t light_files_initialize();
void light_file_set_measure_state(bool enable);
void light_file_set_test_mode(bool enable);
bool light_file_is_enabled();
void light_file_set_enabled(bool enable);
void light_file_set_interval(uint32_t interval);
void light_file_execute_measurement();
void light_file_transmit_config_file();

#endif