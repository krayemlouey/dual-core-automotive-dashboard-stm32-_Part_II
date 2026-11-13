/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c  (CM4 core)
  * @brief          : Réception CAN (FDCAN1) depuis NUCLEO-F446RE → SRAM4 partagée → CM7 dashboard
  *
  * Architecture :
  *   1. NUCLEO-F446RE lit les capteurs (ADC vitesse, ADC carburant, DHT11, LiDAR)
  *      et transmet via CAN (500 kbps) avec CAN ID 0x100.
  *   2. CM4 reçoit la trame FDCAN1 et écrit les données en SRAM4 @ 0x38000000.
  *   3. CM7 lit SRAM4 dans Model::tick() et met à jour le dashboard TouchGFX.
  *
  * Protocole CAN (Payload 8 octets) ID 0x100 :
  *   [0-1] speed_x10   : uint16_t, vitesse * 10  (ex: 1234 = 123.4 km/h)
  *   [2-3] temp_x10    : uint16_t, temp * 10     (ex:  235 =  23.5 °C)
  *   [4-5] fuel_x10    : uint16_t, fuel * 10     (ex:  653 =  65.3 %)
  *   [6-7] lidar_cm    : uint16_t, distance cm   (ex:  120 = 120 cm)
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include <math.h>

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct {
    float speed;           // offset 0x00 — Vitesse (km/h) reçue par CAN
    float temperature;     // offset 0x04 — Température (°C) reçue par CAN
    float fuel;            // offset 0x08 — Carburant (%) reçu par CAN
    float gps_lat;         // offset 0x0C — GPS (fixe, non transmis)
    float gps_lon;         // offset 0x10 — GPS (fixe, non transmis)
    uint8_t gps_updated;   // offset 0x14 — flag GPS
    uint8_t demo_mode;     // offset 0x15 — 1 si simulation (pas de CAN réel)
    uint8_t padding[2];    // offset 0x16 — alignement
    float lidar_cm;        // offset 0x18 — Distance LiDAR (cm) reçue par CAN
    uint32_t can_rx_count; // offset 0x1C — Nombre total de trames CAN reçues (diagnostic)
    /* ── Champs Jetson Nano (ajoutés phase 2) ─────────────────────────── */
    char alert_text[64];   // offset 0x20 — Message d'alerte IA Jetson (ex: "OBSTACLE")
    uint8_t alert_level;   // offset 0x60 — 0=OK, 1=WARNING, 2=DANGER
    uint8_t ethernet_ok;   // offset 0x61 — 1 si lien Ethernet Jetson actif
    uint8_t jetson_pad[2]; // offset 0x62 — réservé
} SharedData_t;

volatile SharedData_t* sharedData = (volatile SharedData_t*)0x38000000;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* ID de la trame CAN envoyée par le NUCLEO-F446RE */
#define CAN_ID_TELEMETRY    0x100U

/* DUAL_CORE_BOOT_SYNC_SEQUENCE */
#define DUAL_CORE_BOOT_SYNC_SEQUENCE

#if defined(DUAL_CORE_BOOT_SYNC_SEQUENCE)
#ifndef HSEM_ID_0
#define HSEM_ID_0 (0U)
#endif
#endif /* DUAL_CORE_BOOT_SYNC_SEQUENCE */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
FDCAN_HandleTypeDef hfdcan1;  /* instance = FDCAN2 (PB5 RX / PB6 TX) */

/* USER CODE BEGIN PV */
/* Dernières valeurs reçues (mise à jour par le callback FDCAN) */
volatile float   can_speed    = 0.0f;
volatile float   can_temp     = 21.5f;
volatile float   can_fuel     = 50.0f;
volatile float   can_lidar    = -1.0f;
volatile uint8_t can_data_fresh  = 0;   /* flag : nouvelle trame reçue */
volatile uint32_t can_rx_total   = 0;   /* compteur total trames reçues  */
volatile uint32_t can_last_rx_ms = 0;   /* timestamp dernière réception  */

/* -----------------------------------------------------------------------
 * CAN_DEMO_MODE : si aucune trame reçue dans les 3 premières secondes,
 * CM4 génère des données synthétiques pour valider le pipeline SRAM4→CM7.
 * Désactivez cette macro une fois le câblage CAN vérifié.
 * --------------------------------------------------------------------- */
#define CAN_DEMO_TIMEOUT_MS   3000U   /* 3 s sans trame → mode démo */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
static void MX_FDCAN1_Init(void);  /* FDCAN1 sur PA11(RX)/PA12(TX) — PMOD P3 */
static void MX_MDMA_Init(void);

void DWT_Delay_us(volatile uint32_t us)
{
    volatile uint32_t count = us * 60;
    while(count--) { __asm("nop"); }
}

/**
  * @brief Callback FDCAN RX : appelé automatiquement à chaque trame reçue
  *        Décode le payload et stocke dans les variables globales.
  */
static void Process_CAN_Frame(const uint8_t *rx_data)
{
    uint16_t speed_x10  = (uint16_t)rx_data[0] | ((uint16_t)rx_data[1] << 8);
    uint16_t temp_x10   = (uint16_t)rx_data[2] | ((uint16_t)rx_data[3] << 8);
    uint16_t fuel_x10   = (uint16_t)rx_data[4] | ((uint16_t)rx_data[5] << 8);
    uint16_t lidar_raw  = (uint16_t)rx_data[6] | ((uint16_t)rx_data[7] << 8);

    sharedData->speed        = (float)speed_x10 / 10.0f;
    sharedData->temperature  = (float)temp_x10  / 10.0f;
    sharedData->fuel         = (float)fuel_x10  / 10.0f;
    sharedData->lidar_cm     = (lidar_raw == 0xFFFF) ? -1.0f : (float)lidar_raw;
    sharedData->demo_mode    = 0;
    sharedData->can_rx_count = ++can_rx_total;
    can_last_rx_ms           = HAL_GetTick();
}

/**
  * @brief Callback FDCAN RX : appelé automatiquement à chaque trame reçue
  */
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs)
{
    /* La réception est gérée directement et proprement dans le while(1) de main() */
    (void)hfdcan;
    (void)RxFifo0ITs;
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  /* Enable Cortex-M4 FPU CP10 and CP11 Full Access */
  SCB->CPACR |= ((3UL << (10*2)) | (3UL << (11*2)));
  __DSB();
  __ISB();
  /* USER CODE END 1 */

/* USER CODE BEGIN Boot_Mode_Sequence_1 */
  /*HW semaphore Clock enable*/
  __HAL_RCC_HSEM_CLK_ENABLE();
  /* Activate HSEM notification for Cortex-M4*/
  HAL_HSEM_ActivateNotification(__HAL_HSEM_SEMID_TO_MASK(HSEM_ID_0));
  /*
  Domain D2 goes to STOP mode (Cortex-M4 in deep-sleep) waiting for Cortex-M7 to
  perform system initialization (system clock config, external memory configuration.. )
  */
  HAL_PWREx_ClearPendingEvent();
  HAL_PWREx_EnterSTOPMode(PWR_MAINREGULATOR_ON, PWR_STOPENTRY_WFE, PWR_D2_DOMAIN);
  /* Clear HSEM flag */
  __HAL_HSEM_CLEAR_FLAG(__HAL_HSEM_SEMID_TO_MASK(HSEM_ID_0));
/* USER CODE END Boot_Mode_Sequence_1 */

  /* MCU Configuration--------------------------------------------------------*/
  SystemCoreClockUpdate();
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_MDMA_Init();
  MX_FDCAN1_Init();  /* FDCAN1 : PA11(RX) / PA12(TX) sur connecteur PMOD P3 */

  /* USER CODE BEGIN 2 */

  /* Coordonnées GPS fixes */
  sharedData->gps_lat      = 35.8256f;
  sharedData->gps_lon      = 10.6369f;
  sharedData->gps_updated  = 1;
  sharedData->demo_mode    = 0;
  sharedData->can_rx_count = 0;

  /* Valeurs initiales */
  sharedData->speed        = 0.0f;
  sharedData->fuel         = 50.0f;
  sharedData->temperature  = 21.0f;
  sharedData->lidar_cm     = -1.0f;
  sharedData->padding[0]   = 0;
  sharedData->padding[1]   = 0xAA; /* 0xAA = CM4 démarré avec succès */

  /* Champs Jetson Nano — initialisés à vide */
  sharedData->alert_text[0] = '\0';
  sharedData->alert_level   = 0;
  sharedData->ethernet_ok   = 0;


  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* Dépouillement de tous les messages présents dans la FIFO0 */
    while (HAL_FDCAN_GetRxFifoFillLevel(&hfdcan1, FDCAN_RX_FIFO0) > 0)
    {
      FDCAN_RxHeaderTypeDef rx_hdr;
      uint8_t rx_data[8];
      if (HAL_FDCAN_GetRxMessage(&hfdcan1, FDCAN_RX_FIFO0, &rx_hdr, rx_data) == HAL_OK)
      {
        Process_CAN_Frame(rx_data);
      }
      else
      {
        /* Sortie impérative pour éviter de boucler à l'infini si une trame n'est pas lue */
        break;
      }
    }

    /* Acquitter tout dépassement de capacité éventuel de la FIFO0 */
    if (__HAL_FDCAN_GET_FLAG(&hfdcan1, FDCAN_FLAG_RX_FIFO0_MESSAGE_LOST))
    {
      __HAL_FDCAN_CLEAR_FLAG(&hfdcan1, FDCAN_FLAG_RX_FIFO0_MESSAGE_LOST);
    }

    /* Pas de mode démo : on garde les valeurs initiales fixes (0 km/h, 50% carburant, 21°C)
     * jusqu'à la réception de la première trame CAN. Aucune oscillation synthétique. */
    sharedData->demo_mode = (can_rx_total == 0) ? 1 : 0;

    /* Compteur heartbeat visible en mémoire partagée */
    sharedData->padding[0]++;

    /* Délai court non bloquant */
    for (volatile uint32_t d = 0; d < 2000; d++) { __NOP(); }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief FDCAN1 Initialization — 500 kbps @ HSE=25 MHz
  *        PA11 = FDCAN1_RX (AF9)  /  PA12 = FDCAN1_TX (AF9) — PMOD P3
  * @retval None
  */
static void MX_FDCAN1_Init(void)
{
  /* L'horloge FDCAN et les GPIO PA11/PA12 sont configurés dans HAL_FDCAN_MspInit() */
  __HAL_RCC_FDCAN_CLK_ENABLE();

  hfdcan1.Instance                  = FDCAN1;
  hfdcan1.Init.FrameFormat          = FDCAN_FRAME_CLASSIC;
  hfdcan1.Init.Mode                 = FDCAN_MODE_NORMAL;
  hfdcan1.Init.AutoRetransmission   = ENABLE;
  hfdcan1.Init.TransmitPause        = DISABLE;
  hfdcan1.Init.ProtocolException    = DISABLE;
  /* 500 kbps @ HSE=25 MHz : Prescaler=2, Seg1=19, Seg2=5 (Sample Point=80%) */
  hfdcan1.Init.NominalPrescaler     = 2;
  hfdcan1.Init.NominalSyncJumpWidth = 4;
  hfdcan1.Init.NominalTimeSeg1      = 19;
  hfdcan1.Init.NominalTimeSeg2      = 5;
  hfdcan1.Init.DataPrescaler        = 1;
  hfdcan1.Init.DataSyncJumpWidth    = 1;
  hfdcan1.Init.DataTimeSeg1         = 1;
  hfdcan1.Init.DataTimeSeg2         = 1;
  hfdcan1.Init.MessageRAMOffset     = 0;
  hfdcan1.Init.StdFiltersNbr        = 1;
  hfdcan1.Init.ExtFiltersNbr        = 0;
  hfdcan1.Init.RxFifo0ElmtsNbr     = 16;
  hfdcan1.Init.RxFifo0ElmtSize     = FDCAN_DATA_BYTES_8;
  hfdcan1.Init.RxFifo1ElmtsNbr     = 0;
  hfdcan1.Init.RxFifo1ElmtSize     = FDCAN_DATA_BYTES_8;
  hfdcan1.Init.RxBuffersNbr        = 0;
  hfdcan1.Init.RxBufferSize        = FDCAN_DATA_BYTES_8;
  hfdcan1.Init.TxEventsNbr         = 0;
  hfdcan1.Init.TxBuffersNbr        = 0;
  hfdcan1.Init.TxFifoQueueElmtsNbr = 0;
  hfdcan1.Init.TxFifoQueueMode     = FDCAN_TX_FIFO_OPERATION;
  hfdcan1.Init.TxElmtSize          = FDCAN_DATA_BYTES_8;

  if (HAL_FDCAN_Init(&hfdcan1) != HAL_OK)
  {
    sharedData->padding[1] = 0xF1; /* F1 = Init error */
    Error_Handler();
  }

  /* Filtre permissif : accepter toutes les trames dans RxFifo0 (exactement comme MyApplication) */
  FDCAN_FilterTypeDef filter = {0};
  filter.IdType       = FDCAN_STANDARD_ID;
  filter.FilterIndex  = 0;
  filter.FilterType   = FDCAN_FILTER_MASK;
  filter.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
  filter.FilterID1    = 0x000;
  filter.FilterID2    = 0x000;
  HAL_FDCAN_ConfigFilter(&hfdcan1, &filter);

  HAL_FDCAN_ConfigGlobalFilter(&hfdcan1,
    FDCAN_ACCEPT_IN_RX_FIFO0, FDCAN_ACCEPT_IN_RX_FIFO0,
    FDCAN_FILTER_REMOTE, FDCAN_FILTER_REMOTE);

  /* Mode OVERWRITE : une nouvelle trame écrase la plus ancienne si la FIFO est pleine.
   * Empêche définitivement le blocage de la réception FDCAN ! */
  HAL_FDCAN_ConfigRxFifoOverwrite(&hfdcan1, FDCAN_RX_FIFO0, FDCAN_RX_FIFO_OVERWRITE);

  /* Démarrer FDCAN1 */
  if (HAL_FDCAN_Start(&hfdcan1) != HAL_OK)
  {
    sharedData->padding[1] = 0xF2; /* F2 = Start error */
  }
  else
  {
    sharedData->padding[1] = 0x55; /* 0x55 = FDCAN démarré avec succès */
  }
}

/**
  * Enable MDMA controller clock
  */
static void MX_MDMA_Init(void)
{

  /* MDMA controller clock enable */
  __HAL_RCC_MDMA_CLK_ENABLE();
  /* Local variables */

}

/* MX_UART8_Init supprimé */


/**
  * @brief GPIO Initialization Function (minimal : horloges seulement)
  * @param None
  * @retval None
  */
void MX_GPIO_Init(void)
{
  /* Seule l'horloge GPIOA est nécessaire pour FDCAN1 (PA11/PA12)
   * Les autres GPIO (ADC, DHT11, LiDAR) ne sont plus utilisés sur le H747. */
  __HAL_RCC_GPIOA_CLK_ENABLE();
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
