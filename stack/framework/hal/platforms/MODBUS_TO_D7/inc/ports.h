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

#ifndef __PORTS_H_
#define __PORTS_H_

#include "stm32_device.h"
#include "stm32_common_mcu.h"
#include "platform_defs.h"
#include "hwgpio.h"


#define SPI_COUNT 1
static const spi_port_t spi_ports[SPI_COUNT] = {
  {
    .spi = SPI1,
    .miso_pin = PIN(GPIO_PORTA, 6),
    .mosi_pin = PIN(GPIO_PORTA, 7),
    .sck_pin = PIN(GPIO_PORTB, 3),
    .sck_alternate = GPIO_AF0_SPI1,
    .miso_alternate = GPIO_AF0_SPI1,
    .mosi_alternate = GPIO_AF0_SPI1

  }
};

#define UART_COUNT 1
static const uart_port_t uart_ports[UART_COUNT] = {
  {
    // USART1, exposed on CN3 header: TX=PA9, RX=PA10
    .tx = PIN(GPIO_PORTA, 9),
    .rx = PIN(GPIO_PORTA, 10),
    .alternate = GPIO_AF4_USART1,
    .uart = USART1,
    .irq = USART1_IRQn
  }
};



#endif
