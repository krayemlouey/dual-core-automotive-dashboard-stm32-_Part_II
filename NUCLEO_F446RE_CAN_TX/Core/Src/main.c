/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c  (NUCLEO-F446RE — Noeud Capteurs CAN TX)
  * @brief          : Acquisition multi-capteurs + transmission CAN vers STM32H747
  *
  * Capteurs :
  *   - ADC1 CH0 (PA0) : Potentiometre Vitesse  -> 0-260 km/h
  *   - ADC1 CH1 (PA1) : Potentiometre Carburant -> 0-100 %
  *   - DHT11 (PA4)    : Temperature             -> 0-50 C
  *   - TFmini-S USART2 RX (PA3) : LiDAR         -> 0-1200 cm
  *
  * Protocole CAN - Trame ID 0x100 (8 octets, 500 kbps) :
  *   [0-1] speed_x10  : uint16_t LE, vitesse * 10
  *   [2-3] temp_x10   : uint16_t LE, temperature * 10
  *   [4-5] fuel_x10   : uint16_t LE, carburant * 10
  *   [6-7] lidar_cm   : uint16_t LE, distance cm (0xFFFF = invalide)
  *
  * Broches CAN : PB8 (RX) / PB9 (TX) via transceiver SN65HVD230 ou TJA1050
  * Cablage :
  *   Nucleo PB8/PB9 -> Transceiver CAN -> Bus CAN (120 Ohm)
  *   H747   PA11/PA12 -> Transceiver CAN -> Bus CAN (120 Ohm)
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include <string.h>

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define CAN_ID_TELEMETRY    0x100U
#define DHT11_PORT          GPIOA
#define DHT11_PIN           GPIO_PIN_4
#define LIDAR_TIMEOUT_MS    20U

/* Active le LiDAR TFmini-S sur USART2 (PA3 = RX / PA2 = TX). */
#define LIDAR_ENABLED 1
/* USER CODE END PD */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
CAN_HandleTypeDef hcan1;

/* USER CODE BEGIN PV */
static volatile uint16_t adc_speed_raw   = 0;
static volatile uint16_t adc_fuel_raw    = 0;
static volatile float    dht11_temp      = 21.5f;
static volatile uint32_t last_dht11_ms   = 0;
static volatile uint16_t lidar_cm        = 0xFFFF;

static CAN_TxHeaderTypeDef can_tx_header;
static uint8_t             can_tx_data[8];
static uint32_t            can_tx_mailbox;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_CAN1_Init(void);
#ifdef LIDAR_ENABLED
static void MX_USART1_Init(void);
static void MX_USART2_Init(void);
#endif

/* USER CODE BEGIN PFP */

/* ---- Delai us calibre pour ~180 MHz ---- */
static void Delay_us(uint32_t us)
{
    volatile uint32_t count = us * 45;
    while (count--) { __asm("nop"); }
}

/* ======================== DHT11 ======================== */
static void DHT11_Start(void)
{
    GPIO_InitTypeDef g = {0};
    g.Pin   = DHT11_PIN;
    g.Mode  = GPIO_MODE_OUTPUT_OD;
    g.Pull  = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(DHT11_PORT, &g);

    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_RESET);
    HAL_Delay(20);
    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_SET);
    Delay_us(30);

    g.Mode = GPIO_MODE_INPUT;
    HAL_GPIO_Init(DHT11_PORT, &g);
}

static uint8_t DHT11_CheckResponse(void)
{
    uint32_t t = 0;
    while ( HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN)) { if (++t > 50000) return 0; }
    t = 0;
    while (!HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN)) { if (++t > 50000) return 0; }
    t = 0;
    while ( HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN)) { if (++t > 50000) return 0; }
    return 1;
}

static uint8_t DHT11_ReadByte(void)
{
    uint8_t val = 0;
    for (int bit = 7; bit >= 0; bit--)
    {
        uint32_t t = 0;
        while (!HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN)) { if (++t > 50000) break; }
        Delay_us(40);
        if (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN))
        {
            val |= (1u << bit);
            t = 0;
            while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN)) { if (++t > 50000) break; }
        }
    }
    return val;
}

static void DHT11_Read(void)
{
    DHT11_Start();
    if (!DHT11_CheckResponse()) return;

    uint8_t rh1 = DHT11_ReadByte();
    uint8_t rh2 = DHT11_ReadByte();
    uint8_t tp1 = DHT11_ReadByte();
    uint8_t tp2 = DHT11_ReadByte();
    uint8_t cks = DHT11_ReadByte();

    if ((uint8_t)(rh1 + rh2 + tp1 + tp2) == cks)
    {
        dht11_temp = (float)tp1 + (float)tp2 * 0.1f;
    }
}

/* ======================== LiDAR TFmini-S (Interruption RX) ======================== */
#ifdef LIDAR_ENABLED
static void LiDAR_ProcessByte(uint8_t byte)
{
    static uint8_t state = 0;
    static uint8_t buf[9];

    switch (state)
    {
    case 0:
        if (byte == 0x59)
        {
            buf[0] = 0x59;
            state = 1;
        }
        break;
    case 1:
        if (byte == 0x59)
        {
            buf[1] = 0x59;
            state = 2;
        }
        else
        {
            state = (byte == 0x59) ? 1 : 0;
        }
        break;
    default:
        buf[state++] = byte;
        if (state >= 9)
        {
            uint16_t sum = 0;
            for (int i = 0; i < 8; i++) sum += buf[i];
            if ((uint8_t)(sum & 0xFF) == buf[8])
            {
                uint16_t dist = (uint16_t)buf[2] | ((uint16_t)buf[3] << 8);
                if (dist > 0 && dist <= 1200)
                {
                    lidar_cm = dist;
                }
            }
            state = 0;
        }
        break;
    }
}

void USART1_IRQHandler(void)
{
    /* Vider les erreurs ORE/FE/NE */
    if (USART1->SR & (USART_SR_ORE | USART_SR_FE | USART_SR_NE))
    {
        (void)USART1->SR;
        (void)USART1->DR;
    }
    if (USART1->SR & USART_SR_RXNE)
    {
        uint8_t b = (uint8_t)(USART1->DR & 0xFF);
        LiDAR_ProcessByte(b);
    }
}

void USART2_IRQHandler(void)
{
    if (USART2->SR & (USART_SR_ORE | USART_SR_FE | USART_SR_NE))
    {
        (void)USART2->SR;
        (void)USART2->DR;
    }
    if (USART2->SR & USART_SR_RXNE)
    {
        uint8_t b = (uint8_t)(USART2->DR & 0xFF);
        LiDAR_ProcessByte(b);
    }
}

static void LiDAR_Read(void)
{
    /* Rien à faire en polling : les interruptions USART1 et USART2
     * capturent chaque octet instantanément en arrière-plan sans perte ! */
}
#endif /* LIDAR_ENABLED */

/* ======================== ADC ======================== */
static uint16_t speed_filtered = 0;
static uint16_t fuel_filtered  = 0;

static void ADC_ReadAll(void)
{
    ADC_ChannelConfTypeDef sc = {0};
    sc.Rank         = 1;
    sc.SamplingTime = ADC_SAMPLETIME_56CYCLES;

    /* Canal 0 : Vitesse (PA0) */
    sc.Channel = ADC_CHANNEL_0;
    HAL_ADC_ConfigChannel(&hadc1, &sc);
    HAL_ADC_Start(&hadc1);
    if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK)
    {
        uint16_t raw = (uint16_t)HAL_ADC_GetValue(&hadc1);
        /* Filtre 50/50 : réactif (~20ms lag à 50Hz) sans bruit excessif */
        speed_filtered = (speed_filtered == 0) ? raw
                       : (uint16_t)(((uint32_t)speed_filtered + raw) / 2);
        adc_speed_raw = speed_filtered;
    }
    HAL_ADC_Stop(&hadc1);

    /* Canal 1 : Carburant (PA1) */
    sc.Channel = ADC_CHANNEL_1;
    HAL_ADC_ConfigChannel(&hadc1, &sc);
    HAL_ADC_Start(&hadc1);
    if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK)
    {
        uint16_t raw = (uint16_t)HAL_ADC_GetValue(&hadc1);
        /* Filtre 50/50 pour le carburant aussi */
        fuel_filtered = (fuel_filtered == 0) ? raw
                      : (uint16_t)(((uint32_t)fuel_filtered + raw) / 2);
        adc_fuel_raw = fuel_filtered;
    }
    HAL_ADC_Stop(&hadc1);
}

/* ======================== CAN TX ======================== */
static void CAN_SendTelemetry(void)
{
    /* Vraies valeurs directes des potentiomètres (0-260 km/h pour la vitesse, 0-100% pour le carburant) */
    float speed_f = (float)adc_speed_raw * (260.0f / 4095.0f);
    float fuel_f  = (float)adc_fuel_raw  * (100.0f / 4095.0f);

    uint16_t speed_x10 = (uint16_t)(speed_f * 10.0f);
    uint16_t temp_x10  = (uint16_t)(dht11_temp * 10.0f);
    uint16_t fuel_x10  = (uint16_t)(fuel_f  * 10.0f);
    uint16_t ldr       = lidar_cm;

    can_tx_data[0] = (uint8_t)(speed_x10 & 0xFF);
    can_tx_data[1] = (uint8_t)(speed_x10 >> 8);
    can_tx_data[2] = (uint8_t)(temp_x10  & 0xFF);
    can_tx_data[3] = (uint8_t)(temp_x10  >> 8);
    can_tx_data[4] = (uint8_t)(fuel_x10  & 0xFF);
    can_tx_data[5] = (uint8_t)(fuel_x10  >> 8);
    can_tx_data[6] = (uint8_t)(ldr & 0xFF);
    can_tx_data[7] = (uint8_t)(ldr >> 8);

    /* Si aucune boîte aux lettres n'est libre (trame en attente d'ACK ou bus saturé),
     * on annule la requête en attente pour TOUJOURS envoyer la valeur la plus récente ! */
    if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) == 0)
    {
        HAL_CAN_AbortTxRequest(&hcan1, CAN_TX_MAILBOX0 | CAN_TX_MAILBOX1 | CAN_TX_MAILBOX2);
    }

    if (HAL_CAN_AddTxMessage(&hcan1, &can_tx_header, can_tx_data, &can_tx_mailbox) == HAL_OK)
    {
        /* Faire clignoter la LED verte LD2 (PA5) à chaque envoi pour prouver que le Nucleo tourne */
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
    }
}

/* USER CODE END PFP */

/* USER CODE BEGIN 0 */
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */
  /* USER CODE END 1 */

  HAL_Init();

  /* USER CODE BEGIN Init */
  /* USER CODE END Init */

  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  /* USER CODE END SysInit */

  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_CAN1_Init();
#ifdef LIDAR_ENABLED
  MX_USART1_Init();
  MX_USART2_Init();
#endif

  /* USER CODE BEGIN 2 */

  /* En-tete CAN fixe */
  can_tx_header.StdId              = CAN_ID_TELEMETRY;
  can_tx_header.ExtId              = 0x00;
  can_tx_header.RTR                = CAN_RTR_DATA;
  can_tx_header.IDE                = CAN_ID_STD;
  can_tx_header.DLC                = 8;
  can_tx_header.TransmitGlobalTime = DISABLE;

  /* Démarrage CAN — AutoWakeUp est activé, SLEEP déjà cleared dans Init */
  /* On attend que SLAK (Sleep Acknowledge) soit à 0 avant de démarrer */
  uint32_t t0 = HAL_GetTick();
  while ((CAN1->MSR & CAN_MSR_SLAK) && (HAL_GetTick() - t0 < 100)) {}

  HAL_StatusTypeDef can_start_status = HAL_ERROR;
  for (int r = 0; r < 10; r++)
  {
      CAN1->MCR &= ~CAN_MCR_SLEEP;   /* force clear SLEEP à chaque tentative */
      HAL_Delay(5);
      can_start_status = HAL_CAN_Start(&hcan1);
      if (can_start_status == HAL_OK) break;
      HAL_Delay(20);
  }

  /* Boot blink LED LD2 */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
  HAL_Delay(200);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* 1. Lecture ADC (vitesse + carburant) */
    ADC_ReadAll();

    /* 2. Lecture LiDAR TFmini-S */
#ifdef LIDAR_ENABLED
    LiDAR_Read();
#endif

    /* 3. Lecture DHT11 toutes les 2 secondes */
    if (HAL_GetTick() - last_dht11_ms >= 2000)
    {
        last_dht11_ms = HAL_GetTick();
        DHT11_Read();
    }

    /* 4. Envoi trame CAN (fait clignoter LD2 si succès) */
    CAN_SendTelemetry();

    /* 5. Periode : 50 trames/s pour reactivite immediate sans lag */
    HAL_Delay(20);
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Config : HSE 8 MHz -> PLL -> 180 MHz, APB1=45 MHz
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState       = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM       = 8;
  RCC_OscInitStruct.PLL.PLLN       = 360;
  RCC_OscInitStruct.PLL.PLLP       = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ       = 8;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) { Error_Handler(); }

  if (HAL_PWREx_EnableOverDrive() != HAL_OK) { Error_Handler(); }

  RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK  | RCC_CLOCKTYPE_SYSCLK
                                   | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;   /* APB1 = 45 MHz */
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;   /* APB2 = 90 MHz */
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) { Error_Handler(); }
}

/**
  * @brief ADC1 Init : 12 bits, software trigger, single conversion
  */
static void MX_ADC1_Init(void)
{
  /* IMPORTANT : l'horloge ADC1 et les GPIO PA0/PA1 sont configurés
   * dans HAL_ADC_MspInit() (stm32f4xx_hal_msp.c) appelé par HAL_ADC_Init().
   * Ne pas dupliquer les appels ici. */

  hadc1.Instance                   = ADC1;
  hadc1.Init.ClockPrescaler        = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution            = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode          = DISABLE;
  hadc1.Init.ContinuousConvMode    = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion       = 1;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.EOCSelection          = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK) { Error_Handler(); }
}

/**
  * @brief CAN1 Init : 500 kbps @ APB1=45 MHz
  *        Prescaler=10, BS1=6TQ, BS2=2TQ -> 45M/(10*9) = 500 kbps
  */
static void MX_CAN1_Init(void)
{
  __HAL_RCC_CAN1_CLK_ENABLE();

  hcan1.Instance                  = CAN1;
  hcan1.Init.Prescaler            = 10;
  hcan1.Init.Mode                 = CAN_MODE_NORMAL;
  hcan1.Init.SyncJumpWidth        = CAN_SJW_1TQ;
  hcan1.Init.TimeSeg1             = CAN_BS1_6TQ;
  hcan1.Init.TimeSeg2             = CAN_BS2_2TQ;
  hcan1.Init.TimeTriggeredMode    = DISABLE;
  hcan1.Init.AutoBusOff           = ENABLE;
  hcan1.Init.AutoWakeUp           = ENABLE;   /* réveil HW automatique, pas besoin de WakeUp() */
  hcan1.Init.AutoRetransmission   = ENABLE;   /* Retransmet automatiquement jusqu'à ACK */
  hcan1.Init.ReceiveFifoLocked    = DISABLE;
  hcan1.Init.TransmitFifoPriority = DISABLE;
  if (HAL_CAN_Init(&hcan1) != HAL_OK) { Error_Handler(); }

  /* Force la sortie du mode SLEEP au niveau registre */
  CAN1->MCR &= ~CAN_MCR_SLEEP;

  /* Filtre permissif (Nucleo = TX seulement) */
  CAN_FilterTypeDef f = {0};
  f.FilterBank           = 0;
  f.FilterMode           = CAN_FILTERMODE_IDMASK;
  f.FilterScale          = CAN_FILTERSCALE_32BIT;
  f.FilterIdHigh         = 0x0000;
  f.FilterIdLow          = 0x0000;
  f.FilterMaskIdHigh     = 0x0000;
  f.FilterMaskIdLow      = 0x0000;
  f.FilterFIFOAssignment = CAN_RX_FIFO0;
  f.FilterActivation     = ENABLE;
  HAL_CAN_ConfigFilter(&hcan1, &f);
}


#ifdef LIDAR_ENABLED
/**
  * @brief USART1 Init : 115200 bauds sur PA10 (broche Arduino D2) avec interruption RXNE
  */
static void MX_USART1_Init(void)
{
  __HAL_RCC_USART1_CLK_ENABLE();
  USART1->BRR = (48U << 4) | 13U;
  USART1->CR1 = USART_CR1_UE | USART_CR1_RE | USART_CR1_RXNEIE;

  HAL_NVIC_SetPriority(USART1_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(USART1_IRQn);
}

/**
  * @brief USART2 Init : 115200 bauds sur PA3 (broche Arduino D0) avec interruption RXNE
  */
static void MX_USART2_Init(void)
{
  __HAL_RCC_USART2_CLK_ENABLE();
  USART2->BRR = (24U << 4) | 7U;
  USART2->CR1 = USART_CR1_UE | USART_CR1_RE | USART_CR1_RXNEIE;

  HAL_NVIC_SetPriority(USART2_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(USART2_IRQn);
}
#endif /* LIDAR_ENABLED */


/**
  * @brief GPIO Init
  *   PA0      : ADC1 CH0 (vitesse)     - analogique
  *   PA1      : ADC1 CH1 (carburant)   - analogique
  *   PA3      : USART2 RX (LiDAR)      - AF7
  *   PA4      : DHT11 DATA             - input pull-up
  *   PA5      : LED LD2                - output push-pull
  *   PB8/PB9  : CAN1 RX/TX            - AF9
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();  /* HSE sur PH0/PH1 */

  /* PA5 : LED LD2 */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
  GPIO_InitStruct.Pin   = GPIO_PIN_5;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* PA4 : DHT11 (reconfigue en OD dans DHT11_Start) */
  GPIO_InitStruct.Pin  = DHT11_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(DHT11_PORT, &GPIO_InitStruct);

  /* PA0, PA1 : ADC analogique */
  GPIO_InitStruct.Pin  = GPIO_PIN_0 | GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* PA10 : USART1 RX (LiDAR sur broche Arduino D2 — ZÉRO conflit ST-LINK !) */
  GPIO_InitStruct.Pin       = GPIO_PIN_10;
  GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull      = GPIO_PULLUP;
  GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* PA3 : USART2 RX (LiDAR sur broche Arduino D0) */
  GPIO_InitStruct.Pin       = GPIO_PIN_3;
  GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull      = GPIO_PULLUP;
  GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* PB8 (CAN1 RX), PB9 (CAN1 TX) */
  GPIO_InitStruct.Pin       = GPIO_PIN_8 | GPIO_PIN_9;
  GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull      = GPIO_NOPULL;
  GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF9_CAN1;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

/* USER CODE BEGIN 4 */
/* USER CODE END 4 */

/**
  * @brief  Error Handler.
  * @retval None
  */
void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
    HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
    for (volatile uint32_t i = 0; i < 400000; i++) { /* blink rapide = erreur */ }
  }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  (void)file;
  (void)line;
}
#endif /* USE_FULL_ASSERT */
