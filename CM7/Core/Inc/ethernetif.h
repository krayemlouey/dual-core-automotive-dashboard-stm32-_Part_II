/**
 * ethernetif.h — Interface Ethernet LwIP pour STM32H747I-DISCO (LAN8742A, RMII)
 */
#ifndef __ETHERNETIF_H__
#define __ETHERNETIF_H__

#include "lwip/netif.h"

err_t ethernetif_init(struct netif *netif);
void  ethernetif_input(struct netif *netif);
void  ethernet_link_check_state(struct netif *netif);

#endif /* __ETHERNETIF_H__ */
