
#ifndef BME280_STM32_H_
#define BME280_STM32_H_


/* Header includes */
#include "bme280.h"
#include "errors.h"

error_t bme280_stm32_init(i2c_handle_t* handle);
int8_t bme280_stm32_get_sensor_values(float* parsed_temperature, float* parsed_humidity, float* parsed_pressure);

#endif /* BME280_STM32_H_ */
/** @}*/
