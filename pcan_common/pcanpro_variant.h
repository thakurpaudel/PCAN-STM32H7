#pragma once
#if !defined(ESP_PLATFORM)
#include "main.h"
#endif

// Define your hardware variant here
// Options: DEVEBOX_H743, NUCLEO_H743, CUSTOM_H743, EMPTY_STUB


#if (defined STM32H743XX)


/* LED Mapping for PCAN Firmware */
/*
* Leads are setup from the cube max
*/
#define LED_STAT_PORT LED_1_GPIO_Port
#define LED_STAT_PIN LED_1_Pin

// Map all Channel Activity to LED_2 (Shared)
#define LED_CH0_TX_PORT LED_2_GPIO_Port
#define LED_CH0_TX_PIN LED_2_Pin
#define LED_CH0_RX_PORT LED_2_GPIO_Port
#define LED_CH0_RX_PIN LED_2_Pin

#define LED_CH1_TX_PORT LED_2_GPIO_Port
#define LED_CH1_TX_PIN LED_2_Pin
#define LED_CH1_RX_PORT LED_2_GPIO_Port
#define LED_CH1_RX_PIN LED_2_Pin

/* LED polarity (active high) */
#define LED_ON GPIO_PIN_SET
#define LED_OFF GPIO_PIN_RESET

/* Hardware initialization - GPIOs are initialized by CubeMX */
#define IO_HW_INIT()

#elif (defined NUCLEO_H743)
/* FDCAN1 pins on NUCLEO-H743ZI (PD0/PD1) */
#define CAN1_RX D, 0, MODE_AF_PP, PULLUP, SPEED_FREQ_VERY_HIGH, AF9_FDCAN1
#define CAN1_TX D, 1, MODE_AF_PP, NOPULL, SPEED_FREQ_VERY_HIGH, AF9_FDCAN1

/* FDCAN2 pins on NUCLEO-H743ZI (PB12/PB13) */
#define CAN2_RX B, 12, MODE_AF_PP, PULLUP, SPEED_FREQ_VERY_HIGH, AF9_FDCAN2
#define CAN2_TX B, 13, MODE_AF_PP, NOPULL, SPEED_FREQ_VERY_HIGH, AF9_FDCAN2

/* Nucleo user LEDs (LD1=Green PB0, LD2=Yellow PB7, LD3=Red PB14) */
#define IO_LED_STAT B, 0, MODE_OUTPUT_PP, NOPULL, SPEED_FREQ_MEDIUM, NOAF
#define IO_LED_TX0 B, 7, MODE_OUTPUT_PP, NOPULL, SPEED_FREQ_MEDIUM, NOAF
#define IO_LED_RX0 B, 14, MODE_OUTPUT_PP, NOPULL, SPEED_FREQ_MEDIUM, NOAF
#define IO_LED_TX1 E, 1, MODE_OUTPUT_PP, NOPULL, SPEED_FREQ_MEDIUM, NOAF
#define IO_LED_RX1 C, 13, MODE_OUTPUT_PP, NOPULL, SPEED_FREQ_MEDIUM, NOAF

/* LED polarity (active high on Nucleo) */
#define IO_LED_HI PIN_HI
#define IO_LED_LOW PIN_LOW

/* Hardware initialization */
#define IO_HW_INIT()

#elif (defined HEXV2_CLONE_H7)
/* FDCAN1 pins - using AF9_FDCAN1 for STM32H7 */
#define CAN1_RX D, 0, MODE_AF_PP, PULLUP, SPEED_FREQ_VERY_HIGH, AF9_FDCAN1
#define CAN1_TX D, 1, MODE_AF_PP, NOPULL, SPEED_FREQ_VERY_HIGH, AF9_FDCAN1

/* LEDs - Red, Blue, Green */
#define IO_LED_STAT                                                            \
  C, 10, MODE_OUTPUT_PP, NOPULL, SPEED_FREQ_MEDIUM, NOAF /* red */
#define IO_LED_TX0                                                             \
  E, 0, MODE_OUTPUT_PP, NOPULL, SPEED_FREQ_MEDIUM, NOAF /* blue */
#define IO_LED_RX0                                                             \
  B, 9, MODE_OUTPUT_PP, NOPULL, SPEED_FREQ_MEDIUM, NOAF /* green */

/* LED polarity (inverted for common anode) */
#define IO_LED_HI PIN_LOW
#define IO_LED_LOW PIN_HI

/* CAN transceiver standby control */
#define CAN1_S E, 10, MODE_OUTPUT_OD, PULLUP, SPEED_FREQ_MEDIUM, NOAF

/* Hardware initialization - enable CAN transceiver */
#define IO_HW_INIT()                                                           \
  PORT_ENABLE_CLOCK(PIN_PORT(CAN1_S), PIN_PORT(CAN1_S));                       \
  PIN_LOW(CAN1_S);                                                             \
  PIN_INIT(CAN1_S)

#elif (defined CUSTOM_H743)
/* Define your custom STM32H743 hardware here */

/* FDCAN1 pins - Customize based on your schematic */
#define CAN1_RX D, 0, MODE_AF_PP, PULLUP, SPEED_FREQ_VERY_HIGH, AF9_FDCAN1
#define CAN1_TX D, 1, MODE_AF_PP, NOPULL, SPEED_FREQ_VERY_HIGH, AF9_FDCAN1

/* FDCAN2 pins - Customize based on your schematic */
#define CAN2_RX B, 12, MODE_AF_PP, PULLUP, SPEED_FREQ_VERY_HIGH, AF9_FDCAN2
#define CAN2_TX B, 13, MODE_AF_PP, NOPULL, SPEED_FREQ_VERY_HIGH, AF9_FDCAN2

/* LED pins - Customize based on your schematic */
#define IO_LED_STAT E, 3, MODE_OUTPUT_PP, NOPULL, SPEED_FREQ_MEDIUM, NOAF
#define IO_LED_TX0 E, 4, MODE_OUTPUT_PP, NOPULL, SPEED_FREQ_MEDIUM, NOAF
#define IO_LED_RX0 E, 5, MODE_OUTPUT_PP, NOPULL, SPEED_FREQ_MEDIUM, NOAF
#define IO_LED_TX1 E, 6, MODE_OUTPUT_PP, NOPULL, SPEED_FREQ_MEDIUM, NOAF
#define IO_LED_RX1 E, 7, MODE_OUTPUT_PP, NOPULL, SPEED_FREQ_MEDIUM, NOAF

/* LED polarity */
#define IO_LED_HI PIN_HI
#define IO_LED_LOW PIN_LOW

/* Optional: CAN transceiver control pins */
// #define CAN1_S        E, 10, MODE_OUTPUT_OD, PULLUP, SPEED_FREQ_MEDIUM, NOAF
// #define CAN2_S        E, 11, MODE_OUTPUT_OD, PULLUP, SPEED_FREQ_MEDIUM, NOAF

/* Hardware initialization */
#define IO_HW_INIT()

#elif (defined DISCO_H745)

/* LED Mapping for STM32H745I-DISCO */
// LED1 is Green LED (LD7), connected to PJ2.
// LED2 is Red LED (LD6), connected to PI13.
// Active Low: 0 = ON, 1 = OFF
#define LED_ON  GPIO_PIN_RESET
#define LED_OFF GPIO_PIN_SET

#define LED_STAT_PORT GPIOJ
#define LED_STAT_PIN  GPIO_PIN_2

// Map Channel 0 and 1 activity to Red LED (LD6 / PI13)
#define LED_CH0_TX_PORT GPIOI
#define LED_CH0_TX_PIN  GPIO_PIN_13
#define LED_CH0_RX_PORT GPIOI
#define LED_CH0_RX_PIN  GPIO_PIN_13

#define LED_CH1_TX_PORT GPIOI
#define LED_CH1_TX_PIN  GPIO_PIN_13
#define LED_CH1_RX_PORT GPIOI
#define LED_CH1_RX_PIN  GPIO_PIN_13

/* Enable GPIOI and GPIOJ clocks and initialize pins as output */
#define IO_HW_INIT() do { \
  __HAL_RCC_GPIOI_CLK_ENABLE(); \
  __HAL_RCC_GPIOJ_CLK_ENABLE(); \
  GPIO_InitTypeDef GPIO_InitStruct = {0}; \
  GPIO_InitStruct.Pin = GPIO_PIN_2; \
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP; \
  GPIO_InitStruct.Pull = GPIO_NOPULL; \
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW; \
  HAL_GPIO_Init(GPIOJ, &GPIO_InitStruct); \
  GPIO_InitStruct.Pin = GPIO_PIN_13; \
  HAL_GPIO_Init(GPIOI, &GPIO_InitStruct); \
} while(0)

#elif (defined EMPTY_STUB)
/* Empty stub for testing without hardware */
#define IO_HW_INIT()

#else
#error                                                                         \
    "Invalid hardware variant! Define one of: DEVEBOX_H743, NUCLEO_H743, HEXV2_CLONE_H7, CUSTOM_H743, EMPTY_STUB"
#endif

/*
 * STM32H743 FDCAN Pin Options Reference:
 *
 * FDCAN1 (AF9):
 * - RX: PD0, PA11, PH14, PI9
 * - TX: PD1, PA12, PH13, PB9
 *
 * FDCAN2 (AF9):
 * - RX: PB5, PB12
 * - TX: PB6, PB13
 *
 * FDCAN3 (AF9) - if available on your package:
 * - RX: PA8, PF6
 * - TX: PA15, PF7
 *
 * Note: Always verify pin availability for your specific package (LQFP100,
 * LQFP144, etc.)
 */