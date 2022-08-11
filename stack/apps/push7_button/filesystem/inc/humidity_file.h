#ifndef HUMIDITY_FILE_H
#define HUMIDITY_FILE_H

#include "errors.h"
#include "stdint.h"

error_t humidity_files_initialize();
void humidity_file_set_measure_state(bool enable);
void humidity_file_set_test_mode(bool enable);
bool humidity_file_is_enabled();
void humidity_file_set_enabled(bool enable);
void humidity_file_set_interval(uint32_t interval);
void humidity_file_execute_measurement();
void humidity_file_transmit_config_file();
#endif