/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef STHS34_STM32_H
#define STHS34_STM32_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include "hwi2c.h"
#include "stm32_common_gpio.h"

typedef void (*sths34_data_change_callback_t)(bool motion_state, bool presence_state);

void sths34pf80_tmos_presence_detection(i2c_handle_t* handler);
void shts34_set_interrupt_enabled_state(bool active);
int32_t shts34_setup_presence_detection(pin_id_t interrupt_pin, bool presence_interrupt, bool motion_interrupt, sths34_data_change_callback_t sths34_data_change_callback);
void shts34_interface_init(i2c_handle_t* handler);


#endif /*STHS34_STM32_H */

