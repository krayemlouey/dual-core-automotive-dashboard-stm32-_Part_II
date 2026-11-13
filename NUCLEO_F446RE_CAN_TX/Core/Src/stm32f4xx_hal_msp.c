/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file         stm32f4xx_hal_msp.c  (NUCLEO-F446RE)
  * @brief        MSP Initialization — active les horloges périphériques
  *               et configure les GPIO pour ADC1, CAN1, USART2.
  ******************************************************************************
  */
/* USER CODE END Header */
#include "main.h"

/**
  * @brief HAL MSP Init global
  */
void HAL_MspInit(void)
{
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();
}

/**
  * @brief ADC MSP Init — active l'horloge ADC1 et configure PA0/PA1 en analogique
  *        IMPORTANT : sans cette fonction, HAL_ADC_Init() échoue silencieusement
  *        et toutes les lectures ADC retournent 0.
  */
void HAL_ADC_MspInit(ADC_HandleTypeDef* hadc)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (hadc->Instance == ADC1)
    {
        /* Activer l'horloge ADC1 (sur APB2) */
        __HAL_RCC_ADC1_CLK_ENABLE();

        /* Activer l'horloge GPIOA */
        __HAL_RCC_GPIOA_CLK_ENABLE();

        /* PA0 = ADC1 CH0 (Vitesse), PA1 = ADC1 CH1 (Carburant) */
        GPIO_InitStruct.Pin  = GPIO_PIN_0 | GPIO_PIN_1;
        GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    }
}

/**
  * @brief ADC MSP DeInit
  */
void HAL_ADC_MspDeInit(ADC_HandleTypeDef* hadc)
{
    if (hadc->Instance == ADC1)
    {
        __HAL_RCC_ADC1_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOA, GPIO_PIN_0 | GPIO_PIN_1);
    }
}

/**
  * @brief CAN MSP Init — configure PB8 (RX) / PB9 (TX) en AF9
  */
void HAL_CAN_MspInit(CAN_HandleTypeDef* hcan)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (hcan->Instance == CAN1)
    {
        __HAL_RCC_CAN1_CLK_ENABLE();
        __HAL_RCC_GPIOB_CLK_ENABLE();

        /* PB8 = CAN1_RX, PB9 = CAN1_TX */
        GPIO_InitStruct.Pin       = GPIO_PIN_8 | GPIO_PIN_9;
        GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull      = GPIO_PULLUP;
        GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF9_CAN1;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    }
}

/**
  * @brief CAN MSP DeInit
  */
void HAL_CAN_MspDeInit(CAN_HandleTypeDef* hcan)
{
    if (hcan->Instance == CAN1)
    {
        __HAL_RCC_CAN1_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOB, GPIO_PIN_8 | GPIO_PIN_9);
    }
}
