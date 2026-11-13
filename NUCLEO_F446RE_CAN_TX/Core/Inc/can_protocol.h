#ifndef CAN_PROTOCOL_H
#define CAN_PROTOCOL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Identifiants des trames CAN (Standard 11-bit) ── */
#define CAN_ID_POT_DATA       0x100U   /* Trame TPS émise par le F446RE */
#define CAN_ID_CMD_FREQ       0x200U   /* Commande reçue du H747 pour adapter la fréquence */
#define CAN_DLC_POT           4U       /* 4 octets */

/* ── Structure trame émise (Payload 4 octets) ── */
typedef struct __attribute__((packed))
{
  uint16_t pot_value;    /* Valeur ADC 12-bit (0 à 4095) */
  uint8_t  frame_index;  /* Compteur séquentiel (détection perte de trame E2E) */
  uint8_t  checksum;     /* Contrôle d'intégrité applicatif (XOR) */
} CAN_PotPacket_t;

/* ── Classes TinyML (décidées par le H747) ── */
#define ML_CLASS_SLOW_MOVE   0U   /* Rotation normale/lente */
#define ML_CLASS_FAST_MOVE   1U   /* Rotation rapide */
#define ML_CLASS_EMI_NOISE   2U   /* Bruit électromagnétique détecté */
#define ML_CLASS_FAILURE     3U   /* Défaillance capteur (Fail-Safe) */

/* Macro de calcul du checksum */
#define CAN_COMPUTE_CHECKSUM(hi, lo, idx)  ((uint8_t)((hi) ^ (lo) ^ (idx)))

#ifdef __cplusplus
}
#endif

#endif /* CAN_PROTOCOL_H */
