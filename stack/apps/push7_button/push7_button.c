/*
 * Copyright (c) 2015-2021 University of Antwerp, Aloxy NV.
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

#include "adc_stuff.h"
#include "button.h"
#include "debug.h"
#include "file_definitions.h"
#include "led.h"
#include "little_queue.h"
#include "log.h"
#include "scheduler.h"
#include "sensor_manager.h"
#include "timer.h"

//#define FRAMEWORK_APP_LOG 1
#ifdef FRAMEWORK_APP_LOG
#include "log.h"
#define DPRINT(...) log_print_string(__VA_ARGS__)
#define DPRINT_DATA(...) log_print_data(__VA_ARGS__)
#else
#define DPRINT(...)
#define DPRINT_DATA(...)
#endif

#define STATE_COUNTER_EVENT_SEC TIMER_TICKS_PER_SEC * 1

typedef enum { BOOTED_STATE, OPERATIONAL_STATE, CONFIGURATION_STATE } APP_STATE_t;

typedef enum {
    BUTTON1_EVENT = 0,
    BUTTON2_EVENT = 1,
    BUTTON3_EVENT = 2,
    HALL_EFFECT_EVENT,
    STATE_COUNTER_EVENT,
} input_type_t;

static void app_state_input_event_handler(input_type_t i, bool mask);
static APP_STATE_t current_app_state = BOOTED_STATE;
static buttons_state_t current_buttons_state = NO_BUTTON_PRESSED;
static buttons_state_t previous_buttons_state = NO_BUTTON_PRESSED;
static uint8_t app_event_counter = 0;
static bool timer_active = false;
static uint8_t operational_event_timer_counter = 0;

static void state_counter_event()
{
    if (timer_active) {
        app_state_input_event_handler(STATE_COUNTER_EVENT, false);
        timer_post_task_delay(&state_counter_event, STATE_COUNTER_EVENT_SEC);
    }
}

static void app_state_start_timer()
{
    timer_active = true;
    operational_event_timer_counter = 0;
    timer_post_task_delay(&state_counter_event, STATE_COUNTER_EVENT_SEC);
}

static void app_state_stop_timer()
{
    timer_active = false;
    operational_event_timer_counter = 0;
    timer_cancel_task(&state_counter_event);
}

static void switch_state(APP_STATE_t new_state)
{
    current_app_state = new_state;
    sensor_manager_set_state(new_state == OPERATIONAL_STATE);
}

static void operational_input_event_handler(input_type_t i, bool mask)
{
    if (current_buttons_state == ALL_BUTTONS_PRESSED && previous_buttons_state != ALL_BUTTONS_PRESSED) {
        app_state_start_timer();
    } else if (previous_buttons_state == ALL_BUTTONS_PRESSED && current_buttons_state != ALL_BUTTONS_PRESSED) {
        app_state_stop_timer();
    }
    switch (i) {
    case STATE_COUNTER_EVENT:
        operational_event_timer_counter++;
        if (operational_event_timer_counter > 10) {
            switch_state(CONFIGURATION_STATE);
            app_state_stop_timer();
        }
    default:
        break;
    }
}

static void configuration_input_event_handler(input_type_t i, bool mask)
{
    //TODO add state timeout
    //TODO implement sensor config menu here
    switch (i) {
    case BUTTON1_EVENT:
        break;
    case BUTTON2_EVENT:
        break;
    case BUTTON3_EVENT:
        break;
    case HALL_EVENT:
        break;
    case STATE_COUNTER_EVENT:
        break;
    default:
        break;
    }
}

static void app_state_input_event_handler(input_type_t i, bool mask)
{
    switch (current_app_state) {
    case OPERATIONAL_STATE:
        operational_input_event_handler(i, mask); //wait for NO_BUTTON_PRESSED after state switch
        break;
    case CONFIGURATION_STATE:
        configuration_input_event_handler(i, mask); //wait for NO_BUTTON_PRESSED after state switch
        break;
    default:
        break;
    }
}

static void userbutton_callback(button_id_t button_id, uint8_t mask, buttons_state_t buttons_state)
{
    current_buttons_state = buttons_state;
    app_state_input_event_handler(buttons_state, mask);
    sensor_manager_button_pressed(button_id, mask, buttons_state);
}

void bootstrap()
{
    log_print_string("Device booted\n");
    little_queue_init();
    adc_stuff_init();
    led_flash_white();
    ubutton_register_callback(&userbutton_callback);
    sched_register_task(&state_counter_event);
}