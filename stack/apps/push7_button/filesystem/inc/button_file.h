
#ifndef BUTTON_FILE_H
#define BUTTON_FILE_H

#include "errors.h"
#include "stdint.h"
#include "button.h"

error_t button_files_initialize();
void button_file_set_measure_state(bool enable);
void button_file_set_test_mode(bool enable);
void button_file_register_cb(ubutton_callback_t callback);
bool button_file_is_enabled();
void button_file_set_enabled(bool enable);
void button_file_transmit_config_file();

#endif