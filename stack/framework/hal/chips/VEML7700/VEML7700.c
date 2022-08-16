#include "VEML7700.h"

#include "errors.h"
#include "hwi2c.h"
#include "hwsystem.h"
#include "log.h"
#include "math.h"
#include <stdio.h>

#ifdef true
#define DPRINT(...) log_print_string(__VA_ARGS__)
#define DPRINT_DATA(...) log_print_data(__VA_ARGS__)
#else
#define DPRINT(...)
#define DPRINT_DATA(...)
#endif

#define VEML7700_I2C_ADDRESS 0x10

static i2c_handle_t* i2c_dev;
static uint16_t measurement_wait_time_ms;
static uint8_t current_power_mode = ALS_POWER_MODE_4;
static uint8_t current_low_power_mode_state = false;

static VEML7700_CONFIG_REG current_config_reg = { .ALS_GAIN = ALS_GAIN_x1,
    .ALS_INT_EN = false,
    .ALS_IT = ALS_INTEGRATION_100ms,
    .ALS_PERS = ALS_PERSISTENCE_1,
    .ALS_SD = 1 };

typedef enum {
    VEML7700_CONFIGURATION_REGISTER = 0x00,
    VEML7700_HIGH_THRESHOLD_WINDOWS_SETTING = 0x01,
    VEML7700_LOW_THRESHOLD_WINDOWS_SETTING = 0x02,
    VEML7700_POWER_SAVING_MODES = 0x03,
    VEML7700_ALS_HIGH_RESOLUTION_OUTPUT_DATA = 0x04,
    VEML7700_WHITE_CHANNEL_OUTPUT_DATA = 0x05,
    VEML7700_INTERRUPT_STATUS = 0x06,
} VEML7700_Pointers;

static void convert_data_to_lux(uint16_t raw_counts, float* lux);
static error_t VEML7700_configure(VEML7700_CONFIG_REG reg);

/*!
 * @brief This function reading the sensor's registers through I2C bus.
 */
static error_t user_i2c_read(uint8_t reg_addr, uint16_t* data)
{
    uint8_t buffer[2];
    if (!i2c_read_memory(i2c_dev, VEML7700_I2C_ADDRESS, reg_addr, 8, buffer, 2)) {
        return FAIL;
    }

    *data = (buffer[1] << 8) | buffer[0];

    return SUCCESS;
}

/*!
 * @brief This function for writing the sensor's registers through I2C bus.
 */
static error_t user_i2c_write(uint8_t reg_addr, uint16_t data)
{
    uint8_t buffer[2];
    buffer[0] = data & 0xff;
    buffer[1] = data >> 8;

    if (!i2c_write_memory(i2c_dev, VEML7700_I2C_ADDRESS, reg_addr, 8, buffer, 2)) {
        return FAIL;
    }
    return SUCCESS;
}

/*!
 * @brief Converts raw output data scaled to set parameters
 */
static void convert_data_to_lux(uint16_t raw_counts, float* lux)
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

    if (raw_counts > 100)
        *lux = *lux * (1.0023f + *lux * (8.1488e-5f + *lux * (-9.3924e-9f + *lux * 6.0135e-13f)));
}

/*!
 * @brief Sets up the default parameters of the sensor
 */
error_t VEML7700_init(i2c_handle_t* i2c_handle)
{
    i2c_dev = i2c_handle;

    VEML7700_configure(current_config_reg);
    VEML7700_set_power_mode(current_power_mode, false);
    VEML7700_set_shutdown_state(true);
}

void VEML7700_change_settings(
    uint8_t integration_time, uint8_t persistence_number, uint8_t gain, bool low_power_enabled, uint8_t low_power_mode)
{
    VEML7700_CONFIG_REG new_config_reg = { .ALS_GAIN = gain,
        .ALS_INT_EN = current_config_reg.ALS_INT_EN,
        .ALS_IT = integration_time,
        .ALS_PERS = persistence_number,
        .ALS_SD = current_config_reg.ALS_SD };

    if (new_config_reg.rawData != current_config_reg.rawData) {
        current_config_reg.rawData = new_config_reg.rawData;
        VEML7700_configure(current_config_reg);
    }

    if (low_power_enabled != current_low_power_mode_state) {
        current_low_power_mode_state = low_power_enabled;
        current_power_mode = low_power_mode;
        VEML7700_set_power_mode(current_power_mode, current_low_power_mode_state);
    }
}

/*!
 * @brief Writes the desired settings to the sensor and stores it for the conversion formula
 */
static error_t VEML7700_configure(VEML7700_CONFIG_REG reg)
{
    current_config_reg = reg;
    measurement_wait_time_ms = 25;
    measurement_wait_time_ms = (current_config_reg.ALS_IT == ALS_INTEGRATION_50ms) ? 50 : measurement_wait_time_ms;
    measurement_wait_time_ms = (current_config_reg.ALS_IT == ALS_INTEGRATION_100ms) ? 100 : measurement_wait_time_ms;
    measurement_wait_time_ms = (current_config_reg.ALS_IT == ALS_INTEGRATION_200ms) ? 200 : measurement_wait_time_ms;
    measurement_wait_time_ms = (current_config_reg.ALS_IT == ALS_INTEGRATION_400ms) ? 400 : measurement_wait_time_ms;
    measurement_wait_time_ms = (current_config_reg.ALS_IT == ALS_INTEGRATION_800ms) ? 800 : measurement_wait_time_ms;
    return user_i2c_write(VEML7700_CONFIGURATION_REGISTER, reg.rawData);
}

/*!
 * @brief Sets the desired power mode. In combination with VEML7700_ALS_INTEGRATION_TIME it determines the current
 * consumption and sample interval
 */
error_t VEML7700_set_power_mode(VEML7700_ALS_POWER_MODE mode, bool power_saving_mode_enabled)
{
    VEML_POWER_MODE_REG_T VEML_POWER_MODE_REG = { .PSM_EN = power_saving_mode_enabled, .PSM = mode };
    return user_i2c_write(VEML7700_POWER_SAVING_MODES, VEML_POWER_MODE_REG.rawData);
}

/*!
 * @brief Reads the ALS output data of the sensor and converts it to lux
 */
error_t VEML7700_read_ALS_Lux(uint16_t* raw_data, float* light_lux)
{
    if (!current_low_power_mode_state) {
        for (uint8_t i = 0; i < measurement_wait_time_ms + 1; i++)
            hw_busy_wait(1000);
    }

    error_t ret = user_i2c_read(VEML7700_ALS_HIGH_RESOLUTION_OUTPUT_DATA, raw_data);
    convert_data_to_lux(*raw_data, light_lux);
    DPRINT("VEML7700 als channel output: %d, lux %d \n", *raw_data, (uint32_t)round(*light_lux));
    return ret;
}

/*!
 * @brief Reads the white output data of the sensor
 */
error_t VEML7700_read_White_Lux(uint16_t* raw_data)
{
    error_t ret = user_i2c_read(VEML7700_WHITE_CHANNEL_OUTPUT_DATA, raw_data);
    DPRINT("VEML7700 white channel output: %d", *raw_data);
    return ret;
}

/*!
 * @brief Sets the shutdown state of the sensor. If state == true the sensor will enter power down mode.
 */
error_t VEML7700_set_shutdown_state(bool state)
{
    if (current_config_reg.ALS_SD != state) {
        current_config_reg.ALS_SD = state;
        error_t ret = VEML7700_configure(current_config_reg);
        if (!state)
            hw_busy_wait(5000);
        return ret;

    } else
        return EALREADY;
}

error_t VEML7700_set_threshold(bool interrupt_enabled, uint16_t threshold_high, uint16_t threshold_low)
{
    user_i2c_write(VEML7700_HIGH_THRESHOLD_WINDOWS_SETTING, threshold_high);
    user_i2c_write(VEML7700_LOW_THRESHOLD_WINDOWS_SETTING, threshold_low);
    if (current_config_reg.ALS_INT_EN != interrupt_enabled) {
        current_config_reg.ALS_INT_EN = interrupt_enabled;
        VEML7700_configure(current_config_reg);
    }
    return SUCCESS;
}

error_t VEML7700_get_interrupt_state(bool* high_triggered, bool* low_triggered)
{
    uint16_t interrupt_state;
    user_i2c_read(VEML7700_INTERRUPT_STATUS, &interrupt_state);
    *high_triggered = interrupt_state & (1 << 14);
    *low_triggered = interrupt_state & (1 << 15);
    return SUCCESS;
}