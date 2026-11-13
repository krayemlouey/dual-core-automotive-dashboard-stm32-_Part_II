/**
 * udp_sender.h — Communication UDP bidirectionnelle STM32H747 ↔ Jetson Nano
 *
 * ENVOI  (H747 → Jetson)  PORT 5000
 *   Trame JSON : {"speed":120.5,"temp":23.1,"fuel":75.0,"lidar":45,"gear":3}
 *
 * RECEPTION (Jetson → H747)  PORT 5001
 *   Trame JSON : {"level":1,"msg":"OBSTACLE DETECTE"}
 *   level : 0=OK, 1=WARNING, 2=DANGER
 *
 * Config réseau :
 *   STM32H747 IP  : 192.168.1.10
 *   Jetson Nano IP: 192.168.1.20
 */
#ifndef __UDP_SENDER_H__
#define __UDP_SENDER_H__

#include "lwip/netif.h"
#include "lwip/udp.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Adresse IP du STM32H747 (interface Ethernet) ─────────────────── */
#define STM32_IP0   192
#define STM32_IP1   168
#define STM32_IP2     1
#define STM32_IP3    50     /* 192.168.1.50 */

/* ── Adresse IP de la Jetson Nano ──────────────────────────────────── */
#define JETSON_IP0  192
#define JETSON_IP1  168
#define JETSON_IP2    1
#define JETSON_IP3   10     /* 192.168.1.10 */

/* ── Ports UDP ─────────────────────────────────────────────────────── */
#define UDP_TX_PORT  5000   /* H747 → Jetson : données capteurs          */
#define UDP_RX_PORT  5001   /* Jetson → H747 : alertes IA                */

/* ── Taille max du message d'alerte (doit correspondre à SharedData_t) */
#define ALERT_MSG_MAX  64

/**
 * @brief Initialise LwIP, l'interface ETH et les sockets UDP TX/RX
 *        À appeler UNE seule fois avant la boucle principale.
 * @return 0 si succès, -1 si erreur
 */
int UDP_Comms_Init(void);

/**
 * @brief Envoie la trame JSON des capteurs vers la Jetson Nano
 * @param speed       Vitesse (km/h)
 * @param temperature Température (°C)
 * @param fuel        Carburant (%)
 * @param lidar_cm    Distance LiDAR (cm), -1 si invalide
 * @param gear        Rapport de boîte (0=N, 1-6 = vitesses)
 */
void UDP_SendTelemetry(float speed, float temperature, float fuel,
                       float lidar_cm, uint8_t gear);

/**
 * @brief Polling LwIP — à appeler dans la boucle principale (≥ 1 fois/ms)
 *        Traite les paquets UDP entrants (alertes Jetson) et écrit
 *        le message dans SRAM4 via les pointeurs SharedData.
 */
void UDP_Poll(void);

/**
 * @brief Retourne le pointeur vers l'interface réseau LwIP (pour debug)
 */
struct netif *udp_get_netif(void);

#ifdef __cplusplus
}
#endif

#endif /* __UDP_SENDER_H__ */
