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
#include "humidity_file.h"
#include "d7ap_fs.h"
#include "errors.h"
#include "little_queue.h"
#include "log.h"
#include "math.h"
#include "platform.h"
#include "stdint.h"
#include "timer.h"
// #include "bme280_STM32.h"
#include "bmp5.h"
#include "bmp5_STM32.h"
#include "bmp5_defs.h"

#ifdef true
#define DPRINT(...) log_print_string(__VA_ARGS__)
#else
#define DPRINT(...)
#endif

#define HUMIDITY_FILE_ID 53
#define HUMIDITY_FILE_SIZE sizeof(humidity_file_t)
#define RAW_HUMIDITY_FILE_SIZE 12

#define HUMIDITY_CONFIG_FILE_ID 63
#define HUMIDITY_CONFIG_FILE_SIZE sizeof(humidity_config_file_t)
#define RAW_HUMIDITY_CONFIG_FILE_SIZE 5

#define TESTMODE_HUMID_INTERVAL_SEC 60
#define DEFAULT_HUMIDITY_INTERVAL_SEC 20

typedef struct {
    union {
        uint8_t bytes[RAW_HUMIDITY_FILE_SIZE];
        struct {
            int32_t humidity;
            int32_t temperature;
            int32_t pressure;
        } __attribute__((__packed__));
    };
} humidity_file_t;

typedef struct {
    union {
        uint8_t bytes[RAW_HUMIDITY_CONFIG_FILE_SIZE];
        struct {
            uint32_t interval;
            bool enabled;
        } __attribute__((__packed__));
    };
} humidity_config_file_t;

static void file_modified_callback(uint8_t file_id);

static humidity_config_file_t humidity_config_file_cached
    = (humidity_config_file_t) { .interval = DEFAULT_HUMIDITY_INTERVAL_SEC, .enabled = true };

static bool humidity_file_transmit_state = false;
static bool humidity_config_file_transmit_state = false;
static bool test_mode_state = false;
static struct bmp5_dev bmp;
static struct bmp5_osr_odr_press_config osr_odr_press_cfg_u = { 0 };

static int8_t set_config(struct bmp5_osr_odr_press_config *osr_odr_press_cfg, struct bmp5_dev *dev)
{
    int8_t rslt;
    struct bmp5_iir_config set_iir_cfg;
    struct bmp5_int_source_select int_source_select;

    rslt = bmp5_set_power_mode(BMP5_POWERMODE_STANDBY, dev);
    bmp5_error_codes_print_result("bmp5_set_power_mode1", rslt);

    if (rslt == BMP5_OK)
    {
        /* Enable pressure */
        osr_odr_press_cfg->press_en = BMP5_ENABLE;

        rslt = bmp5_set_osr_odr_press_config(osr_odr_press_cfg, dev);
        bmp5_error_codes_print_result("bmp5_set_osr_odr_press_config", rslt);

        if (rslt == BMP5_OK)
        {
            set_iir_cfg.iir_flush_forced_en = BMP5_ENABLE;

            rslt = bmp5_set_iir_config(&set_iir_cfg, dev);
            bmp5_error_codes_print_result("bmp5_set_iir_config1", rslt);

            if (rslt == BMP5_OK)
            {
                set_iir_cfg.set_iir_t = BMP5_IIR_FILTER_COEFF_1;
                set_iir_cfg.set_iir_p = BMP5_IIR_FILTER_COEFF_1;
                set_iir_cfg.shdw_set_iir_t = BMP5_ENABLE;
                set_iir_cfg.shdw_set_iir_p = BMP5_ENABLE;

                rslt = bmp5_set_iir_config(&set_iir_cfg, dev);
                bmp5_error_codes_print_result("bmp5_set_iir_config2", rslt);
            }
        }

        if (rslt == BMP5_OK)
        {
            rslt = bmp5_configure_interrupt(BMP5_PULSED, BMP5_ACTIVE_HIGH, BMP5_INTR_PUSH_PULL, BMP5_INTR_ENABLE, dev);

            bmp5_error_codes_print_result("bmp5_configure_interrupt", rslt);

            if (rslt == BMP5_OK)
            {
                /* Note : Select INT_SOURCE after configuring interrupt */
                int_source_select.drdy_en = BMP5_ENABLE;
                rslt = bmp5_int_source_select(&int_source_select, dev);
                bmp5_error_codes_print_result("bmp5_int_source_select", rslt);
            }
        }
    }

    return rslt;
}

error_t humidity_files_initialize()
{
    d7ap_fs_file_header_t volatile_file_header
        = { .file_permissions = (file_permission_t) { .guest_read = true, .user_read = true },
              .file_properties.storage_class = FS_STORAGE_VOLATILE,
              .length = HUMIDITY_FILE_SIZE,
              .allocated_length = HUMIDITY_FILE_SIZE };

    d7ap_fs_file_header_t permanent_file_header = { .file_permissions
        = (file_permission_t) { .guest_read = true, .guest_write = true, .user_read = true, .user_write = true },
        .file_properties.storage_class = FS_STORAGE_PERMANENT,
        .length = HUMIDITY_CONFIG_FILE_SIZE,
        .allocated_length = HUMIDITY_CONFIG_FILE_SIZE + 10 };

    uint32_t length = HUMIDITY_CONFIG_FILE_SIZE;
    error_t ret = d7ap_fs_read_file(HUMIDITY_CONFIG_FILE_ID, 0, humidity_config_file_cached.bytes, &length, ROOT_AUTH);
    if (ret == -ENOENT) {
        ret = d7ap_fs_init_file(HUMIDITY_CONFIG_FILE_ID, &permanent_file_header, humidity_config_file_cached.bytes);
        if (ret != SUCCESS) {
            log_print_error_string("Error initializing humidity effect configuration file: %d", ret);
            return ret;
        }
    } else if (ret != SUCCESS)
        log_print_error_string("Error reading humidity effect configuration file: %d", ret);

    humidity_file_t humidity_file = {
        0,
    };

    ret = d7ap_fs_init_file(HUMIDITY_FILE_ID, &volatile_file_header, humidity_file.bytes);
    if (ret != SUCCESS) {
        log_print_error_string("Error initializing humidity effect file: %d", ret);
    }
    // initialize sensor
    // bme280_stm32_init(platf_get_i2c_handle());

    int8_t rslt;
    rslt = bmp5_interface_init(&bmp, platf_get_i2c_handle());
    bmp5_error_codes_print_result("bmp5_interface_init", rslt);
    rslt = bmp5_init(&bmp);
    bmp5_error_codes_print_result("bmp5_init", rslt);
    rslt = set_config(&osr_odr_press_cfg_u, &bmp);
    bmp5_error_codes_print_result("set_config", rslt);
    rslt = bmp5_set_power_mode(BMP5_POWERMODE_STANDBY, &bmp);
    bmp5_error_codes_print_result("bmp5_set_power_mode1", rslt);



    // register callbacks for when the files get modified internally or over the air
    d7ap_fs_register_file_modified_callback(HUMIDITY_CONFIG_FILE_ID, &file_modified_callback);
    d7ap_fs_register_file_modified_callback(HUMIDITY_FILE_ID, &file_modified_callback);
    sched_register_task(&humidity_file_execute_measurement);
}

static void file_modified_callback(uint8_t file_id)
{
    if (file_id == HUMIDITY_CONFIG_FILE_ID) {
        // humidity config file got adapted, apply settings
        uint32_t size = HUMIDITY_CONFIG_FILE_SIZE;
        d7ap_fs_read_file(HUMIDITY_CONFIG_FILE_ID, 0, humidity_config_file_cached.bytes, &size, ROOT_AUTH);
        if (humidity_config_file_cached.enabled && humidity_file_transmit_state) {
            timer_post_task_delay(&humidity_file_execute_measurement, humidity_config_file_cached.interval * TIMER_TICKS_PER_SEC);
        } else
            timer_cancel_task(&humidity_file_execute_measurement);

        if (humidity_config_file_transmit_state)
            queue_add_file(humidity_config_file_cached.bytes, HUMIDITY_CONFIG_FILE_SIZE, HUMIDITY_CONFIG_FILE_ID);
    } else if (file_id == HUMIDITY_FILE_ID) {
        // humidity file got modified, transmit file
        humidity_file_t humidity_file;
        uint32_t size = HUMIDITY_FILE_SIZE;
        d7ap_fs_read_file(HUMIDITY_FILE_ID, 0, humidity_file.bytes, &size, ROOT_AUTH);
        queue_add_file(humidity_file.bytes, HUMIDITY_FILE_SIZE, HUMIDITY_FILE_ID);
        timer_post_task_delay(&humidity_file_execute_measurement, humidity_config_file_cached.interval * TIMER_TICKS_PER_SEC);
    }
}

void humidity_file_transmit_config_file()
{
    uint32_t size = HUMIDITY_CONFIG_FILE_SIZE;
    d7ap_fs_read_file(HUMIDITY_CONFIG_FILE_ID, 0, humidity_config_file_cached.bytes, &size, ROOT_AUTH);
    queue_add_file(humidity_config_file_cached.bytes, HUMIDITY_CONFIG_FILE_SIZE, HUMIDITY_CONFIG_FILE_ID);
}
void humidity_file_execute_measurement()
{
    float parsed_temperature = 0;
    float parsed_humidity = 0;
    float parsed_pressure = 0;
    int8_t rslt;
    uint8_t int_status;

    rslt = bmp5_set_power_mode(BMP5_POWERMODE_FORCED, &bmp);

    rslt = bmp5_get_interrupt_status(&int_status, &bmp);
    bmp5_error_codes_print_result("bmp5_get_interrupt_status", rslt);
    uint8_t counter = 0;
    while (!(int_status & BMP5_INT_ASSERTED_DRDY))
    {
        bmp5_get_interrupt_status(&int_status, &bmp);
        counter++;
        if (counter > 50)
        {
            break;
        }
    }

    // bme280_stm32_get_sensor_values(&parsed_temperature, &parsed_humidity, &parsed_pressure);

    struct bmp5_sensor_data sensor_data;
    rslt = bmp5_get_sensor_data(&sensor_data, &osr_odr_press_cfg_u, &bmp);
    bmp5_error_codes_print_result("bmp5_get_sensor_data", rslt);

    log_print_string("(%lu /100) pa, (%ld /100) C, counter: %d\n", (long int)sensor_data.pressure,
                     (long int)sensor_data.temperature, counter);

    humidity_file_t humidity_file = { .humidity = counter,
        .temperature = (int32_t)round(sensor_data.temperature / 10), .pressure = (int32_t)round(sensor_data.pressure / 1000) };
    d7ap_fs_write_file(HUMIDITY_FILE_ID, 0, humidity_file.bytes, HUMIDITY_FILE_SIZE, ROOT_AUTH);
    rslt = bmp5_set_power_mode(BMP5_POWERMODE_STANDBY, &bmp);
    bmp5_error_codes_print_result("bmp5_set_power_mode1", rslt);
}


void humidity_file_set_measure_state(bool enable)
{
    timer_cancel_task(&humidity_file_execute_measurement);
    humidity_file_transmit_state = enable;
    humidity_config_file_transmit_state = enable;

    if (humidity_config_file_cached.enabled && humidity_file_transmit_state)
        timer_post_task_delay(&humidity_file_execute_measurement, humidity_config_file_cached.interval * TIMER_TICKS_PER_SEC);
}

void humidity_file_set_test_mode(bool enable)
{
    if (test_mode_state == enable)
        return;
    test_mode_state == enable;
    timer_cancel_task(&humidity_file_execute_measurement);
    if (enable) {
        humidity_config_file_cached.interval = TESTMODE_HUMID_INTERVAL_SEC;
        humidity_config_file_cached.enabled = true;
        timer_post_task_delay(&humidity_file_execute_measurement, humidity_config_file_cached.interval * TIMER_TICKS_PER_SEC);
    } else {
        uint32_t size = HUMIDITY_CONFIG_FILE_SIZE;
        d7ap_fs_read_file(HUMIDITY_CONFIG_FILE_ID, 0, humidity_config_file_cached.bytes, &size, ROOT_AUTH);
        if (humidity_config_file_cached.enabled && humidity_config_file_transmit_state)
            timer_post_task_delay(&humidity_file_execute_measurement, humidity_config_file_cached.interval * TIMER_TICKS_PER_SEC);
    }
}

bool humidity_file_is_enabled() { return humidity_config_file_cached.enabled; }

void humidity_file_set_enabled(bool enable)
{
    if (humidity_config_file_cached.enabled != enable) {
        humidity_config_file_cached.enabled = enable;
        d7ap_fs_write_file(
            HUMIDITY_CONFIG_FILE_ID, 0, humidity_config_file_cached.bytes, HUMIDITY_CONFIG_FILE_SIZE, ROOT_AUTH);
    }
}

void humidity_file_set_interval(uint32_t interval)
{
    if (humidity_config_file_cached.interval != interval) {
        humidity_config_file_cached.interval = interval;
        d7ap_fs_write_file(
            HUMIDITY_CONFIG_FILE_ID, 0, humidity_config_file_cached.bytes, HUMIDITY_CONFIG_FILE_SIZE, ROOT_AUTH);
    }
}