/**
 * ethernetif.c — Driver Ethernet LwIP adapté pour STM32H747I-DISCO
 * PHY : LAN8742A en mode RMII
 * Basé sur l'exemple officiel STM32Cube_FW_H7
 */

#include "stm32h7xx_hal.h"
#include "lwip/opt.h"
#include "lwip/timeouts.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "netif/etharp.h"
#include "ethernetif.h"
#include "lan8742.h"
#include <string.h>
#include <stdio.h>

/* Nom de l'interface réseau */
#define IFNAME0 's'
#define IFNAME1 't'

#define ETH_DMA_TRANSMIT_TIMEOUT   20U
#define ETH_RX_BUFFER_SIZE         1000U
#define ETH_RX_BUFFER_CNT          12U

typedef enum { RX_ALLOC_OK = 0, RX_ALLOC_ERROR = 1 } RxAllocStatusTypeDef;

typedef struct {
    struct pbuf_custom pbuf_custom;
    uint8_t buff[(ETH_RX_BUFFER_SIZE + 31) & ~31] __ALIGNED(32);
} RxBuff_t;

/* DMA Descriptors en SRAM AXI (section spéciale pour STM32H7) */
ETH_DMADescTypeDef DMARxDscrTab[ETH_RX_DESC_CNT] __attribute__((section(".RxDescripSection")));
ETH_DMADescTypeDef DMATxDscrTab[ETH_TX_DESC_CNT] __attribute__((section(".TxDescripSection")));

LWIP_MEMPOOL_DECLARE(RX_POOL, ETH_RX_BUFFER_CNT, sizeof(RxBuff_t), "Zero-copy RX PBUF pool");
__attribute__((section(".Rx_PoolSection"))) extern u8_t memp_memory_RX_POOL_base[];

static RxAllocStatusTypeDef RxAllocStatus;
ETH_HandleTypeDef EthHandle;
ETH_TxPacketConfig TxConfig;

/* Fonctions PHY */
int32_t ETH_PHY_IO_Init(void);
int32_t ETH_PHY_IO_DeInit(void);
int32_t ETH_PHY_IO_ReadReg(uint32_t DevAddr, uint32_t RegAddr, uint32_t *pRegVal);
int32_t ETH_PHY_IO_WriteReg(uint32_t DevAddr, uint32_t RegAddr, uint32_t RegVal);
int32_t ETH_PHY_IO_GetTick(void);

lan8742_Object_t LAN8742;
lan8742_IOCtx_t  LAN8742_IOCtx = {
    ETH_PHY_IO_Init,
    ETH_PHY_IO_DeInit,
    ETH_PHY_IO_WriteReg,
    ETH_PHY_IO_ReadReg,
    ETH_PHY_IO_GetTick
};

void pbuf_free_custom(struct pbuf *p);

/* ============================================================
   Initialisation hardware bas niveau
   ============================================================ */
static void low_level_init(struct netif *netif)
{
    uint8_t macaddress[6] = {ETH_MAC_ADDR0, ETH_MAC_ADDR1, ETH_MAC_ADDR2,
                              ETH_MAC_ADDR3, ETH_MAC_ADDR4, ETH_MAC_ADDR5};

    EthHandle.Instance          = ETH;
    EthHandle.Init.MACAddr      = macaddress;
    EthHandle.Init.MediaInterface = HAL_ETH_RMII_MODE;
    EthHandle.Init.RxDesc       = DMARxDscrTab;
    EthHandle.Init.TxDesc       = DMATxDscrTab;
    EthHandle.Init.RxBuffLen    = ETH_RX_BUFFER_SIZE;

    printf("[ETH] HAL_ETH_Init()...\r\n");
    HAL_StatusTypeDef eth_status = HAL_ETH_Init(&EthHandle);
    printf("[ETH] HAL_ETH_Init = %s (Desc RX=0x%08lX TX=0x%08lX)\r\n",
           (eth_status == HAL_OK) ? "OK" : "ERREUR",
           (unsigned long)DMARxDscrTab,
           (unsigned long)DMATxDscrTab);

    netif->hwaddr_len = ETH_HWADDR_LEN;
    for (int i = 0; i < 6; i++) netif->hwaddr[i] = macaddress[i];
    netif->mtu    = ETH_MAX_PAYLOAD;
    netif->flags |= NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP;

    LWIP_MEMPOOL_INIT(RX_POOL);

    memset(&TxConfig, 0, sizeof(ETH_TxPacketConfig));
    TxConfig.Attributes    = ETH_TX_PACKETS_FEATURES_CSUM | ETH_TX_PACKETS_FEATURES_CRCPAD;
    TxConfig.ChecksumCtrl  = ETH_CHECKSUM_IPHDR_PAYLOAD_INSERT_PHDR_CALC;
    TxConfig.CRCPadCtrl    = ETH_CRC_PAD_INSERT;

    LAN8742_RegisterBusIO(&LAN8742, &LAN8742_IOCtx);
    printf("[ETH] LAN8742_Init()...\r\n");
    int32_t phy_status = LAN8742_Init(&LAN8742);
    printf("[ETH] LAN8742_Init = %s (code=%ld, Addr=%lu)\r\n",
           (phy_status == LAN8742_STATUS_OK) ? "OK" : "ERREUR",
           (long)phy_status,
           (unsigned long)LAN8742.DevAddr);

    if (phy_status != LAN8742_STATUS_OK) {
        printf("[ETH] PHY non detecte ! Verifiez cable et adresse PHY.\r\n");
        netif_set_link_down(netif);
        netif_set_down(netif);
        return;
    }
    ethernet_link_check_state(netif);
    printf("[ETH] low_level_init termine. Lien=%s\r\n",
           netif_is_link_up(netif) ? "UP" : "DOWN (cable debranche ?)");
}

/* ============================================================
   Envoi de paquets
   ============================================================ */
static err_t low_level_output(struct netif *netif, struct pbuf *p)
{
    static uint32_t tx_count = 0;
    tx_count++;
    if (tx_count <= 5 || (tx_count % 25 == 0)) {
        printf("[ETH TX #%lu] trame tot_len=%u (gState=%d)\r\n", 
               (unsigned long)tx_count, (unsigned)p->tot_len, (int)EthHandle.gState);
    }

    ETH_BufferTypeDef Txbuffer[ETH_TX_DESC_CNT] = {0};
    uint32_t i = 0;
    for (struct pbuf *q = p; q != NULL; q = q->next) {
        if (i >= ETH_TX_DESC_CNT) return ERR_IF;
        Txbuffer[i].buffer = q->payload;
        Txbuffer[i].len    = q->len;
        if (i > 0) Txbuffer[i-1].next = &Txbuffer[i];
        if (q->next == NULL) Txbuffer[i].next = NULL;
        /* Vider le cache de donnees pour que le DMA envoie les donnees physiques a jour */
        SCB_CleanDCache_by_Addr((uint32_t *)q->payload, q->len);
        i++;
    }
    TxConfig.Length  = p->tot_len;
    TxConfig.TxBuffer = Txbuffer;
    TxConfig.pData   = p;
    HAL_StatusTypeDef tx_st = HAL_ETH_Transmit(&EthHandle, &TxConfig, ETH_DMA_TRANSMIT_TIMEOUT);
    if (tx_st != HAL_OK) {
        printf("[ETH TX ERR] code=%d EthErr=0x%08lX gState=%d\r\n",
               (int)tx_st, (unsigned long)EthHandle.ErrorCode, (int)EthHandle.gState);
    }
    return ERR_OK;
}

/* ============================================================
   Réception de paquets
   ============================================================ */
static struct pbuf *low_level_input(struct netif *netif)
{
    struct pbuf *p = NULL;
    if (RxAllocStatus == RX_ALLOC_OK) {
        HAL_StatusTypeDef rx_st = HAL_ETH_ReadData(&EthHandle, (void **)&p);
        if (rx_st == HAL_OK && p != NULL) {
            printf("[ETH RX] Paquet recu ! len=%u\r\n", (unsigned)p->tot_len);
        }
    }
    return p;
}

void ethernetif_input(struct netif *netif)
{
    struct pbuf *p;
    do {
        p = low_level_input(netif);
        if (p != NULL) {
            if (netif->input(p, netif) != ERR_OK) pbuf_free(p);
        }
    } while (p != NULL);
}

/* ============================================================
   Initialisation de l'interface LwIP
   ============================================================ */
err_t ethernetif_init(struct netif *netif)
{
    LWIP_ASSERT("netif != NULL", (netif != NULL));
#if LWIP_NETIF_HOSTNAME
    netif->hostname = "stm32h747";
#endif
    netif->name[0] = IFNAME0;
    netif->name[1] = IFNAME1;
    netif->output     = etharp_output;
    netif->linkoutput = low_level_output;
    low_level_init(netif);
    return ERR_OK;
}

/* ============================================================
   Utilitaires
   ============================================================ */
u32_t sys_now(void) { return HAL_GetTick(); }

void pbuf_free_custom(struct pbuf *p)
{
    struct pbuf_custom *custom_pbuf = (struct pbuf_custom *)p;
    LWIP_MEMPOOL_FREE(RX_POOL, custom_pbuf);
    if (RxAllocStatus == RX_ALLOC_ERROR) RxAllocStatus = RX_ALLOC_OK;
}

/* ============================================================
   MSP : GPIO + Horloges Ethernet (STM32H747I-DISCO, RMII)
   ============================================================ */
void HAL_ETH_MspInit(ETH_HandleTypeDef *heth)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();

    /*
     * STM32H747I-DISCO RMII pinout (UM2411 Official Pinout) :
     *   PA1  → RMII_REF_CLK
     *   PA2  → RMII_MDIO
     *   PA7  → RMII_CRS_DV
     *   PC1  → RMII_MDC
     *   PC4  → RMII_RXD0
     *   PC5  → RMII_RXD1
     *   PG11 → RMII_TX_EN
     *   PG12 → RMII_TXD1 (sur GPIOG, et NON PB13 !)
     *   PG13 → RMII_TXD0
     */
    GPIO_InitStructure.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStructure.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStructure.Pull      = GPIO_NOPULL;
    GPIO_InitStructure.Alternate = GPIO_AF11_ETH;

    GPIO_InitStructure.Pin = GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_7;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.Pin = GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStructure);

    GPIO_InitStructure.Pin = GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStructure);

    __HAL_RCC_ETH1MAC_CLK_ENABLE();
    __HAL_RCC_ETH1TX_CLK_ENABLE();
    __HAL_RCC_ETH1RX_CLK_ENABLE();
}

/* ============================================================
   PHY : LAN8742A via MDIO
   ============================================================ */
int32_t ETH_PHY_IO_Init(void)     { HAL_ETH_SetMDIOClockRange(&EthHandle); return 0; }
int32_t ETH_PHY_IO_DeInit(void)   { return 0; }
int32_t ETH_PHY_IO_GetTick(void)  { return HAL_GetTick(); }

int32_t ETH_PHY_IO_ReadReg(uint32_t DevAddr, uint32_t RegAddr, uint32_t *pRegVal)
{
    return (HAL_ETH_ReadPHYRegister(&EthHandle, DevAddr, RegAddr, pRegVal) == HAL_OK) ? 0 : -1;
}

int32_t ETH_PHY_IO_WriteReg(uint32_t DevAddr, uint32_t RegAddr, uint32_t RegVal)
{
    return (HAL_ETH_WritePHYRegister(&EthHandle, DevAddr, RegAddr, RegVal) == HAL_OK) ? 0 : -1;
}

/* ============================================================
   Surveillance du lien Ethernet
   ============================================================ */
void ethernet_link_check_state(struct netif *netif)
{
    ETH_MACConfigTypeDef MACConf = {0};
    int32_t PHYLinkState = LAN8742_GetLinkState(&LAN8742);
    uint32_t linkchanged = 0, speed = 0, duplex = 0;

    if (netif_is_link_up(netif) && (PHYLinkState <= LAN8742_STATUS_LINK_DOWN)) {
        HAL_ETH_Stop(&EthHandle);
        netif_set_down(netif);
        netif_set_link_down(netif);
    } else if (!netif_is_link_up(netif) && (PHYLinkState > LAN8742_STATUS_LINK_DOWN)) {
        switch (PHYLinkState) {
            case LAN8742_STATUS_100MBITS_FULLDUPLEX: duplex = ETH_FULLDUPLEX_MODE; speed = ETH_SPEED_100M; linkchanged = 1; break;
            case LAN8742_STATUS_100MBITS_HALFDUPLEX: duplex = ETH_HALFDUPLEX_MODE; speed = ETH_SPEED_100M; linkchanged = 1; break;
            case LAN8742_STATUS_10MBITS_FULLDUPLEX:  duplex = ETH_FULLDUPLEX_MODE; speed = ETH_SPEED_10M;  linkchanged = 1; break;
            case LAN8742_STATUS_10MBITS_HALFDUPLEX:  duplex = ETH_HALFDUPLEX_MODE; speed = ETH_SPEED_10M;  linkchanged = 1; break;
            default: break;
        }
        if (linkchanged) {
            HAL_ETH_GetMACConfig(&EthHandle, &MACConf);
            MACConf.DuplexMode = duplex;
            MACConf.Speed = speed;
            HAL_ETH_SetMACConfig(&EthHandle, &MACConf);
            HAL_ETH_Start(&EthHandle);
            netif_set_up(netif);
            netif_set_link_up(netif);
        }
    }

    if (netif_is_link_up(netif) && EthHandle.gState != HAL_ETH_STATE_STARTED) {
        printf("[ETH] Relance automatique ETH DMA (gState=%d)...\r\n", (int)EthHandle.gState);
        HAL_ETH_Start(&EthHandle);
    }
}

/* ============================================================
   Callbacks DMA
   ============================================================ */
void HAL_ETH_RxAllocateCallback(uint8_t **buff)
{
    struct pbuf_custom *p = LWIP_MEMPOOL_ALLOC(RX_POOL);
    if (p) {
        *buff = (uint8_t *)p + offsetof(RxBuff_t, buff);
        p->custom_free_function = pbuf_free_custom;
        pbuf_alloced_custom(PBUF_RAW, 0, PBUF_REF, p, *buff, ETH_RX_BUFFER_SIZE);
    } else {
        RxAllocStatus = RX_ALLOC_ERROR;
        *buff = NULL;
    }
}

void HAL_ETH_RxLinkCallback(void **pStart, void **pEnd, uint8_t *buff, uint16_t Length)
{
    struct pbuf **ppStart = (struct pbuf **)pStart;
    struct pbuf **ppEnd   = (struct pbuf **)pEnd;
    struct pbuf *p = (struct pbuf *)(buff - offsetof(RxBuff_t, buff));
    p->next = NULL; p->tot_len = 0; p->len = Length;
    if (!*ppStart) *ppStart = p; else (*ppEnd)->next = p;
    *ppEnd = p;
    for (p = *ppStart; p != NULL; p = p->next) p->tot_len += Length;
    SCB_InvalidateDCache_by_Addr((uint32_t *)buff, Length);
}

void HAL_ETH_TxFreeCallback(uint32_t *buff) { pbuf_free((struct pbuf *)buff); }
