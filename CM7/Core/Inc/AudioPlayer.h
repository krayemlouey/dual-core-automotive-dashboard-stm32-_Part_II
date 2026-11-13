/*
 * AudioPlayer.h
 * Lecteur audio WAV via SAI1 + DMA + FatFs
 * Compatible STM32H747I-DISCO (WM8994 codec)
 */
#ifndef AUDIOPLAYER_H
#define AUDIOPLAYER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h7xx_hal.h"
#include <stdint.h>

/* ---- Configuration ------------------------------------------------ */
#define AUDIO_BUFFER_SIZE        (4096 * 2)   /* 2 x 4096 octets double buffer */
#define AUDIO_SAMPLE_RATE        44100
#define AUDIO_BITS_PER_SAMPLE    16
#define AUDIO_CHANNELS           2
#define AUDIO_DEFAULT_VOLUME     80           /* 0 – 100 */

/* ---- États du lecteur --------------------------------------------- */
typedef enum {
    AUDIO_STATE_IDLE = 0,
    AUDIO_STATE_PLAYING,
    AUDIO_STATE_PAUSED,
    AUDIO_STATE_STOPPED,
    AUDIO_STATE_ERROR
} AudioState_t;

/* ---- Infos sur la chanson en cours --------------------------------- */
typedef struct {
    char     name[64];       /* Nom du fichier sans chemin             */
    uint32_t duration_sec;   /* Durée totale en secondes               */
    uint32_t position_sec;   /* Position actuelle en secondes          */
    uint8_t  volume;         /* Volume 0–100                           */
    uint8_t  total_songs;    /* Nombre total de fichiers dans /music   */
    uint8_t  current_index;  /* Index de la chanson actuelle (0-based) */
} AudioInfo_t;

/* ---- API publique -------------------------------------------------- */
void         AudioPlayer_Init(void);
void         AudioPlayer_Play(const char* filepath);
void         AudioPlayer_Pause(void);
void         AudioPlayer_Resume(void);
void         AudioPlayer_Stop(void);
void         AudioPlayer_Next(void);
void         AudioPlayer_Prev(void);
void         AudioPlayer_SetVolume(uint8_t vol);  /* 0–100 */
AudioState_t AudioPlayer_GetState(void);
AudioInfo_t* AudioPlayer_GetInfo(void);

/* Appelé depuis le gestionnaire DMA en ISR — ne pas appeler manuellement */
void         AudioPlayer_DMA_HalfCallback(void);
void         AudioPlayer_DMA_CpltCallback(void);

#ifdef __cplusplus
}
#endif

#endif /* AUDIOPLAYER_H */
