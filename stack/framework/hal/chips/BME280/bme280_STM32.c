/**\
 * Copyright (c) 2020 Bosch Sensortec GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 **/

/**
 * \ingroup bme280
 * \defgroup bme280Examples Examples
 * @brief Reference Examples
 */

/*!
 * @ingroup bme280Examples
 * @defgroup bme280GroupExampleLU linux_userspace
 * @brief Linux userspace test code, simple and mose code directly from the doco.
 * compile like this: gcc linux_userspace.c ../bme280.c -I ../ -o bme280
 * tested: Raspberry Pi.
 * Use like: ./bme280 /dev/i2c-0
 * \include linux_userspace.c
 */

#include "log.h"
#include "errors.h"
#include "hwi2c.h"
#include "hwsystem.h"
#ifdef true
    #define DPRINT(...)      log_print_string(__VA_ARGS__)
    #define DPRINT_DATA(...) log_print_data(__VA_ARGS__)
#else
    #define DPRINT(...)
    #define DPRINT_DATA(...)
#endif

/******************************************************************************/
/*!                         System header files                               */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>


/******************************************************************************/
/*!                         Own header files                                  */
#include "bme280.h"

/******************************************************************************/
/*!                               Structures                                  */


/* Structure that contains identifier details used in example */
struct identifier
{
    /* Variable to hold device address */
    uint8_t dev_addr;

    /* Variable that contains file descriptor */
    int8_t fd;

    i2c_handle_t* bmp280_i2c;
};

static struct bme280_dev dev;
static struct identifier id;

/****************************************************************************/
/*!                         Functions                                       */

/*!
 *  @brief Function that creates a mandatory delay required in some of the APIs.
 *
 * @param[in] period              : Delay in microseconds.
 * @param[in, out] intf_ptr       : Void pointer that can enable the linking of descriptors
 *                                  for interface related call backs
 *  @return void.
 *
 */
void user_delay_us(uint32_t period, void *intf_ptr);

/*!
 * @brief Function for print the temperature, humidity and pressure data.
 *
 * @param[out] comp_data    :   Structure instance of bme280_data
 *
 * @note Sensor data whose can be read
 *
 * sens_list
 * --------------
 * Pressure
 * Temperature
 * Humidity
 *
 */
void print_sensor_data(struct bme280_data *comp_data);

/*!
 *  @brief Function for reading the sensor's registers through I2C bus.
 *
 *  @param[in] reg_addr       : Register address.
 *  @param[out] data          : Pointer to the data buffer to store the read data.
 *  @param[in] len            : No of bytes to read.
 *  @param[in, out] intf_ptr  : Void pointer that can enable the linking of descriptors
 *                                  for interface related call backs.
 *
 *  @return Status of execution
 *
 *  @retval 0 -> Success
 *  @retval > 0 -> Failure Info
 *
 */
int8_t user_i2c_read(uint8_t reg_addr, uint8_t *data, uint32_t len, void *intf_ptr);

/*!
 *  @brief Function for writing the sensor's registers through I2C bus.
 *
 *  @param[in] reg_addr       : Register address.
 *  @param[in] data           : Pointer to the data buffer whose value is to be written.
 *  @param[in] len            : No of bytes to write.
 *  @param[in, out] intf_ptr  : Void pointer that can enable the linking of descriptors
 *                                  for interface related call backs
 *
 *  @return Status of execution
 *
 *  @retval BME280_OK -> Success
 *  @retval BME280_E_COMM_FAIL -> Communication failure.
 *
 */
int8_t user_i2c_write(uint8_t reg_addr, const uint8_t *data, uint32_t len, void *intf_ptr);

/*!
 * @brief Function reads temperature, humidity and pressure data in forced mode.
 *
 * @param[in] dev_remote   :   Structure instance of bme280_dev.
 *
 * @return Result of API execution status
 *
 * @retval BME280_OK - Success.
 * @retval BME280_E_NULL_PTR - Error: Null pointer error
 * @retval BME280_E_COMM_FAIL - Error: Communication fail error
 * @retval BME280_E_NVM_COPY_FAILED - Error: NVM copy failed
 *
 */
int8_t stream_sensor_data_forced_mode(struct bme280_dev *dev_remote);

/*!
 * @brief This function starts execution of the program.
 */
error_t bme280_stm32_init(i2c_handle_t* handle)
{


    /* Variable to define the result */
    int8_t rslt = BME280_OK;

    /* Make sure to select BME280_I2C_ADDR_PRIM or BME280_I2C_ADDR_SEC as needed */
    id.dev_addr = BME280_I2C_ADDR_PRIM;
    id.bmp280_i2c = handle;

    dev.intf = BME280_I2C_INTF;
    dev.read = user_i2c_read;
    dev.write = user_i2c_write;
    dev.delay_us = user_delay_us;

    /* Update interface pointer with the structure that contains both device address and file descriptor */
    dev.intf_ptr = &id;

    /* Initialize the bme280 */
    rslt = bme280_init(&dev);
    if (rslt != BME280_OK)
    {
        log_print_error_string("Failed to initialize the device: %d", rslt );
        return FAIL;
    }

    dev.settings.osr_h = BME280_OVERSAMPLING_1X;
    dev.settings.osr_p = BME280_OVERSAMPLING_1X;
    dev.settings.osr_t = BME280_OVERSAMPLING_1X;
    dev.settings.filter = BME280_FILTER_COEFF_OFF;

    uint8_t settings_sel = 0;
    settings_sel = BME280_OSR_PRESS_SEL | BME280_OSR_TEMP_SEL | BME280_OSR_HUM_SEL | BME280_FILTER_SEL;

    /* Set the sensor settings */
    rslt = bme280_set_sensor_settings(settings_sel, &dev);
    if (rslt != BME280_OK)
    {
        log_print_error_string("Failed to set sensor settings (code %+d).", rslt);

        return rslt;
    }

    
    /* Set the sensor to forced mode */
    rslt = bme280_set_sensor_mode(BME280_NORMAL_MODE, &dev);
    if (rslt != BME280_OK)
    {
        log_print_error_string("Failed to set sensor mode (code %+d).", rslt);
        return rslt;
    }

    return SUCCESS;
}

/*!
 * @brief This function reading the sensor's registers through I2C bus.
 */
int8_t user_i2c_read(uint8_t reg_addr, uint8_t *data, uint32_t len, void *intf_ptr)
{
    // struct identifier id;

    // id = *((struct identifier *)intf_ptr);


    if (!i2c_read_memory(id.bmp280_i2c, id.dev_addr, reg_addr, 8, (uint8_t*) data, len))
    {
        return BME280_E_COMM_FAIL;
    }

    return SUCCESS;
}

/*!
 * @brief This function provides the delay for required time (Microseconds) as per the input provided in some of the
 * APIs
 */
void user_delay_us(uint32_t period, void *intf_ptr)
{
    hw_busy_wait(period);
}

/*!
 * @brief This function for writing the sensor's registers through I2C bus.
 */
int8_t user_i2c_write(uint8_t reg_addr, const uint8_t *data, uint32_t len, void *intf_ptr)
{
    // struct identifier id;
    // id = *((struct identifier *)intf_ptr);

    if (!i2c_write_memory(id.bmp280_i2c, id.dev_addr, reg_addr, 8, (uint8_t*) data, len))
    {
        return BME280_E_COMM_FAIL;
    }


    return BME280_OK;
}

/*!
 * @brief This API used to print the sensor temperature, pressure and humidity data.
 */
void print_sensor_data(struct bme280_data *comp_data)
{
    uint16_t temp, press, hum;
    temp = (uint16_t)comp_data->temperature;
    press = (uint16_t)(0.01 * comp_data->pressure);
    hum = (uint16_t)comp_data->humidity;

    DPRINT("%d deg C, %d hPa, %d \n", temp, press, hum);
}

/*!
 * @brief This API reads the sensor temperature, pressure and humidity data in forced mode.
 */
int8_t stream_sensor_data_forced_mode(struct bme280_dev *dev_remote)
{
    /* Variable to define the result */
    int8_t rslt = BME280_OK;

    /* Variable to define the selecting sensors */
    uint8_t settings_sel = 0;

    /* Variable to store minimum wait time between consecutive measurement in force mode */
    uint32_t req_delay;

    /* Structure to get the pressure, temperature and humidity values */
    struct bme280_data comp_data;

    /* Recommended mode of operation: Indoor navigation */
    dev_remote->settings.osr_h = BME280_OVERSAMPLING_1X;
    dev_remote->settings.osr_p = BME280_OVERSAMPLING_16X;
    dev_remote->settings.osr_t = BME280_OVERSAMPLING_2X;
    dev_remote->settings.filter = BME280_FILTER_COEFF_16;

    settings_sel = BME280_OSR_PRESS_SEL | BME280_OSR_TEMP_SEL | BME280_OSR_HUM_SEL | BME280_FILTER_SEL;

    /* Set the sensor settings */
    rslt = bme280_set_sensor_settings(settings_sel, dev_remote);
    if (rslt != BME280_OK)
    {
        log_print_error_string("Failed to set sensor settings (code %+d).", rslt);

        return rslt;
    }

    DPRINT("Temperature, Pressure, Humidity\n");

    /*Calculate the minimum delay required between consecutive measurement based upon the sensor enabled
     *  and the oversampling configuration. */
    req_delay = bme280_cal_meas_delay(&dev_remote->settings);

    /* Continuously stream sensor data */
     while (1)
     {
        /* Set the sensor to forced mode */
        rslt = bme280_set_sensor_mode(BME280_FORCED_MODE, dev_remote);
        if (rslt != BME280_OK)
        {
            log_print_error_string("Failed to set sensor mode (code %+d).", rslt);
            return rslt;
        }

        /* Wait for the measurement to complete and print data */
        dev_remote->delay_us(req_delay, dev_remote->intf_ptr);
        rslt = bme280_get_sensor_data(BME280_ALL, &comp_data, dev_remote);
        if (rslt != BME280_OK)
        {
            log_print_error_string("Failed to get sensor data (code %+d).", rslt);
            return rslt;
        }

        print_sensor_data(&comp_data);
        hw_busy_wait(10000);
        hw_busy_wait(10000);
        hw_busy_wait(10000);
     }

    return rslt;
}

/*!
 * @brief This API reads the sensor temperature, pressure and humidity data in forced mode.
 */
int8_t bme280_stm32_get_sensor_values(float* parsed_temperature, float* parsed_humidity, float* parsed_pressure)
{
    /* Variable to define the result */
    int8_t rslt = BME280_OK;

    /* Variable to define the selecting sensors */
    

    /* Variable to store minimum wait time between consecutive measurement in force mode */
    uint32_t req_delay;

    /* Structure to get the pressure, temperature and humidity values */
    struct bme280_data comp_data;

    /* Recommended mode of operation: Indoor navigation */
   

    DPRINT("Temperature, Pressure, Humidity\n");


    rslt = bme280_get_sensor_data(BME280_ALL, &comp_data, &dev);
    if (rslt != BME280_OK)
    {
        log_print_error_string("Failed to get sensor data (code %+d).", rslt);
        return rslt;
    }
    *parsed_temperature = (float)comp_data.temperature;
    *parsed_humidity = (float)comp_data.humidity;
    *parsed_pressure = 0.01 * (float)comp_data.pressure;
    print_sensor_data(&comp_data);
    // bme280_set_sensor_mode(BME280_SLEEP_MODE, &dev);

    return rslt;
}