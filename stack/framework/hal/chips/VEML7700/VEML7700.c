#include "VEML7700.h"

#include "errors.h"
#include "hwi2c.h"
#include "hwsystem.h"
#include "log.h"
#include "math.h"
#include <stdio.h>

#ifdef true
    #define DPRINT(...)      log_print_string(__VA_ARGS__)
    #define DPRINT_DATA(...) log_print_data(__VA_ARGS__)
#else
    #define DPRINT(...)
    #define DPRINT_DATA(...)
#endif

#define VEML7700_I2C_ADDRESS 0x10

static i2c_handle_t* i2c_dev;

static VEML7700_CONFIG_REG current_config_reg = { .ALS_GAIN = ALS_GAIN_x1,
    .ALS_INT_EN = 0,
    .ALS_IT = ALS_INTEGRATION_100ms,
    .ALS_PERS = ALS_PERSISTENCE_1,
    .ALS_SD = 0 };

typedef enum {
    VEML7700_CONFIGURATION_REGISTER = 0x00,
    VEML7700_HIGH_THRESHOLD_WINDOWS_SETTING = 0x01,
    VEML7700_LOW_THRESHOLD_WINDOWS_SETTING = 0x02,
    VEML7700_POWER_SAVING_MODES = 0x03,
    VEML7700_ALS_HIGH_RESOLUTION_OUTPUT_DATA = 0x04,
    VEML7700_WHITE_CHANNEL_OUTPUT_DATA = 0x05,
    VEML7700_INTERRUPT_STATUS = 0x06,
} VEML7700_Pointers;

void convert_data_to_lux(uint16_t raw_counts, float* lux);

/*!
 * @brief This function reading the sensor's registers through I2C bus.
 */
static error_t user_i2c_read(uint8_t reg_addr, uint16_t* data)
{
    uint8_t buffer[2];
    if (!i2c_read_memory(i2c_dev, VEML7700_I2C_ADDRESS, reg_addr, 8, buffer, 2)) {
        return FAIL;
    }

    *data = (buffer[0] << 8) | buffer[1];

    return SUCCESS;
}

/*!
 * @brief This function for writing the sensor's registers through I2C bus.
 */
static error_t user_i2c_write(uint8_t reg_addr, uint16_t data)
{
    uint8_t buffer[2];
    buffer[0] = (uint8_t)data & 0xff;
    buffer[1] = (uint8_t)data >> 8;

    if (!i2c_write_memory(i2c_dev, VEML7700_I2C_ADDRESS, reg_addr, 8, buffer, 2)) {
        return FAIL;
    }
    return SUCCESS;
}

/*!
 * @brief Converts raw output data scaled to set parameters
 */
void convert_data_to_lux(uint16_t raw_counts, float* lux)
{
    VEML7700_ALS_GAIN_t gain = current_config_reg.ALS_GAIN;
    VEML7700_ALS_INTEGRATION_TIME_t itime = current_config_reg.ALS_IT;

    float factor1, factor2;

    switch (gain & 0x3) {
    case ALS_GAIN_x1:
        factor1 = 1.f;
        break;
    case ALS_GAIN_x2:
        factor1 = 0.5f;
        break;
    case ALS_GAIN_d8:
        factor1 = 8.f;
        break;
    case ALS_GAIN_d4:
        factor1 = 4.f;
        break;
    default:
        factor1 = 1.f;
        break;
    }

    switch (itime) {
    case ALS_INTEGRATION_25ms:
        factor2 = 0.2304f;
        break;
    case ALS_INTEGRATION_50ms:
        factor2 = 0.1152f;
        break;
    case ALS_INTEGRATION_100ms:
        factor2 = 0.0576f;
        break;
    case ALS_INTEGRATION_200ms:
        factor2 = 0.0288f;
        break;
    case ALS_INTEGRATION_400ms:
        factor2 = 0.0144f;
        break;
    case ALS_INTEGRATION_800ms:
        factor2 = 0.0072f;
        break;
    default:
        factor2 = 0.2304f;
        break;
    }

    *lux = raw_counts * factor1 * factor2;

    // apply correction from App. Note for all readings
    //   using Horner's method
    *lux = *lux * (1.0023f + *lux * (8.1488e-5f + *lux * (-9.3924e-9f + *lux * 6.0135e-13f)));
}

/*!
 * @brief Sets up the default parameters of the sensor
 */
error_t VEML7700_init(i2c_handle_t* i2c_handle)
{
    i2c_dev = i2c_handle;

    VEML7700_configure(current_config_reg);
    VEML7700_set_power_mode(ALS_POWER_MODE_2);
}

/*!
 * @brief Writes the desired settings to the sensor and stores it for the conversion formula
 */
error_t VEML7700_configure(VEML7700_CONFIG_REG reg)
{
    current_config_reg = reg;
    return user_i2c_write(VEML7700_CONFIGURATION_REGISTER, reg.rawData);
}

/*!
 * @brief Sets the desired power mode. In combination with VEML7700_ALS_INTEGRATION_TIME it determines the current consumption and sample interval
 */
error_t VEML7700_set_power_mode(VEML7700_ALS_POWER_MODE mode)
{
    return user_i2c_write(VEML7700_POWER_SAVING_MODES, mode);
}

/*!
 * @brief Reads the ALS output data of the sensor and converts it to lux
 */
error_t VEML7700_read_ALS_Lux(uint16_t* raw_data, float* parsed_data)
{
    error_t ret = user_i2c_read(VEML7700_ALS_HIGH_RESOLUTION_OUTPUT_DATA, raw_data);
    convert_data_to_lux(*raw_data, parsed_data);
    DPRINT("VEML7700 white channel output: %d, scaled output lux %d", *raw_data, (uint32_t)round(*parsed_data));
    return ret;
}

/*!
 * @brief Reads the white output data of the sensor and converts it to lux
 */
error_t VEML7700_read_White_Lux(uint16_t* raw_data, float* parsed_data)
{
    error_t ret = user_i2c_read(VEML7700_WHITE_CHANNEL_OUTPUT_DATA, raw_data);
    convert_data_to_lux(*raw_data, parsed_data);
    DPRINT("VEML7700 white channel output: %d, scaled output lux %d", *raw_data, (uint32_t)round(*parsed_data));
    return ret;
}

/*!
 * @brief Sets the shutdown state of the sensor. If state == true the sensor will enter power down mode.
 */
error_t VEML7700_set_shutdown_state(bool state)
{
  current_config_reg.ALS_SD = state;
  return VEML7700_configure(current_config_reg);
}
