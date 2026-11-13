/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
extern portBASE_TYPE IdleTaskHook(void* p);
/* USER CODE END FunctionPrototypes */

/* Hook prototypes */
void vApplicationIdleHook(void);


/* USER CODE BEGIN 2 */
void vApplicationIdleHook( void )
{
   /* vApplicationIdleHook() will only be called if configUSE_IDLE_HOOK is set
   to 1 in FreeRTOSConfig.h. It will be called on each iteration of the idle
   task. It is essential that code added to this hook function never attempts
   to block in any way (for example, call xQueueReceive() with a block time
   specified, or call vTaskDelay()). If the application makes use of the
   vTaskDelete() API function (as this demo application does) then it is also
   important that vApplicationIdleHook() is permitted to return to its calling
   function, because it is the responsibility of the idle task to clean up
   memory allocated by the kernel to any task that has since been deleted. */
vTaskSetApplicationTaskTag(NULL, IdleTaskHook);
}
/* USER CODE END 2 */

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/**
 * @brief  EthernetTask — tâche FreeRTOS dédiée à la communication Jetson Nano
 *
 *  - Appelle UDP_Comms_Init() une seule fois au démarrage
 *  - Appelle UDP_Poll() en boucle (gestion paquets entrants + timers LwIP)
 *  - Envoie la trame JSON de télémétrie vers la Jetson toutes les 200 ms
 *
 *  Les données capteurs sont lues directement depuis SRAM4 (0x38000000)
 *  — même zone que Model::tick() du CM7.
 */
#include "udp_sender.h"
#include <string.h>

/* Pointeurs SRAM4 vers les données partagées CM4 → CM7 → Jetson */
#define SRAM4_BASE_ADDR  0x38000000UL
#define p_speed       ((volatile float*)   (SRAM4_BASE_ADDR + 0x00))
#define p_temperature ((volatile float*)   (SRAM4_BASE_ADDR + 0x04))
#define p_fuel        ((volatile float*)   (SRAM4_BASE_ADDR + 0x08))
#define p_lidar       ((volatile float*)   (SRAM4_BASE_ADDR + 0x18))

void EthernetTask(void *argument)
{
    (void)argument;

    /* Attendre 1 s (suffisant — MPU/clocks déjà configurés avant osKernelStart) */
    printf("[ETH] EthernetTask demarre, attente 1s...\r\n");
    vTaskDelay(pdMS_TO_TICKS(1000));

    /* Initialisation LwIP + sockets UDP */
    printf("[ETH] Appel UDP_Comms_Init()...\r\n");
    int ret = UDP_Comms_Init();
    if (ret != 0)
    {
        /* Échec init Ethernet — loop de retry toutes les 2s */
        printf("[ETH] ERREUR UDP_Comms_Init() = %d ! Retry...\r\n", ret);
        for (;;) {
            vTaskDelay(pdMS_TO_TICKS(2000));
            printf("[ETH] Retry UDP_Comms_Init()...\r\n");
            if (UDP_Comms_Init() == 0) {
                printf("[ETH] UDP_Comms_Init() OK apres retry !\r\n");
                break;
            }
        }
    }
    else
    {
        printf("[ETH] UDP_Comms_Init() OK ! LwIP actif, IP=192.168.1.50\r\n");
    }

    uint32_t lastSendTick = 0;
    uint32_t lastLogTick  = 0;

    for (;;)
    {
        /* Traitement des commandes et alertes série reçues de la Jetson Nano */
        extern void USART1_ProcessRx(void);
        USART1_ProcessRx();

        /* Polling LwIP : traite les paquets entrants + timers ARP/TCP */
        UDP_Poll();

        uint32_t now = xTaskGetTickCount();

        /* Log état lien et télémétrie CAN toutes les 3s */
        if ((now - lastLogTick) >= pdMS_TO_TICKS(3000)) {
            lastLogTick = now;
            volatile uint32_t *p_can_cnt = (volatile uint32_t*)0x3800001C;
            volatile uint8_t  *p_cm4_hb  = (volatile uint8_t*) 0x38000016;
            printf("[ETH+CAN] Link:%s | CAN_RX_Cnt=%lu | Vitesse=%.1f km/h | Fuel=%.1f%% | CM4_HB=%u\r\n",
                   netif_is_link_up(udp_get_netif()) ? "UP" : "DOWN",
                   (unsigned long)*p_can_cnt, *p_speed, *p_fuel, *p_cm4_hb);
        }

        /* Envoi télémétrie toutes les 200 ms */
        if ((now - lastSendTick) >= pdMS_TO_TICKS(200))
        {
            lastSendTick = now;

            float spd  = *p_speed;
            float temp = *p_temperature;
            float fuel = *p_fuel;
            float lidar= *p_lidar;

            uint8_t gear = 0;
            if      (spd <= 0.0f)  gear = 0;
            else if (spd <  20.0f) gear = 1;
            else if (spd <  40.0f) gear = 2;
            else if (spd <  70.0f) gear = 3;
            else if (spd < 100.0f) gear = 4;
            else if (spd < 140.0f) gear = 5;
            else                   gear = 6;

            UDP_SendTelemetry(spd, temp, fuel, lidar, gear);
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

/* USER CODE END Application */

