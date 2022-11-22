/*
 * Copyright (c) 2015-2021 University of Antwerp, Aloxy NV, LiQuiBit VOF.
 *
 * This file is part of Sub-IoT.
 * See https://github.com/Sub-IoT/Sub-IoT-Stack for further info.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* \file
 *
 *
 * @author contact@liquibit.be
 */
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
void push7_state_file_execute_measurement();
void push7_state_file_transmit_config_file();
void push7_state_file_set_high_tx_power_state(bool enable_high_tx_power);
bool push7_state_file_get_high_tx_power_state();

#endif