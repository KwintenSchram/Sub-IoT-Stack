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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "button.h"
#include "button_file.h"
#include "debug.h"
#include "led.h"
#include "little_queue.h"
#include "log.h"
#include "scheduler.h"
#include "sensor_manager.h"
#include "state_machine_file.h"
#include "device_state_file.h"

#define FRAMEWORK_APP_LOG 1
#ifdef FRAMEWORK_APP_LOG
#include "log.h"
#define DPRINT(...) log_print_string(__VA_ARGS__)
#define DPRINT_DATA(...) log_print_data(__VA_ARGS__)
#else
#define DPRINT(...)
#define DPRINT_DATA(...)
#endif

#define STATE_COUNTER_EVENT_SEC TIMER_TICKS_PER_SEC * 1

typedef enum {
    BUTTON1_EVENT = 0,
    HALL_EFFECT_EVENT = 3,
    STATE_COUNTER_EVENT = 4,
    WATER_INPUT_EVENT = 5,
    CHARGING_INPUT_EVENT = 6
} input_type_t;



static void app_state_input_event_handler(uint8_t i, bool mask);
static void userbutton_callback(uint8_t button_id, uint8_t mask, buttons_state_t buttons_state);
static APP_STATE_t current_app_state = BOOTED_STATE;
static APP_STATE_t previous_app_state = BOOTED_STATE;
static buttons_state_t current_buttons_state = NO_BUTTON_PRESSED;
static buttons_state_t previous_buttons_state = NO_BUTTON_PRESSED;
static buttons_state_t max_buttons_state = NO_BUTTON_PRESSED;
static buttons_state_t prev_max_buttons_state = NO_BUTTON_PRESSED;
static input_type_t prev_input_type = STATE_COUNTER_EVENT;
static uint8_t operational_event_timer_counter = 0;
static buttons_state_t booted_button_state;
static bool initial_button_press_released = false;
static bool sensor_enabled_state_array[ALL_BUTTONS_PRESSED+1];
static uint32_t new_sensor_interval = 0;
static bool current_high_power_led_state = false;
static bool previous_high_power_led_state = false;

static void userbutton_callback(uint8_t button_id, uint8_t mask, buttons_state_t buttons_state)
{
    current_buttons_state = buttons_state;
    app_state_input_event_handler(button_id, mask);
}

static void switch_state(APP_STATE_t new_state)
{
    DPRINT("entering a new state: %d", new_state);
    current_app_state = new_state;

    // write new state to the state machine file (only usefull when state before reset is needed)
    previous_app_state = state_machine_file_switch_state(current_app_state);

    // only enable sensors and transmit when in operational or test state
    sensor_manager_set_transmit_state(new_state == OPERATIONAL_STATE);
}

static void operational_input_event_handler(input_type_t i, bool mask) 
{
    switch (i) {
    case CHARGING_INPUT_EVENT:
        if(mask)
            led_off(0);
        else
            led_on(0);
        break;
    case BUTTON1_EVENT:
        current_high_power_led_state = mask ? !current_high_power_led_state : current_high_power_led_state;
        break;
    case WATER_INPUT_EVENT:
        current_high_power_led_state = mask ? true : current_high_power_led_state;
    default:
        break;
    }

    if(previous_high_power_led_state != current_high_power_led_state)
    {
        led_set(1, current_high_power_led_state);
        device_state_file_set_high_power_led_state(current_high_power_led_state);
        previous_high_power_led_state = current_high_power_led_state;
    }
}


// this is the main input handler which will forward the input to the relevant state handler
static void app_state_input_event_handler(uint8_t i, bool mask)
{
    switch (current_app_state) {
    case OPERATIONAL_STATE:
        operational_input_event_handler(i, mask);
        break;
    default:
        break;
    }
}

/**
 * @brief Start of the application software
 */
void bootstrap()
{
    // initialize the network queue
    little_queue_init();

    // initialize buttons
    button_file_register_cb(&userbutton_callback);

    // initialize a file that keeps the current and previous global state
    state_machine_file_initialize();

    // initialize all files related to sensors and their configuration
    sensor_manager_init(&app_state_input_event_handler);

    // depending on the initial button state, we should go to a different global state

    led_flash(1);
    switch_state(OPERATIONAL_STATE);

    log_print_string("Device booted %d\n", booted_button_state);
}
