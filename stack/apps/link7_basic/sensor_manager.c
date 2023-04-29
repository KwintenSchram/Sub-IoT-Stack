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
#include "hall_effect_file.h"
#include "little_queue.h"
#include "push7_state_file.h"

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
    push7_state_files_initialize();
    // hall effect (magnetic field switch) files
    hall_effect_files_initialize();
    // button files
    // button_files_initialize();
}

void sensor_manager_set_transmit_state(bool state)
{
    if (state == current_transmit_state)
        return;

    // enable or disable transmission of all sensor files
    push7_state_file_set_measure_state(state);
    hall_effect_file_set_measure_state(state);
    // button_file_set_measure_state(state);

    current_transmit_state = state;
}

void sensor_manager_set_test_mode(bool enable)
{
    if (enable == current_testmode_state)
        return;

    // enable or disable test mode on all sensor files

    // test mode will set all sensors to use a shorter transmission interval, to send on every action and to also send on button presses

    DPRINT("setting test mode: %d", enable);
    
    push7_state_file_set_test_mode(enable);
    hall_effect_file_set_test_mode(enable);
    // button_file_set_test_mode(enable);
    current_testmode_state = enable;
}

void sensor_manager_set_sensor_states(bool sensor_enabled_state_array[])
{
    // in sensor configuration state, the sensors can be enabled or disabled individually. 
    // This passes an array of booleans which enable or disable the sensors

    DPRINT("setting enable states");
    DPRINT_DATA(sensor_enabled_state_array, 6);
    hall_effect_file_set_enabled(sensor_enabled_state_array[HALL_EFFECT_SENSOR_INDEX]);
    push7_flash_set_led_enabled(sensor_enabled_state_array[QUEUE_LIGHT_STATE]);
    push7_state_file_set_high_tx_power_state(sensor_enabled_state_array[HIGH_TX_POWER_STATE]);

    // DPRINT("SET HUMIDITY %d, LIGHT %d, PIR %d, HALL_EFFECT %d, BUTTON %d, QUEUE LED %d",
    //     sensor_enabled_state_array[HUMIDITY_SENSOR_INDEX], sensor_enabled_state_array[LIGHT_SENSOR_INDEX],
    //     sensor_enabled_state_array[PIR_SENSOR_INDEX], sensor_enabled_state_array[HALL_EFFECT_SENSOR_INDEX],
    //     sensor_enabled_state_array[BUTTON_SENSOR_INDEX], sensor_enabled_state_array[QUEUE_LIGHT_STATE]);
}

void sensor_manager_set_interval(uint32_t interval)
{
    // changing the interval will only change the sensors that use interval based transmissions

    DPRINT("setting sensor interval %d", interval);
}

void sensor_manager_get_sensor_states(bool sensor_enabled_state_array[])
{
    sensor_enabled_state_array[HALL_EFFECT_SENSOR_INDEX] = hall_effect_file_is_enabled();
    sensor_enabled_state_array[QUEUE_LIGHT_STATE] = push7_flash_is_led_enabled();
    sensor_enabled_state_array[HIGH_TX_POWER_STATE] = push7_state_file_get_high_tx_power_state();
    DPRINT("getting enable states");
    DPRINT_DATA(sensor_enabled_state_array, 6);
    // DPRINT("GET HUMIDITY %d, LIGHT %d, PIR %d, HALL_EFFECT %d, BUTTON %d, QUEUE LED %d",
    //     sensor_enabled_state_array[HUMIDITY_SENSOR_INDEX], sensor_enabled_state_array[LIGHT_SENSOR_INDEX],
    //     sensor_enabled_state_array[PIR_SENSOR_INDEX], sensor_enabled_state_array[HALL_EFFECT_SENSOR_INDEX],
    //     sensor_enabled_state_array[BUTTON_SENSOR_INDEX], sensor_enabled_state_array[QUEUE_LIGHT_STATE]);
}

void sensor_manager_measure_sensor(uint8_t sensor)
{
    //trigger a measurement manually, only used in test mode


    if (sensor == 2)
        push7_state_file_execute_measurement();
}

void sensor_manager_send_config_files()
{
    // send all sensor configuration files
    push7_state_file_transmit_config_file();
    hall_effect_file_transmit_config_file();
}
