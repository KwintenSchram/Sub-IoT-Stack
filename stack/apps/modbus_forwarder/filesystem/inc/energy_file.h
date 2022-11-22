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
#ifndef ENERGY_FILE_H
#define ENERGY_FILE_H

#include "errors.h"
#include "stdint.h"

error_t push7_state_files_initialize();
error_t energy_files_initialize();
void energy_file_transmit_config_file();
void energy_file_execute_measurement();
void energy_file_set_measure_state(bool enable);
void energy_file_set_enabled(bool enable);
void energy_file_set_interval(uint32_t interval);

#endif