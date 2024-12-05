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
#include "led.h"
#include "little_queue.h"


#include "hwgpio.h"
#include "math.h"
#include "platform.h"
#include "scheduler.h"
#include "sensor_manager.h"
#include "stm32_common_gpio.h"

#include "little_queue.h"
#include "device_state_file.h"
#include "humidity_file.h"

#include "bmp5.h"
#include "bmp5_STM32.h"
#include "bmp5_defs.h"
#include "ir_file.h"


#include "accelerometer_file.h"

#ifdef FRAMEWORK_SENSOR_MANAGER_LOG
#define DPRINT(...) log_print_string(__VA_ARGS__)
#define DPRINT_DATA(...) log_print_data(__VA_ARGS__)
#else
#define DPRINT(...)
#define DPRINT_DATA(...)
#endif

static bool current_transmit_state = false;
static bool current_testmode_state = false;
static struct bmp5_dev bmp;
static struct bmp5_osr_odr_press_config osr_odr_press_cfg_u = { 0 };

void tap_detected_check();
void enable_tap_detection();
static void get_sensor_data();
static int8_t set_config(struct bmp5_osr_odr_press_config *osr_odr_press_cfg, struct bmp5_dev *dev);

void sensor_manager_init()
{
    // sched_register_task(&tap_detected_check);
    // sched_register_task(&get_sensor_data);
    // global settings, versions and voltage files
    // humidity_files_initialize();
    device_state_files_initialize();
    accelerometer_files_initialize();
    ir_files_initialize();
    humidity_files_initialize();

    // sths34pf80_tmos_presence_detection(platf_get_i2c_handle());
    // bma400_interface_init(&bma, platf_get_i2c_handle());
    // int8_t rslt;
    // rslt = bma400_init(&bma);
    // bma400_check_rslt("bma400_init", rslt);

    // rslt = bma400_soft_reset(&bma);
    // bma400_check_rslt("bma400_soft_reset", rslt);

    // rslt = bmp5_interface_init(&bmp, platf_get_i2c_handle());
    // bmp5_error_codes_print_result("bmp5_interface_init", rslt);

    // rslt = bmp5_init(&bmp);
    // bmp5_error_codes_print_result("bmp5_init", rslt);

    // rslt = set_config(&osr_odr_press_cfg_u, &bmp);
    // bmp5_error_codes_print_result("set_config", rslt);

    // get_sensor_data();



  

}

void sensor_manager_set_transmit_state(bool state)
{
    if (state == current_transmit_state)
        return;

    // enable or disable transmission of all sensor files
    humidity_file_set_measure_state(state);
    device_state_file_set_measure_state(state);
    // accelerometer_file_set_measure_state(state);
    // ir_file_set_measure_state(state);
    current_transmit_state = state;
}



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

static void get_sensor_data()
{
    int8_t rslt;
    uint8_t idx = 0;
    uint8_t int_status;
    struct bmp5_sensor_data sensor_data;
    long int pressure_avg;
    long int temperature_avg;

    rslt = bmp5_set_power_mode(BMP5_POWERMODE_FORCED, &bmp);
    bmp5_error_codes_print_result("bmp5_set_power_mode1", rslt);

    log_print_string("\nOutput :\n\n");
    log_print_string("Data, Pressure (Pa), Temperature (deg C)\n");

    while (idx < 50)
    {
        rslt = bmp5_get_interrupt_status(&int_status, &bmp);
        bmp5_error_codes_print_result("bmp5_get_interrupt_status", rslt);

        if (int_status & BMP5_INT_ASSERTED_DRDY)
        {
            rslt = bmp5_get_sensor_data(&sensor_data, &osr_odr_press_cfg_u, &bmp);
            bmp5_error_codes_print_result("bmp5_get_sensor_data", rslt);

            if (rslt == BMP5_OK)
            {
                // log_print_string("%d, (%lu /100) pa, (%ld /100) C\n", idx, (long unsigned int)sensor_data.pressure,
                //        (long int)sensor_data.temperature);
                 if (idx > 0) {
                pressure_avg += ((long int)sensor_data.pressure - pressure_avg) / (idx + 1);
                temperature_avg += ((long int)sensor_data.temperature - temperature_avg) / (idx + 1);
            } else {
                // For the first iteration, just set the initial values
                pressure_avg = sensor_data.pressure;
                temperature_avg = sensor_data.temperature;}

                idx++;

                rslt = bmp5_set_power_mode(BMP5_POWERMODE_FORCED, &bmp);
                bmp5_error_codes_print_result("bmp5_set_power_mode2", rslt);
            }
        }
    }
log_print_string("%d, (%lu /100) pa, (%ld /100) C\n", idx, (long int)pressure_avg,
                       (long int)temperature_avg);
    timer_post_task_delay(&get_sensor_data, 1000);
}