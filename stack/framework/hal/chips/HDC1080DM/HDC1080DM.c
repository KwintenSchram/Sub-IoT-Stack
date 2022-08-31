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
#include "HDC1080DM.h"

#include "errors.h"
#include "hwi2c.h"
#include "hwsystem.h"
#include "log.h"
#include "math.h"
#include <stdio.h>

#ifdef false
#define DPRINT(...) log_print_string(__VA_ARGS__)
#define DPRINT_DATA(...) log_print_data(__VA_ARGS__)
#else
#define DPRINT(...)
#define DPRINT_DATA(...)
#endif

#define HDC1080_I2C_ADDRESS 0x40
#define HDC1080_RESOLUTION_8BIT_WAIT_TIME 3 // 2.50 ms
#define HDC1080_RESOLUTION_11BIT_WAIT_TIME 4 // 3.85 ms
#define HDC1080_RESOLUTION_14BIT_WAIT_TIME 7 // 6.50 ms
static i2c_handle_t* i2c_dev;
static uint8_t temperature_readout_time = HDC1080_RESOLUTION_14BIT_WAIT_TIME;
static uint8_t humidity_readout_time = HDC1080_RESOLUTION_14BIT_WAIT_TIME;

typedef enum {
    HDC1080_TEMPERATURE = 0x00,
    HDC1080_HUMIDITY = 0x01,
    HDC1080_CONFIGURATION = 0x02,
    HDC1080_MANUFACTURER_ID = 0xFE,
    HDC1080_DEVICE_ID = 0xFF,
    HDC1080_SERIAL_ID_FIRST = 0xFB,
    HDC1080_SERIAL_ID_MID = 0xFC,
    HDC1080_SERIAL_ID_LAST = 0xFD,
} HDC1080_Pointers;

static error_t user_i2c_read(uint8_t reg_addr, uint16_t* data, uint8_t wait_time_ms);
static error_t user_i2c_write(uint8_t reg_addr, uint8_t* data, uint32_t len);
static error_t HDC1080DM_read_manufacturer_id(uint16_t* man_id);
static error_t HDC1080DM_read_device_id(uint16_t* device_id);

/*!
 * @brief This function reading the sensor's registers through I2C bus.
 */
static error_t user_i2c_read(uint8_t reg_addr, uint16_t* data, uint8_t wait_time_ms)
{
    uint8_t buffer_r[] = { reg_addr };
    uint8_t buffer[2];
    i2c_write(i2c_dev, HDC1080_I2C_ADDRESS, buffer_r, 1);
    for (uint8_t i = 0; i < wait_time_ms; i++) {
        hw_busy_wait(1000);
    }
    i2c_read(i2c_dev, HDC1080_I2C_ADDRESS, buffer, 2);

    *data = (buffer[0] << 8) | buffer[1];
    return SUCCESS;
}

/*!
 * @brief This function for writing the sensor's registers through I2C bus.
 */
static error_t user_i2c_write(uint8_t reg_addr, uint8_t* data, uint32_t len)
{

    if (!i2c_write_memory(i2c_dev, HDC1080_I2C_ADDRESS, reg_addr, 8, data, len)) {
        return FAIL;
    }
    return SUCCESS;
}

/*!
 * @brief Sets up the default parameters of the sensor and verifies the device id
 */
error_t HDC1080DM_init(i2c_handle_t* i2c_handle)
{
    error_t ret1, ret2;
    uint16_t device_id;
    i2c_dev = i2c_handle;
    ret1 = HDC1080DM_set_resolution(HDC1080_RESOLUTION_14BIT, HDC1080_RESOLUTION_14BIT);
    ret2 = HDC1080DM_read_device_id(&device_id);

    return (ret1 == SUCCESS && ret2 == SUCCESS) ? SUCCESS : FAIL;
}

/*!
 * @brief Reads the manufacturer id of the sensor and verifies the value. This should be 0x5449
 */
static error_t HDC1080DM_read_manufacturer_id(uint16_t* man_id)
{
    error_t ret;
    ret = user_i2c_read(HDC1080_MANUFACTURER_ID, man_id, 0);
    DPRINT("HDC1080DM manufacter id %d", *man_id);
    return (ret == SUCCESS && *man_id == 0x5449) ? SUCCESS : FAIL;
}

/*!
 * @brief Reads the device id of the sensor and verifies the value. This should be 0x1050
 */
static error_t HDC1080DM_read_device_id(uint16_t* device_id)
{
    error_t ret;
    ret = user_i2c_read(HDC1080_DEVICE_ID, device_id, 0);
    DPRINT("HDC1080DM device id %d", *device_id);
    return (ret == SUCCESS && *device_id == 0x1050) ? SUCCESS : FAIL;
}

/*!
 * @brief Writes the configuration register of the sensor.
 */
error_t HDC1080DM_write_config_register(HDC1080_config_register_t reg)
{
    return user_i2c_write(HDC1080_CONFIGURATION, &reg.rawData, 1);
}

/*!
 * @brief Sets the desires resolution of the humidity and temperature sensor. Also stores the related wait time
 * dependant on the set resolution
 */
error_t HDC1080DM_set_resolution(HDC1080_measurement_resolution humidity, HDC1080_measurement_resolution temperature)
{
    HDC1080_config_register_t reg;
    reg.rawData = 0;
    temperature_readout_time = HDC1080_RESOLUTION_14BIT_WAIT_TIME;
    humidity_readout_time = HDC1080_RESOLUTION_14BIT_WAIT_TIME;

    if (temperature == HDC1080_RESOLUTION_11BIT) {
        reg.TemperatureMeasurementResolution = 0x01;
        temperature_readout_time = HDC1080_RESOLUTION_11BIT_WAIT_TIME;
    }

    switch (humidity) {
    case HDC1080_RESOLUTION_8BIT:
        reg.HumidityMeasurementResolution = 0x02;
        humidity_readout_time = HDC1080_RESOLUTION_8BIT_WAIT_TIME;
        break;
    case HDC1080_RESOLUTION_11BIT:
        reg.HumidityMeasurementResolution = 0x01;
        humidity_readout_time = HDC1080_RESOLUTION_11BIT_WAIT_TIME;
        break;
    default:
        break;
    }

    return user_i2c_write(HDC1080_CONFIGURATION, &reg.rawData, 1);
}

/*!
 * @brief TODO. Uses the embedded heat element to heat up the sensor.
 */
error_t HDC1080DM_heat_up(uint8_t seconds) { return SUCCESS; }

/*!
 * @brief Reads the temperature value of the sensor.
 */
error_t HDC1080DM_read_temperature(float* parsed_temperature)
{
    error_t ret;
    uint16_t rawT;

    ret = user_i2c_read(HDC1080_TEMPERATURE, &rawT, temperature_readout_time);

    *parsed_temperature = (((rawT / pow(2, 16)) * 165.0) - 40.0);

    DPRINT("raw tempertature :%d, parsed militemperature: %d", rawT, (int32_t)round(*parsed_temperature * 1000));
    return ret;
}

/*!
 * @brief Reads the humidity value of the sensor.
 */
error_t HDC1080DM_read_humidity(float* parsed_humidity)
{
    error_t ret;
    uint16_t rawH;

    ret = user_i2c_read(HDC1080_HUMIDITY, &rawH, humidity_readout_time);

    *parsed_humidity = (((rawH / pow(2, 16)) * 165.0) - 40.0);

    DPRINT("raw humidity :%d, parsed milihumidity: %d", rawH, (int32_t)round(*parsed_humidity * 1000));
    return ret;
}
