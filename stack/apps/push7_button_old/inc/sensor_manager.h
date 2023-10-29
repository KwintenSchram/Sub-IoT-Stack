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
#ifndef __NETWORK_MANAGER_H
#define __NETWORK_MANAGER_H

#include "button.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    HUMIDITY_SENSOR_INDEX = BUTTON1_PRESSED,
    LIGHT_SENSOR_INDEX = BUTTON2_PRESSED,
    PIR_SENSOR_INDEX = BUTTON3_PRESSED,
    HALL_EFFECT_SENSOR_INDEX = BUTTON1_2_PRESSED,
    BUTTON_SENSOR_INDEX = BUTTON1_3_PRESSED,
    QUEUE_LIGHT_STATE = BUTTON2_3_PRESSED,
    HIGH_TX_POWER_STATE = ALL_BUTTONS_PRESSED
} SENSOR_ARRAY_INDEXES;

void sensor_manager_init();
void sensor_manager_set_transmit_state(bool state);
void sensor_manager_set_test_mode(bool enable);
void sensor_manager_set_sensor_states(bool sensor_enabled_state_array[]);
void sensor_manager_get_sensor_states(bool sensor_enabled_state_array[]);
void sensor_manager_set_interval(uint32_t interval);
void sensor_manager_measure_sensor(uint8_t sensor);
void sensor_manager_send_config_files();
void sensor_manager_set_light_threshold(bool high_threshold);
bool sensor_manager_get_light_detection_state();
void sensor_manager_set_light_detection_state(bool state);

#endif //__NETWORK_MANAGER_H