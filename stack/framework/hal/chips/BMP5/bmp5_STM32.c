/**
 * Copyright (C) 2022 Bosch Sensortec GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "bmp5.h"
#include "log.h"
#include "errors.h"
#include "hwi2c.h"
#include "hwsystem.h"

/******************************************************************************/
/*!                         Macro definitions                                 */

/*! BMP5 shuttle id */
#define BMP5_SHUTTLE_ID_PRIM  UINT16_C(0x1B3)
#define BMP5_SHUTTLE_ID_SEC   UINT16_C(0x1D3)

/******************************************************************************/
/*!                Static variable definition                                 */

/*! Variable that holds the I2C device address or SPI chip selection */
static uint8_t dev_addr;
i2c_handle_t* bmp5_i2c;

/******************************************************************************/
/*!                User interface functions                                   */

/*!
 * I2C read function map to COINES platform
 */
BMP5_INTF_RET_TYPE bmp5_i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t length, void *intf_ptr)
{
    // uint8_t device_addr = *(uint8_t*)intf_ptr;

    // (void)intf_ptr;

    return !i2c_read_memory(bmp5_i2c, dev_addr, reg_addr, 8, (uint8_t*) reg_data, length);
}

/*!
 * I2C write function map to COINES platform
 */
BMP5_INTF_RET_TYPE bmp5_i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t length, void *intf_ptr)
{
    // uint8_t device_addr = *(uint8_t*)intf_ptr;

    // (void)intf_ptr;

    return !i2c_write_memory(bmp5_i2c, dev_addr, reg_addr, 8, (uint8_t*) reg_data, length);
}

/*!
 * Delay function map to COINES platform
 */
void bmp5_delay_us(uint32_t period, void *intf_ptr)
{
    const uint32_t max_period = 10000; // Maximum period per hw_busy_wait call
    
    // Calculate the number of full max_period delays required
    uint32_t full_delays = period / max_period;
    
    // Calculate the remaining period after the full delays
    uint32_t remaining_period = period % max_period;
    
    // Perform full max_period delays
    for(uint32_t i = 0; i < full_delays; i++)
    {
        hw_busy_wait(max_period);
    }
    
    // Perform the remaining delay, if any
    if(remaining_period > 0)
    {
        hw_busy_wait(remaining_period);
    }
}

/*!
 *  @brief Prints the execution status of the APIs.
 */
void bmp5_error_codes_print_result(const char api_name[], int8_t rslt)
{
    if (rslt != BMP5_OK)
    {
        log_print_error_string("%s\t", api_name);
        if (rslt == BMP5_E_NULL_PTR)
        {
            log_print_error_string("Error [%d] : Null pointer\r\n", rslt);
        }
        else if (rslt == BMP5_E_COM_FAIL)
        {
            log_print_error_string("Error [%d] : Communication failure\r\n", rslt);
        }
        else if (rslt == BMP5_E_DEV_NOT_FOUND)
        {
            log_print_error_string("Error [%d] : Device not found\r\n", rslt);
        }
        else if (rslt == BMP5_E_INVALID_CHIP_ID)
        {
            log_print_error_string("Error [%d] : Invalid chip id\r\n", rslt);
        }
        else if (rslt == BMP5_E_POWER_UP)
        {
            log_print_error_string("Error [%d] : Power up error\r\n", rslt);
        }
        else if (rslt == BMP5_E_POR_SOFTRESET)
        {
            log_print_error_string("Error [%d] : Power-on reset/softreset failure\r\n", rslt);
        }
        else if (rslt == BMP5_E_INVALID_POWERMODE)
        {
            log_print_error_string("Error [%d] : Invalid powermode\r\n", rslt);
        }
        else
        {
            /* For more error codes refer "*_defs.h" */
            log_print_error_string("Error [%d] : Unknown error code\r\n", rslt);
        }
    }
}

/*!
 *  @brief Function to select the interface between SPI and I2C.
 */
int8_t bmp5_interface_init(struct bmp5_dev *bmp5_dev, i2c_handle_t* handle)
{
    int8_t rslt = BMP5_OK;
    int16_t result;
    bmp5_i2c = handle;

    if (bmp5_dev != NULL)
    {

        dev_addr = BMP5_I2C_ADDR_PRIM;
        bmp5_dev->read = bmp5_i2c_read;
        bmp5_dev->write = bmp5_i2c_write;
        bmp5_dev->intf = BMP5_I2C_INTF;

        /* Holds the I2C device addr or SPI chip selection */
        bmp5_dev->intf_ptr = &dev_addr;

        /* Configure delay in microseconds */
        bmp5_dev->delay_us = bmp5_delay_us;
    }
    else
    {
        rslt = BMP5_E_NULL_PTR;
    }

    return rslt;
}

void bmp5_coines_deinit(void)
{
}
