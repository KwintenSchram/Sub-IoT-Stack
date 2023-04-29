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
#include "push7_state_file.h"
#include "adc_stuff.h"
#include "d7ap_fs.h"
#include "errors.h"
#include "little_queue.h"
#include "log.h"
#include "network_manager.h"
#include "stdint.h"
#include "timer.h"

#ifdef true
#define DPRINT(...) log_print_string(__VA_ARGS__)
#else
#define DPRINT(...)
#endif

#define PUSH7_STATE_FILE_ID 56
#define PUSH7_STATE_FILE_SIZE sizeof(push7_state_file_t)
#define RAW_PUSH7_STATE_FILE_SIZE 4

#define PUSH7_STATE_CONFIG_FILE_ID 66
#define PUSH7_STATE_CONFIG_FILE_SIZE sizeof(push7_state_config_file_t)
#define RAW_PUSH7_STATE_CONFIG_FILE_SIZE 7

#define TESTMODE_STATE_INTERVAL_SEC 30
#define HIGH_TX_POWER 17
#define LOW_TX_POWER 15

typedef struct {
    union {
        uint8_t bytes[RAW_PUSH7_STATE_FILE_SIZE];
        struct {
            uint16_t battery_voltage;
            uint8_t hw_version;
            uint8_t sw_version;
        } __attribute__((__packed__));
    };
} push7_state_file_t;

typedef struct {
    union {
        uint8_t bytes[RAW_PUSH7_STATE_CONFIG_FILE_SIZE];
        struct {
            uint32_t interval;
            bool led_flash_state;
            bool enabled;
            uint8_t tx_power;
        } __attribute__((__packed__));
    };
} push7_state_config_file_t;

static void file_modified_callback(uint8_t file_id);

static push7_state_config_file_t push7_state_config_file_cached
    = (push7_state_config_file_t) { .interval = 1, .led_flash_state = true, .enabled = true, .tx_power = LOW_TX_POWER };

static bool push7_state_file_transmit_state = false;
static bool push7_state_config_file_transmit_state = false;
static bool test_mode_state = false;

/**
 * @brief Initialize the push7 state file and push7 state config file
 * The push7 state file tells us about the status of the application, it includes the versions and voltage
 * the push7 state config file configures the global settings of the application like tx power and leds behaviour
 * @return error_t
 */
error_t push7_state_files_initialize()
{
    d7ap_fs_file_header_t volatile_file_header
        = { .file_permissions = (file_permission_t) { .guest_read = true, .user_read = true },
              .file_properties.storage_class = FS_STORAGE_VOLATILE,
              .length = PUSH7_STATE_FILE_SIZE,
              .allocated_length = PUSH7_STATE_FILE_SIZE };

    d7ap_fs_file_header_t permanent_file_header = { .file_permissions
        = (file_permission_t) { .guest_read = true, .guest_write = true, .user_read = true, .user_write = true },
        .file_properties.storage_class = FS_STORAGE_PERMANENT,
        .length = PUSH7_STATE_CONFIG_FILE_SIZE,
        .allocated_length = PUSH7_STATE_CONFIG_FILE_SIZE + 10 };

    uint32_t length = PUSH7_STATE_CONFIG_FILE_SIZE;
    error_t ret
        = d7ap_fs_read_file(PUSH7_STATE_CONFIG_FILE_ID, 0, push7_state_config_file_cached.bytes, &length, ROOT_AUTH);
    if (ret == -ENOENT) {
        ret = d7ap_fs_init_file(
            PUSH7_STATE_CONFIG_FILE_ID, &permanent_file_header, push7_state_config_file_cached.bytes);
        if (ret != SUCCESS) {
            log_print_error_string("Error initializing push7_state effect configuration file: %d", ret);
            return ret;
        }
    } else if (ret != SUCCESS)
        log_print_error_string("Error reading push7_state effect configuration file: %d", ret);

    push7_state_file_t push7_state_file = {
        0,
    };

    ret = d7ap_fs_init_file(PUSH7_STATE_FILE_ID, &volatile_file_header, push7_state_file.bytes);
    if (ret != SUCCESS) {
        log_print_error_string("Error initializing push7_state effect file: %d", ret);
    }
    // set the configurations of the configuration file and register a callback on all changes on those files
    adc_stuff_init();
    little_queue_set_led_state(push7_state_config_file_cached.led_flash_state);
    network_manager_set_tx_power(push7_state_config_file_cached.tx_power);
    d7ap_fs_register_file_modified_callback(PUSH7_STATE_CONFIG_FILE_ID, &file_modified_callback);
    d7ap_fs_register_file_modified_callback(PUSH7_STATE_FILE_ID, &file_modified_callback);
    sched_register_task(&push7_state_file_execute_measurement);
}

static void file_modified_callback(uint8_t file_id)
{
    if (file_id == PUSH7_STATE_CONFIG_FILE_ID) {
        // push7 state config file got modified
        uint32_t size = PUSH7_STATE_CONFIG_FILE_SIZE;
        d7ap_fs_read_file(PUSH7_STATE_CONFIG_FILE_ID, 0, push7_state_config_file_cached.bytes, &size, ROOT_AUTH);
        // set a timer to read the voltage periodically
        if (push7_state_config_file_cached.enabled && push7_state_file_transmit_state)
            timer_post_task_delay(
                &push7_state_file_execute_measurement, push7_state_config_file_cached.interval * TIMER_TICKS_PER_SEC);
        else
            timer_cancel_task(&push7_state_file_execute_measurement);
        little_queue_set_led_state(push7_state_config_file_cached.led_flash_state);
        network_manager_set_tx_power(push7_state_config_file_cached.tx_power);
        if (push7_state_config_file_transmit_state)
            queue_add_file(
                push7_state_config_file_cached.bytes, PUSH7_STATE_CONFIG_FILE_SIZE, PUSH7_STATE_CONFIG_FILE_ID);
    } else if (file_id == PUSH7_STATE_FILE_ID) {
        // push7 state file got modified, most likely internally
        push7_state_file_t push7_state_file;
        uint32_t size = PUSH7_STATE_FILE_SIZE;
        d7ap_fs_read_file(PUSH7_STATE_FILE_ID, 0, push7_state_file.bytes, &size, ROOT_AUTH);
        queue_add_file(push7_state_file.bytes, PUSH7_STATE_FILE_SIZE, PUSH7_STATE_FILE_ID);
        timer_post_task_delay(
            &push7_state_file_execute_measurement, push7_state_config_file_cached.interval * TIMER_TICKS_PER_SEC);
    }
}

void push7_state_file_transmit_config_file()
{
    uint32_t size = PUSH7_STATE_CONFIG_FILE_SIZE;
    d7ap_fs_read_file(PUSH7_STATE_CONFIG_FILE_ID, 0, push7_state_config_file_cached.bytes, &size, ROOT_AUTH);
    queue_add_file(push7_state_config_file_cached.bytes, PUSH7_STATE_CONFIG_FILE_SIZE, PUSH7_STATE_CONFIG_FILE_ID);
}

void push7_state_file_execute_measurement()
{
    update_battery_voltage();
    uint16_t voltage = get_battery_voltage();
    push7_state_file_t push7_state_file = { .hw_version = 0, .sw_version = 0, .battery_voltage = voltage };
    d7ap_fs_write_file(PUSH7_STATE_FILE_ID, 0, push7_state_file.bytes, PUSH7_STATE_FILE_SIZE, ROOT_AUTH);
}

void push7_state_file_set_measure_state(bool enable)
{
    // enable or disable the periodic voltage measurement
    timer_cancel_task(&push7_state_file_execute_measurement);
    push7_state_file_transmit_state = enable;
    push7_state_config_file_transmit_state = enable;
    if (push7_state_config_file_cached.enabled && push7_state_file_transmit_state)
        timer_post_task_delay(
            &push7_state_file_execute_measurement, push7_state_config_file_cached.interval * TIMER_TICKS_PER_SEC);
}

void push7_state_file_set_test_mode(bool enable)
{
    // in test mode, everything gets sent at 30 seconds interval instead of the current configuration
    if (test_mode_state == enable)
        return;
    test_mode_state == enable;
    timer_cancel_task(&push7_state_file_execute_measurement);
    if (enable) {
        push7_state_config_file_cached.interval = TESTMODE_STATE_INTERVAL_SEC;
        push7_state_config_file_cached.enabled = true;
        timer_post_task_delay(
            &push7_state_file_execute_measurement, push7_state_config_file_cached.interval * TIMER_TICKS_PER_SEC);
    } else {
        uint32_t size = PUSH7_STATE_CONFIG_FILE_SIZE;
        d7ap_fs_read_file(PUSH7_STATE_CONFIG_FILE_ID, 0, push7_state_config_file_cached.bytes, &size, ROOT_AUTH);
        if (push7_state_config_file_cached.enabled && push7_state_config_file_transmit_state)
            timer_post_task_delay(
                &push7_state_file_execute_measurement, push7_state_config_file_cached.interval * TIMER_TICKS_PER_SEC);
    }
}

bool push7_state_file_is_enabled() { return push7_state_config_file_cached.enabled; }

bool push7_flash_is_led_enabled() { return push7_state_config_file_cached.led_flash_state; }

void push7_flash_set_led_enabled(bool state)
{
    // enable or disable the led after transmission
    if (push7_state_config_file_cached.led_flash_state != state) {
        push7_state_config_file_cached.led_flash_state = state;
        d7ap_fs_write_file(PUSH7_STATE_CONFIG_FILE_ID, 0, push7_state_config_file_cached.bytes,
            PUSH7_STATE_CONFIG_FILE_SIZE, ROOT_AUTH);
    }
}

void push7_state_file_set_enabled(bool enable)
{
    // enable or disable sending and gathering the push7 state file
    if (push7_state_config_file_cached.enabled != enable) {
        push7_state_config_file_cached.enabled = enable;
        d7ap_fs_write_file(PUSH7_STATE_CONFIG_FILE_ID, 0, push7_state_config_file_cached.bytes,
            PUSH7_STATE_CONFIG_FILE_SIZE, ROOT_AUTH);
    }
}

void push7_state_file_set_interval(uint32_t interval)
{
    // change the interval on which the push7 state file gets gathered and sent
    if (push7_state_config_file_cached.interval != interval) {
        push7_state_config_file_cached.interval = interval;
        d7ap_fs_write_file(PUSH7_STATE_CONFIG_FILE_ID, 0, push7_state_config_file_cached.bytes,
            PUSH7_STATE_CONFIG_FILE_SIZE, ROOT_AUTH);
    }
}

void push7_state_file_set_high_tx_power_state(bool enable_high_tx_power)
{
    // change the tx power to the higher or lower preset
    if (enable_high_tx_power)
        push7_state_config_file_cached.tx_power = HIGH_TX_POWER;
    else
        push7_state_config_file_cached.tx_power = LOW_TX_POWER;
    d7ap_fs_write_file(
        PUSH7_STATE_CONFIG_FILE_ID, 0, push7_state_config_file_cached.bytes, PUSH7_STATE_CONFIG_FILE_SIZE, ROOT_AUTH);
}

bool push7_state_file_get_high_tx_power_state() { return (push7_state_config_file_cached.tx_power == HIGH_TX_POWER); }
