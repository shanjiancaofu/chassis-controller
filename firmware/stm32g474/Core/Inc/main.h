/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32g4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define LED_B_Pin GPIO_PIN_0
#define LED_B_GPIO_Port GPIOC
#define LED_G_Pin GPIO_PIN_1
#define LED_G_GPIO_Port GPIOC
#define LED_R_Pin GPIO_PIN_2
#define LED_R_GPIO_Port GPIOC
#define MOTOR_LEFT_ENC_A_Pin GPIO_PIN_0
#define MOTOR_LEFT_ENC_A_GPIO_Port GPIOA
#define MOTOR_LEFT_ENC_B_Pin GPIO_PIN_1
#define MOTOR_LEFT_ENC_B_GPIO_Port GPIOA
#define MOTOR_SUPPLY_ADC_Pin GPIO_PIN_2
#define MOTOR_SUPPLY_ADC_GPIO_Port GPIOA
#define LCD_RST_Pin GPIO_PIN_5
#define LCD_RST_GPIO_Port GPIOA
#define LCD_BLK_Pin GPIO_PIN_6
#define LCD_BLK_GPIO_Port GPIOA
#define LCD_CS_Pin GPIO_PIN_7
#define LCD_CS_GPIO_Port GPIOA
#define LCD_SCK_Pin GPIO_PIN_13
#define LCD_SCK_GPIO_Port GPIOB
#define LCD_DC_Pin GPIO_PIN_14
#define LCD_DC_GPIO_Port GPIOB
#define LCD_SDA_Pin GPIO_PIN_15
#define LCD_SDA_GPIO_Port GPIOB
#define MOTOR_RIGHT_ENC_A_Pin GPIO_PIN_12
#define MOTOR_RIGHT_ENC_A_GPIO_Port GPIOD
#define MOTOR_RIGHT_ENC_B_Pin GPIO_PIN_13
#define MOTOR_RIGHT_ENC_B_GPIO_Port GPIOD
#define MOTOR_LEFT_IN1_Pin GPIO_PIN_6
#define MOTOR_LEFT_IN1_GPIO_Port GPIOC
#define MOTOR_LEFT_IN2_Pin GPIO_PIN_7
#define MOTOR_LEFT_IN2_GPIO_Port GPIOC
#define MOTOR_RIGHT_IN1_Pin GPIO_PIN_8
#define MOTOR_RIGHT_IN1_GPIO_Port GPIOC
#define MOTOR_RIGHT_IN2_Pin GPIO_PIN_9
#define MOTOR_RIGHT_IN2_GPIO_Port GPIOC
#define E_STOP_Pin GPIO_PIN_2
#define E_STOP_GPIO_Port GPIOD
#define E_STOP_EXTI_IRQn EXTI2_IRQn
#define KEY_Pin GPIO_PIN_8
#define KEY_GPIO_Port GPIOB
#define KEY_EXTI_IRQn EXTI9_5_IRQn

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
