#ifndef HALL_EFFECT_FILE_H
#define HALL_EFFECT_FILE_H

#include "errors.h"
#include "stdint.h"

error_t hall_effect_files_initialize();
void hall_effect_file_set_measure_state(bool enable);
void hall_effect_file_set_test_mode(bool enable);
bool hall_effect_file_is_enabled();
void hall_effect_file_set_enabled(bool enable);


#endif