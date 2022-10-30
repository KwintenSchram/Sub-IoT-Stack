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
    BUTTON2_EVENT = 1,
    BUTTON3_EVENT = 2,
    HALL_EFFECT_EVENT,
    STATE_COUNTER_EVENT,
} input_type_t;

static void app_state_input_event_handler(input_type_t i, bool mask);
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

static void userbutton_callback(uint8_t button_id, uint8_t mask, buttons_state_t buttons_state)
{
    current_buttons_state = buttons_state;
    app_state_input_event_handler(button_id, mask);
}

static void switch_state(APP_STATE_t new_state)
{
    DPRINT("entering a new state: %d", new_state);
    current_app_state = new_state;

    // write new state to the state machine file
    previous_app_state = state_machine_file_switch_state(current_app_state);

    // only enable sensors and transmit when in operational or test state
    sensor_manager_set_transmit_state((new_state == OPERATIONAL_STATE) || (new_state == TEST_STATE));

    switch (new_state) {
    case OPERATIONAL_STATE:
        // send the configuration files if we just came from sleep or from a configuration state
        if (previous_app_state != BOOTED_STATE && previous_app_state != OPERATIONAL_STATE
            && previous_app_state != TEST_STATE)
            sensor_manager_send_config_files();
        break;
    case SENSOR_CONFIGURATION_STATE:
        // get from all sensors if they are enabled or not
        sensor_manager_get_sensor_states(sensor_enabled_state_array);
        break;
    case INTERVAL_CONFIGURATION_STATE:
        new_sensor_interval = 0;
        break;
    default:;
        break;
    }
}

// if the sensor is enabled, flash once. If it is disabled, flash twice
static void display_state(bool state) { led_flash(state ? 1 : 2); }

// if we are in operational state, we don't care about the buttons
static void operational_input_event_handler(input_type_t i, bool mask) { }

// if we are in sensor configuration state, the buttons are used to enable or disable sensors
static void sensor_configuration_input_event_handler(input_type_t i, bool mask)
{
    // only use button events
    if (i == STATE_COUNTER_EVENT || i == HALL_EFFECT_EVENT)
        return;

    // apply the setting if all buttons are currently unpressed
    if (current_buttons_state == NO_BUTTON_PRESSED) {
        // if no buttons were pressed in the meantime, we don't have to do anything
        if (max_buttons_state == NO_BUTTON_PRESSED)
            return;

        // The first button press will only show if the sensor is enabled or not. The second button press will toggle it being enabled or not
        // e.g. 1 button press: 1 led flash indicating the sensor is enabled. 2nd button press: 2 led flashes indicating the sensor is now disabled
        if (max_buttons_state == prev_max_buttons_state) {
            sensor_enabled_state_array[max_buttons_state] = !sensor_enabled_state_array[max_buttons_state];
            sensor_manager_set_sensor_states(sensor_enabled_state_array);
            DPRINT(
                "setting the state of %d to %d \n", max_buttons_state, sensor_enabled_state_array[max_buttons_state]);
        }
        display_state(sensor_enabled_state_array[max_buttons_state]);
        prev_max_buttons_state = max_buttons_state;
        // reset the max of buttons pressed
        max_buttons_state = NO_BUTTON_PRESSED;
    } else if (current_buttons_state > max_buttons_state)
        // keep the maximum number of buttons pressed to see which combination of buttons is pressed
        max_buttons_state = current_buttons_state;
}

// if we are in interval configuration state, the buttons are used to configure a new interval for the humidity and light sensors
static void interval_configuration_input_event_handler(input_type_t i, bool mask)
{
    // new_sensor_interval is always initialized on 0 when entering this state

    if (current_buttons_state != NO_BUTTON_PRESSED || mask == true) {
        return;
    }

    switch (i) {
    case BUTTON1_EVENT:
        // button 1 will increase the interval by 30 seconds
        new_sensor_interval += 30;
        sensor_manager_set_interval(new_sensor_interval);
        led_flash(1);
        break;

    case BUTTON2_EVENT:
        // button 2 will increase the interval by 10 minutes
        new_sensor_interval += 600;
        sensor_manager_set_interval(new_sensor_interval);
        led_flash(2);
        break;

    case BUTTON3_EVENT:
        // button 3 will increase the interval by 2 hours
        new_sensor_interval += (2 * 60 * 60);
        sensor_manager_set_interval(new_sensor_interval);
        led_flash(3);
        break;
    }
}

// if we are in test state, the buttons are used to trigger a measurement. This is to test functionality easier
static void test_state_input_event_handler(input_type_t i, bool mask)
{
    if (current_buttons_state != NO_BUTTON_PRESSED || mask == true) {
        return;
    }
    // button1 triggers humidity, button2 triggers light and button3 triggers a voltage measurement
    sensor_manager_measure_sensor(i);
}

// if we are in light detection configuration state, the buttons are used to set the light detection thresholds and to enable/disable it
static void light_detection_configuration_state_event_handler(input_type_t i, bool mask)
{
    if (current_buttons_state != NO_BUTTON_PRESSED || mask == true) {
        return;
    }
    switch (i) {
    case BUTTON1_EVENT:;
        // button1 first press will show if light detection is enabled, the second press will toggle it
        if (prev_input_type == BUTTON1_EVENT)
            sensor_manager_set_light_detection_state(!sensor_manager_get_light_detection_state());
        display_state(sensor_manager_get_light_detection_state());
        break;
    case BUTTON2_EVENT:;
        // button2 will configure the high threshold of the light detection. If the light level goes above this level, it will send a message
        sensor_manager_set_light_threshold(true);
        break;
    case BUTTON3_EVENT:;
        // button3 will configure the low threshold of the light detection. If the light level goes below this level, it will send a message
        sensor_manager_set_light_threshold(false);
        break;
    default:
        break;
    }
    prev_input_type = i;
}

// this is the main input handler which will forward the input to the relevant state handler
static void app_state_input_event_handler(input_type_t i, bool mask)
{
    switch (current_app_state) {
    case OPERATIONAL_STATE:
        operational_input_event_handler(i, mask);
        break;
    case SENSOR_CONFIGURATION_STATE:
        sensor_configuration_input_event_handler(i, mask);
        break;
    case INTERVAL_CONFIGURATION_STATE:
        interval_configuration_input_event_handler(i, mask);
        break;
    case TEST_STATE:
        test_state_input_event_handler(i, mask);
        break;
    case LIGHT_DETECTION_CONFIGURATION_STATE:;
        light_detection_configuration_state_event_handler(i, mask);
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
    sensor_manager_init();

    // depending on the initial button state, we should go to a different global state
    booted_button_state = button_get_booted_state();
    switch (booted_button_state) {
    case BUTTON1_PRESSED:
        switch_state(SENSOR_CONFIGURATION_STATE);
        break;
    case BUTTON2_PRESSED:
        switch_state(INTERVAL_CONFIGURATION_STATE);
        break;
    case BUTTON3_PRESSED:
        switch_state(SLEEP_STATE);
        break;
    case BUTTON2_3_PRESSED:
        switch_state(TEST_STATE);
        break;
    case BUTTON1_3_PRESSED:
        switch_state(LIGHT_DETECTION_CONFIGURATION_STATE);
        break;
    default:
        switch_state(OPERATIONAL_STATE);
        break;
    }

    initial_button_press_released = (booted_button_state == NO_BUTTON_PRESSED);
    led_flash(current_app_state);

    log_print_string("Device booted %d\n", booted_button_state);
}
