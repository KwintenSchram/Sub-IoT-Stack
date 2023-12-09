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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hwleds.h"
#include "hwsystem.h"

#include "d7ap_fs.h"
#include "debug.h"
#include "log.h"
#include "scheduler.h"
#include "timer.h"

#include "alp_layer.h"
#include "d7ap.h"
#include "dae.h"

#include "platform.h"

#include "adc_stuff.h"
#include "button.h"
#include "led.h"
#include "little_queue.h"


#include "hwgpio.h"
#include "math.h"
#include "platform.h"
#include "scheduler.h"
#include "sensor_manager.h"
#include "stm32_common_gpio.h"

#include "button_file.h"
#include "little_queue.h"
#include "device_state_file.h"

#ifdef FRAMEWORK_SENSOR_MANAGER_LOG
#define DPRINT(...) log_print_string(__VA_ARGS__)
#define DPRINT_DATA(...) log_print_data(__VA_ARGS__)
#else
#define DPRINT(...)
#define DPRINT_DATA(...)
#endif

static bool current_transmit_state = false;
static bool current_testmode_state = false;

void sensor_manager_init()
{
    // global settings, versions and voltage files
    device_state_files_initialize();
    // button files
    button_files_initialize();
}

void sensor_manager_set_transmit_state(bool state)
{
    if (state == current_transmit_state)
        return;

    // enable or disable transmission of all sensor files
    device_state_file_set_measure_state(state);
    button_file_set_measure_state(state);
    current_transmit_state = state;
}

