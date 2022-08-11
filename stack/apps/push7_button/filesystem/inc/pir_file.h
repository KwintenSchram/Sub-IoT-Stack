#ifndef PIR_FILE_H
#define PIR_FILE_H

#include "errors.h"
#include "stdint.h"


error_t pir_files_initialize();
void pir_file_set_measure_state(bool enable);
void pir_file_set_test_mode(bool enable);
bool pir_file_is_enabled();
void pir_file_set_enabled(bool enable);
void pir_file_transmit_config_file();

#endif