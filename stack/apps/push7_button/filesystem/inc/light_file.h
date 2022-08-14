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
void light_file_set_light_detection_mode(bool state);
bool light_file_get_light_detection_mode();
void light_file_set_current_light_as_low_threshold();
void light_file_set_current_light_as_high_threshold();

#endif