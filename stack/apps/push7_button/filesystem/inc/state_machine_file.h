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
 * @author info@liquibit.be
 */
#ifndef STATE_MACHINE_FILE_H
#define STATE_MACHINE_FILE_H

#include "errors.h"
#include "stdint.h"


typedef enum {
    BOOTED_STATE,
    OPERATIONAL_STATE,
    SENSOR_CONFIGURATION_STATE,
    INTERVAL_CONFIGURATION_STATE,
    TEST_STATE,
    SLEEP_STATE,
    LIGHT_DETECTION_CONFIGURATION_STATE,
} APP_STATE_t;

error_t state_machine_file_initialize();
uint8_t state_machine_file_switch_state(APP_STATE_t state);

#endif