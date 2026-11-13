/**
 * lwipopts.h — Configuration LwIP pour STM32H747I-DISCO ↔ Jetson Nano
 *
 * IP Statique : 192.168.1.10  (STM32H747)
 * Jetson IP   : 192.168.1.20
 * TX Port     : 5000  (H747 → Jetson : trame capteurs JSON)
 * RX Port     : 5001  (Jetson → H747 : alertes IA JSON)
 */
#ifndef __LWIPOPTS_H__
#define __LWIPOPTS_H__

/* Mode sans OS (FreeRTOS gère le polling via une tâche dédiée) */
#define NO_SYS                          1
#define SYS_LIGHTWEIGHT_PROT            0

/* ── Mémoire LwIP ─────────────────────────────────────────────────── */
#define MEM_ALIGNMENT                   4
#define MEM_SIZE                        (32 * 1024)   /* 32 Ko heap    */
#define MEMP_NUM_PBUF                   32
#define MEMP_NUM_UDP_PCB                4             /* TX + RX PCBs  */
#define MEMP_NUM_TCP_PCB                4
#define MEMP_NUM_TCP_PCB_LISTEN         2
#define MEMP_NUM_TCP_SEG                32
#define MEMP_NUM_SYS_TIMEOUT            10
#define PBUF_POOL_SIZE                  32
#define PBUF_POOL_BUFSIZE               1524

/* ── Protocoles activés ───────────────────────────────────────────── */
#define LWIP_ARP                        1
#define LWIP_ETHERNET                   1
#define LWIP_IPV4                       1
#define LWIP_UDP                        1             /* TX + RX       */
#define LWIP_TCP                        1             /* TCP activé pour flux Jetson */
#define TCP_MSS                         1460
#define TCP_WND                         (4 * TCP_MSS)
#define TCP_SND_BUF                     (4 * TCP_MSS)
#define TCP_QUEUE_OOSEQ                 0
#define LWIP_ICMP                       1             /* Ping activé   */
#define LWIP_DHCP                       0             /* IP statique   */
#define LWIP_DNS                        0
#define LWIP_SNMP                       0

/* Réception UDP activée — callbacks sur datagrammes entrants */
#define LWIP_CALLBACK_API               1

/* ── Checksum logiciel fiable STM32H7 (évite rejet des trames Linux/Jetson) ─ */
#define CHECKSUM_GEN_IP                 1
#define CHECKSUM_GEN_UDP                1
#define CHECKSUM_GEN_TCP                1
#define CHECKSUM_CHECK_IP               1
#define CHECKSUM_CHECK_UDP              1
#define CHECKSUM_CHECK_TCP              1
#define CHECKSUM_GEN_ICMP               1
#define CHECKSUM_CHECK_ICMP             1
#define LWIP_CHECKSUM_ON_COPY           0

/* ── Adresse MAC ──────────────────────────────────────────────────── */
#undef ETH_MAC_ADDR0
#undef ETH_MAC_ADDR1
#undef ETH_MAC_ADDR2
#undef ETH_MAC_ADDR3
#undef ETH_MAC_ADDR4
#undef ETH_MAC_ADDR5
#define ETH_MAC_ADDR0                   0x02
#define ETH_MAC_ADDR1                   0x00
#define ETH_MAC_ADDR2                   0x00
#define ETH_MAC_ADDR3                   0x00
#define ETH_MAC_ADDR4                   0x00
#define ETH_MAC_ADDR5                   0x0A   /* .0A = cohérent avec IP .10 */

/* ── API Réseau (désactivé en NO_SYS=1, on utilise les callbacks bruts) ─ */
#define LWIP_NETCONN                    0
#define LWIP_SOCKET                     0

/* ── Divers ───────────────────────────────────────────────────────── */
#define LWIP_NETIF_HOSTNAME             1
#define LWIP_NETIF_STATUS_CALLBACK      1
#define LWIP_NETIF_LINK_CALLBACK        1
#define LWIP_STATS                      0


#endif /* __LWIPOPTS_H__ */
