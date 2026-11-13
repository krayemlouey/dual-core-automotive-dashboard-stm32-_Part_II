/**
 * udp_sender.c — Communication UDP bidirectionnelle STM32H747 ↔ Jetson Nano
 *
 * TX (H747 → Jetson, port 5000) :
 *   JSON : {"speed":120.5,"temp":23.1,"fuel":75.0,"lidar":45,"gear":3}
 *
 * RX (Jetson → H747, port 5001) :
 *   JSON : {"level":1,"msg":"OBSTACLE DETECTE"}
 *   Le message est copié dans sharedData->alert_text (SRAM4 0x38000020).
 *   Le CM7 TouchGFX lit ce champ dans Model::tick() et l'affiche sur le dashboard.
 *
 * Architecture LwIP NO_SYS :
 *   - Pas d'OS LwIP : on appelle sys_check_timeouts() + ethernetif_input() dans UDP_Poll()
 *   - UDP_Poll() est appelé dans la tâche FreeRTOS « EthernetTask » (dans freertos.c)
 */

#include "udp_sender.h"
#include "ethernetif.h"
#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/ip_addr.h"
#include "lwip/udp.h"
#include "lwip/pbuf.h"
#include "lwip/timeouts.h"
#include "netif/ethernet.h"
#include "stm32h7xx_hal.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

/* ── Pointeur vers SharedData_t en SRAM4 (doit correspondre à CM4/main.c) ── */
/* Layout SRAM4 0x38000000 :
 *   +0x00 : float speed
 *   +0x04 : float temperature
 *   +0x08 : float fuel
 *   +0x0C : float gps_lat
 *   +0x10 : float gps_lon
 *   +0x14 : uint8_t gps_updated
 *   +0x15 : uint8_t demo_mode
 *   +0x16 : uint8_t padding[2]
 *   +0x18 : float lidar_cm
 *   +0x1C : uint32_t can_rx_count
 *   +0x20 : char alert_text[64]    ← NOUVEAU (alertes Jetson)
 *   +0x60 : uint8_t alert_level    ← NOUVEAU (0=OK,1=WARN,2=DANGER)
 *   +0x61 : uint8_t ethernet_ok    ← NOUVEAU (1 si lien Ethernet actif)
 */
#define SRAM4_BASE             0x38000000UL
#define SRAM4_ALERT_TEXT       (SRAM4_BASE + 0x20UL)   /* char[64] */
#define SRAM4_ALERT_LEVEL      (SRAM4_BASE + 0x60UL)   /* uint8_t  */
#define SRAM4_ETHERNET_OK      (SRAM4_BASE + 0x61UL)   /* uint8_t  */
#define SRAM4_ALERT_TARGET     (SRAM4_BASE + 0x62UL)   /* uint8_t (0=speed, 1=temp, 2=fuel) */

/* ── Interface réseau LwIP ─────────────────────────────────────────── */
static struct netif gnetif;

/* ── PCBs UDP ──────────────────────────────────────────────────────── */
static struct udp_pcb *udp_tx_pcb = NULL;   /* TX → Jetson port 5000  */
static struct udp_pcb *udp_rx_pcb = NULL;   /* RX ← Jetson port 5001  */

static ip_addr_t jetson_ip;

/* ──────────────────────────────────────────────────────────────────
   CALLBACK : réception d'une alerte depuis la Jetson Nano
   ────────────────────────────────────────────────────────────────── */
static void udp_rx_callback(void *arg, struct udp_pcb *pcb,
                             struct pbuf *p, const ip_addr_t *addr, u16_t port)
{
    (void)arg; (void)pcb; (void)addr; (void)port;

    if (p == NULL) return;

    /* Copier le message dans un buffer temporaire */
    char raw[96];
    uint16_t copy_len = (p->tot_len < sizeof(raw) - 1) ? p->tot_len : sizeof(raw) - 1;
    pbuf_copy_partial(p, raw, copy_len, 0);
    raw[copy_len] = '\0';
    pbuf_free(p);

    /* Parser JSON minimal : chercher "msg":"..." et "level":X  */
    char alert_msg[64] = {0};
    uint8_t alert_lvl  = 0;

    /* Extraction du niveau numérique éventuel */
    char *lvl_ptr = strstr(raw, "\"level\":");
    if (lvl_ptr) {
        lvl_ptr += 8;
        while (*lvl_ptr == ' ') lvl_ptr++;
        alert_lvl = (uint8_t)(*lvl_ptr - '0');
        if (alert_lvl > 2) alert_lvl = 2;
    }

    /* Extraction du statut texte : "critical", "warning", "ok" */
    char *status_ptr = strstr(raw, "\"status\":\"");
    if (status_ptr) {
        status_ptr += 10; /* sauter "status":" */
        if (strncmp(status_ptr, "critical", 8) == 0 || strncmp(status_ptr, "critic", 6) == 0) {
            alert_lvl = 2; /* CRITICAL -> LED rouge clignotante */
        } else if (strncmp(status_ptr, "warning", 7) == 0) {
            if (alert_lvl < 1) alert_lvl = 1;
        } else if (strncmp(status_ptr, "ok", 2) == 0) {
            alert_lvl = 0;
        }
    }

    /* Extraction du message texte */
    char *msg_ptr = strstr(raw, "\"msg\":\"");
    if (msg_ptr) {
        msg_ptr += 7;  /* sauter "msg":" */
        int i = 0;
        while (*msg_ptr && *msg_ptr != '"' && i < 63) {
            alert_msg[i++] = *msg_ptr++;
        }
        alert_msg[i] = '\0';
    } else {
        /* Pas de champ "msg" -> utiliser le texte brut si pas vide */
        strncpy(alert_msg, raw, 63);
        alert_msg[63] = '\0';
    }

    /* Extraction de la cible de l'alerte : "temp", "fuel", "speed" (0=speed, 1=temp, 2=fuel) */
    uint8_t alert_target = 0;
    char *tgt_ptr = strstr(raw, "\"target\":\"");
    if (tgt_ptr) {
        tgt_ptr += 10;
        if (strncmp(tgt_ptr, "temp", 4) == 0) alert_target = 1;
        else if (strncmp(tgt_ptr, "fuel", 4) == 0) alert_target = 2;
        else alert_target = 0;
    } else {
        /* Auto-détection par mots-clés dans le message */
        if (strstr(alert_msg, "TEMP") || strstr(alert_msg, "temp") ||
            strstr(alert_msg, "CHAUF") || strstr(alert_msg, "chauf")) {
            alert_target = 1;
        } else if (strstr(alert_msg, "FUEL") || strstr(alert_msg, "fuel") ||
                   strstr(alert_msg, "CARBUR") || strstr(alert_msg, "carbur") ||
                   strstr(alert_msg, "ESSENCE") || strstr(alert_msg, "essence")) {
            alert_target = 2;
        } else {
            alert_target = 0; /* Vitesse / obstacle par défaut */
        }
    }

    /* Écrire dans SRAM4 (lu par Model::tick() du CM7) */
    volatile char    *sram_text   = (volatile char*)   SRAM4_ALERT_TEXT;
    volatile uint8_t *sram_level  = (volatile uint8_t*)SRAM4_ALERT_LEVEL;
    volatile uint8_t *sram_target = (volatile uint8_t*)SRAM4_ALERT_TARGET;

    for (int i = 0; i < 64; i++) sram_text[i] = alert_msg[i];
    *sram_level  = alert_lvl;
    *sram_target = alert_target;
}

/* ──────────────────────────────────────────────────────────────────
   ACCESSEUR : retourne le pointeur vers gnetif (pour logs externes)
   ────────────────────────────────────────────────────────────────── */
struct netif *udp_get_netif(void) { return &gnetif; }

/* ──────────────────────────────────────────────────────────────────
   INITIALISATION : LwIP + netif + sockets UDP
   ────────────────────────────────────────────────────────────────── */
int UDP_Comms_Init(void)
{
    ip4_addr_t ipaddr, netmask, gw;

    printf("[ETH] lwip_init()...\r\n");
    lwip_init();

    /* IP statique STM32 : 192.168.1.50 */
    IP4_ADDR(&ipaddr,  STM32_IP0, STM32_IP1, STM32_IP2, STM32_IP3);
    IP4_ADDR(&netmask, 255, 255, 255, 0);
    IP4_ADDR(&gw,      192, 168, 1, 1);

    printf("[ETH] netif_add() IP=%d.%d.%d.%d...\r\n",
           STM32_IP0, STM32_IP1, STM32_IP2, STM32_IP3);

    /* Ajouter l'interface Ethernet (ethernetif.c / LAN8742A) */
    struct netif *added = netif_add(&gnetif, &ipaddr, &netmask, &gw, NULL,
                                    &ethernetif_init, &ethernet_input);
    if (added == NULL) {
        printf("[ETH] ERREUR: netif_add() a retourne NULL !\r\n");
        return -1;
    }
    netif_set_default(&gnetif);
    netif_set_up(&gnetif);
    printf("[ETH] netif OK. MAC=%02X:%02X:%02X:%02X:%02X:%02X\r\n",
           gnetif.hwaddr[0], gnetif.hwaddr[1], gnetif.hwaddr[2],
           gnetif.hwaddr[3], gnetif.hwaddr[4], gnetif.hwaddr[5]);

    /* IP de la Jetson Nano */
    IP4_ADDR(&jetson_ip, JETSON_IP0, JETSON_IP1, JETSON_IP2, JETSON_IP3);

    /* ── Socket TX (H747 → Jetson) ─────────────────────────────── */
    udp_tx_pcb = udp_new();
    if (udp_tx_pcb == NULL) { printf("[ETH] ERREUR: udp_new() TX NULL\r\n"); return -1; }
    if (udp_bind(udp_tx_pcb, IP_ADDR_ANY, 0) != ERR_OK) {
        printf("[ETH] ERREUR: udp_bind() TX\r\n");
        udp_remove(udp_tx_pcb); udp_tx_pcb = NULL; return -1;
    }

    /* ── Socket RX (Jetson → H747 alertes) ────────────────────── */
    udp_rx_pcb = udp_new();
    if (udp_rx_pcb == NULL) { printf("[ETH] ERREUR: udp_new() RX NULL\r\n"); return -1; }
    if (udp_bind(udp_rx_pcb, IP_ADDR_ANY, UDP_RX_PORT) != ERR_OK) {
        printf("[ETH] ERREUR: udp_bind() RX\r\n");
        udp_remove(udp_rx_pcb); udp_rx_pcb = NULL; return -1;
    }
    udp_recv(udp_rx_pcb, udp_rx_callback, NULL);

    /* Marquer Ethernet OK dans SRAM4 */
    *(volatile uint8_t*)SRAM4_ETHERNET_OK = 1;
    printf("[ETH] Init complete. En attente lien PHY...\r\n");

    return 0;
}

/* ──────────────────────────────────────────────────────────────────
   ENVOI : trame capteurs → Jetson Nano
   ────────────────────────────────────────────────────────────────── */
void UDP_SendTelemetry(float speed, float temperature, float fuel,
                       float lidar_cm, uint8_t gear)
{
    if (udp_tx_pcb == NULL) return;

    char json_buf[128];
    int  len;

    int spd_i = (int)speed;
    int spd_d = (int)((speed > spd_i ? speed - spd_i : spd_i - speed) * 10.0f);
    int tmp_i = (int)temperature;
    int tmp_d = (int)((temperature > tmp_i ? temperature - tmp_i : tmp_i - temperature) * 10.0f);
    int ful_i = (int)fuel;
    int ful_d = (int)((fuel > ful_i ? fuel - ful_i : ful_i - fuel) * 10.0f);

    if (lidar_cm < 0.0f) {
        len = snprintf(json_buf, sizeof(json_buf),
            "{\"speed\":%d.%d,\"temp\":%d.%d,\"fuel\":%d.%d,"
            "\"lidar\":null,\"gear\":%u}",
            spd_i, spd_d, tmp_i, tmp_d, ful_i, ful_d, (unsigned)gear);
    } else {
        len = snprintf(json_buf, sizeof(json_buf),
            "{\"speed\":%d.%d,\"temp\":%d.%d,\"fuel\":%d.%d,"
            "\"lidar\":%d,\"gear\":%u}",
            spd_i, spd_d, tmp_i, tmp_d, ful_i, ful_d, (int)lidar_cm, (unsigned)gear);
    }

    if (len <= 0) return;

    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, (u16_t)len, PBUF_RAM);
    if (p == NULL) return;

    memcpy(p->payload, json_buf, len);
    udp_sendto(udp_tx_pcb, p, &jetson_ip, UDP_TX_PORT);
    pbuf_free(p);
}

/* ──────────────────────────────────────────────────────────────────
   POLLING : à appeler dans la boucle / tâche FreeRTOS
   Traite les paquets entrants et met à jour les timers LwIP
   ────────────────────────────────────────────────────────────────── */
void UDP_Poll(void)
{
    static uint32_t last_link_check = 0;
    static bool last_link_state = false;
    uint32_t now = HAL_GetTick();

    /* 1. Vérifier périodiquement l'état du câble / PHY LAN8742A (toutes les 250 ms) */
    if ((now - last_link_check) >= 250) {
        last_link_check = now;
        ethernet_link_check_state(&gnetif);

        bool is_up = netif_is_up(&gnetif) && netif_is_link_up(&gnetif);
        if (is_up != last_link_state) {
            last_link_state = is_up;
            if (is_up) {
                /* Allumer la LED verte PJ12 (demandée) et PI12 (LED verte de la carte) */
                HAL_GPIO_WritePin(GPIOJ, GPIO_PIN_12, GPIO_PIN_SET);
                HAL_GPIO_WritePin(GPIOI, GPIO_PIN_12, GPIO_PIN_SET);
                printf("[ETH] >>> LIEN ETHERNET UP ! IP: 192.168.1.50 (Pret pour Ping, ARP, TCP) <<<\r\n");
            } else {
                HAL_GPIO_WritePin(GPIOI, GPIO_PIN_12, GPIO_PIN_RESET);
                printf("[ETH] >>> LIEN ETHERNET DOWN <<<\r\n");
            }
        }
    }

    /* 2. Traiter les paquets Ethernet reçus (DMA -> LwIP) */
    ethernetif_input(&gnetif);

    /* 3. Gérer les timers LwIP (ARP refresh, TCP timers, retransmissions) */
    sys_check_timeouts();
}

