/**
  ******************************************************************************
  * @file    stream_receiver.h
  * @brief   Header du recepteur de flux video TCP Jetson Nano -> STM32H747.
  *          Detection de cigarette par IA (YOLOv8), affichage video integre
  *          dans l'interface TouchGFX existante (tableau de bord automobile).
  *
  *  IMPORTANT : PJ12 = LCD_BL_Pin (retro-eclairage LCD) sur STM32H747I-DISCO.
  *  Ne jamais reconfigurer PJ12 comme GPIO Output — cela eteint l'ecran !
  *  Les LEDs physiques sont SUPPRIMEES de ce module pour eviter tout conflit.
  ******************************************************************************
  */

#ifndef __STREAM_RECEIVER_H__
#define __STREAM_RECEIVER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h7xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/* --- 1. CONFIGURATION RESEAU ET PORTS ------------------------------------ */
#define STREAM_SERVER_PORT         5000           /* Port d'ecoute TCP STM32 */
#define STM32_IP_ADDR0             192
#define STM32_IP_ADDR1             168
#define STM32_IP_ADDR2             1
#define STM32_IP_ADDR3             50             /* 192.168.1.50 */

#define STM32_NETMASK0             255
#define STM32_NETMASK1             255
#define STM32_NETMASK2             255
#define STM32_NETMASK3             0              /* 255.255.255.0 */

#define STM32_GATEWAY0             192
#define STM32_GATEWAY1             168
#define STM32_GATEWAY2             1
#define STM32_GATEWAY3             1              /* 192.168.1.1 */

/* --- 2. PARAMETRES VIDEO ET AFFICHAGE ------------------------------------ */
#define LCD_WIDTH                  800
#define LCD_HEIGHT                 480
#define VIDEO_WIDTH                320
#define VIDEO_HEIGHT               240

/* Coordonnees de centrage de l'image video 320x240 sur l'ecran 800x480 */
#define VIDEO_POS_X                ((LCD_WIDTH  - VIDEO_WIDTH)  / 2)   /* 240 */
#define VIDEO_POS_Y                ((LCD_HEIGHT - VIDEO_HEIGHT) / 2)   /* 120 */

/* --- 3. PROTOCOLE TRAME JETSON NANO (15 octets) -------------------------- */
#define STREAM_HEADER_SIZE         15
#define STREAM_MAGIC_WORD          0x4A455400UL   /* "JET\0" en Big-Endian */

/* Types de message */
#define MSG_TYPE_IMAGE             0x01
#define MSG_TYPE_HEARTBEAT         0x02
#define MSG_TYPE_ALERT_ON          0x03
#define MSG_TYPE_ALERT_OFF         0x04

/* Taille maximale autorisee pour une trame JPEG 320x240 */
#define MAX_JPEG_PAYLOAD_SIZE      (64 * 1024)    /* 64 Ko max */

/* --- 4. STRUCTURE DE LA TRAME D'EN-TETE ---------------------------------- */
#pragma pack(push, 1)
typedef struct {
    uint32_t magic;          /* 0x4A455400 ("JET\0") */
    uint8_t  msg_type;       /* 0x01=Image, 0x02=Heartbeat */
    uint16_t img_width;      /* 320 */
    uint16_t img_height;     /* 240 */
    uint32_t jpeg_size;      /* Taille N du payload JPEG */
    uint8_t  alert_flag;     /* 1=Cigarette detectee, 0=Pas d'alerte */
    uint8_t  confidence;     /* 0..255 */
} StreamHeader_t;
#pragma pack(pop)

/* --- 5. ETAT DU SYSTEME -------------------------------------------------- */
typedef struct {
    bool     connected;          /* true si client Jetson connecte */
    uint32_t last_packet_tick;   /* Horodatage HAL du dernier paquet valide */
    uint8_t  alert_flag;         /* 1 si cigarette detectee */
    uint8_t  confidence;         /* Niveau de confiance (0..255) */
    float    fps;                /* FPS mesure */
    uint32_t frame_count;        /* Compteur d'images decodees */
} StreamState_t;

/* --- 6. PROTOTYPES DES FONCTIONS PUBLIQUES ------------------------------- */

/**
 * @brief Initialise le serveur TCP LwIP (port 5000).
 * @return 0 si succes, negatif si erreur.
 */
int StreamReceiver_Init(void);

/**
 * @brief Tache FreeRTOS principale de reception reseau.
 *        NE SUSPEND PAS TouchGFX — affiche la video dans le framebuffer.
 * @param argument Parametre de tache osThread (NULL)
 */
void StreamReceiver_Task(void *argument);

/**
 * @brief Recupere l'etat courant de la reception (lecture non bloquante).
 * @return Pointeur const vers la structure d'etat.
 */
const StreamState_t* StreamReceiver_GetState(void);

/**
 * @brief Ecrit la derniere frame video dans le framebuffer SDRAM.
 *        A appeler SANS suspendre TouchGFX.
 * @param pFb Pointeur vers le FrameBuffer (NULL = adresse par defaut 0xD0000000).
 */
void StreamReceiver_RenderFrame(uint32_t *pFb);

#ifdef __cplusplus
}
#endif

#endif /* __STREAM_RECEIVER_H__ */
