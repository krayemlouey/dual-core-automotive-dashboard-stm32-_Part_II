/*
 * AudioPlayer.c
 * Lecteur audio WAV 44100 Hz / 16-bit stéréo
 * via SAI1-Block A + DMA1 Stream0 + I2C4 (WM8994 codec)
 * STM32H747I-DISCO
 */
#include "AudioPlayer.h"
#include "fatfs.h"
#include <string.h>
#include <stdio.h>

#ifndef SIMULATOR

/* ================================================================
   Définitions matérielles (STM32H747I-DISCO)
   ================================================================ */

/* SAI1 Block A : I2S Master Transmit sur PE2/PE3/PE4/PE5/PE6 */
static SAI_HandleTypeDef  hSai1;
static DMA_HandleTypeDef  hDmaSai1;

/* I2C4 pour configurer le codec WM8994 (PD12=SCL, PD13=SDA) */
static I2C_HandleTypeDef  hI2c4;
#define WM8994_I2C_ADDR   (0x1A << 1)   /* ADD = GND → 0x1A */

/* ---- Double buffer audio ----------------------------------------- */
static int16_t audio_buffer[AUDIO_BUFFER_SIZE];   /* en SDRAM via linker */
static FIL     wav_file;
static uint8_t wav_open = 0;
static uint32_t data_start_offset = 0;
static uint32_t data_size_bytes   = 0;
static uint32_t bytes_read_total  = 0;

/* ---- État global -------------------------------------------------- */
static AudioState_t audio_state = AUDIO_STATE_IDLE;
static AudioInfo_t  audio_info  = {0};

/* ---- Liste des chansons ------------------------------------------- */
#define MAX_SONGS 32
static char song_list[MAX_SONGS][64];
static uint8_t song_count = 0;

/* ================================================================
   Fonctions internes
   ================================================================ */

/* Écrire un registre WM8994 via I2C */
static void WM8994_WriteReg(uint16_t reg, uint16_t val) {
    uint8_t buf[4];
    buf[0] = (reg >> 8) & 0xFF;
    buf[1] = (reg)      & 0xFF;
    buf[2] = (val >> 8) & 0xFF;
    buf[3] = (val)      & 0xFF;
    HAL_I2C_Master_Transmit(&hI2c4, WM8994_I2C_ADDR, buf, 4, 100);
}

/* Initialiser I2C4 */
static void I2C4_Init(void) {
    __HAL_RCC_I2C4_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Pin       = GPIO_PIN_12 | GPIO_PIN_13;  /* SCL / SDA */
    gpio.Mode      = GPIO_MODE_AF_OD;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF4_I2C4;
    HAL_GPIO_Init(GPIOD, &gpio);

    hI2c4.Instance              = I2C4;
    hI2c4.Init.Timing           = 0x00C0EAFF;  /* ~100 kHz @200 MHz APB4 */
    hI2c4.Init.OwnAddress1      = 0;
    hI2c4.Init.AddressingMode   = I2C_ADDRESSINGMODE_7BIT;
    hI2c4.Init.DualAddressMode  = I2C_DUALADDRESS_DISABLE;
    hI2c4.Init.GeneralCallMode  = I2C_GENERALCALL_DISABLE;
    hI2c4.Init.NoStretchMode    = I2C_NOSTRETCH_DISABLE;
    HAL_I2C_Init(&hI2c4);
}

/* Configurer le codec WM8994 pour la lecture audio */
static void WM8994_Init_Audio(void) {
    /* Reset */
    WM8994_WriteReg(0x0000, 0x0000);
    HAL_Delay(10);

    /* Enable VMID, BIAS, HPOUT1L/R */
    WM8994_WriteReg(0x0001, 0x0303);   /* VMID_SEL=01, BIAS_ENA, HPOUT1L_ENA, HPOUT1R_ENA */
    WM8994_WriteReg(0x0002, 0x6350);   /* DAC1L/R enable + mixer */
    WM8994_WriteReg(0x0003, 0x0330);   /* MIXOUTL/R + SPKOUTL enable */

    /* AIF1: 16-bit I2S, maître */
    WM8994_WriteReg(0x0300, 0x4010);   /* AIF1_LRCLK_DIR=Master, AIF1_FMT=I2S, AIF1_WL=16bit */
    WM8994_WriteReg(0x0302, 0x0000);   /* AIF1_BCLK_DIV */
    WM8994_WriteReg(0x0303, 0x0040);   /* AIF1_LRCLK_RATE */

    /* FLL1 : 44100 Hz depuis MCLK1 (12 MHz) */
    WM8994_WriteReg(0x0220, 0x0005);
    WM8994_WriteReg(0x0221, 0x0700);
    WM8994_WriteReg(0x0222, 0x3126);
    WM8994_WriteReg(0x0223, 0x0100);
    WM8994_WriteReg(0x0200, 0x0011);   /* AIF1CLK = FLL1 */
    WM8994_WriteReg(0x0208, 0x000A);   /* DSP clock / AIF1 clock enable */

    /* Volume haut-parleur + casque */
    WM8994_WriteReg(0x0025, 0x01EF);   /* Headphone volume gauche  */
    WM8994_WriteReg(0x0026, 0x01EF);   /* Headphone volume droite  */

    /* Chemin audio : DAC1 → HPOUT1 */
    WM8994_WriteReg(0x002D, 0x0001);   /* DAC1L → MIXOUTL */
    WM8994_WriteReg(0x002E, 0x0001);   /* DAC1R → MIXOUTR */
    WM8994_WriteReg(0x0420, 0x0000);   /* AIF1 DAC1 output enable */
}

/* Initialiser SAI1 Block A en I2S Master TX */
static void SAI1_Init(void) {
    __HAL_RCC_SAI1_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();

    /* PE2=MCLK, PE3=SCK, PE4=FS, PE5=SD */
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin       = GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF6_SAI1;
    HAL_GPIO_Init(GPIOE, &gpio);

    hSai1.Instance            = SAI1_Block_A;
    hSai1.Init.Protocol       = SAI_FREE_PROTOCOL;
    hSai1.Init.AudioMode      = SAI_MODEMASTER_TX;
    hSai1.Init.DataSize       = SAI_DATASIZE_16;
    hSai1.Init.FirstBit       = SAI_FIRSTBIT_MSB;
    hSai1.Init.ClockStrobing  = SAI_CLOCKSTROBING_FALLINGEDGE;
    hSai1.Init.Synchro        = SAI_ASYNCHRONOUS;
    hSai1.Init.OutputDrive    = SAI_OUTPUTDRIVE_ENABLE;
    hSai1.Init.NoDivider      = SAI_MASTERDIVIDER_ENABLE;
    hSai1.Init.FIFOThreshold  = SAI_FIFOTHRESHOLD_HF;
    hSai1.Init.AudioFrequency = SAI_AUDIO_FREQUENCY_44K;
    hSai1.Init.SynchroExt     = SAI_SYNCEXT_DISABLE;
    hSai1.Init.MonoStereoMode = SAI_STEREOMODE;
    hSai1.Init.CompandingMode = SAI_NOCOMPANDING;
    hSai1.Init.TriState       = SAI_OUTPUT_NOTRELEASED;

    hSai1.FrameInit.FrameLength       = 64;
    hSai1.FrameInit.ActiveFrameLength = 32;
    hSai1.FrameInit.FSDefinition      = SAI_FS_CHANNEL_IDENTIFICATION;
    hSai1.FrameInit.FSPolarity        = SAI_FS_ACTIVE_LOW;
    hSai1.FrameInit.FSOffset          = SAI_FS_BEFOREFIRSTBIT;

    hSai1.SlotInit.FirstBitOffset = 0;
    hSai1.SlotInit.SlotSize       = SAI_SLOTSIZE_32B;
    hSai1.SlotInit.SlotNumber     = 2;
    hSai1.SlotInit.SlotActive     = SAI_SLOTACTIVE_0 | SAI_SLOTACTIVE_1;

    HAL_SAI_Init(&hSai1);
}

/* Initialiser DMA1 Stream0 pour SAI1 TX */
static void DMA_SAI1_Init(void) {
    __HAL_RCC_DMA1_CLK_ENABLE();

    hDmaSai1.Instance                 = DMA1_Stream0;
    hDmaSai1.Init.Request             = DMA_REQUEST_SAI1_A;
    hDmaSai1.Init.Direction           = DMA_MEMORY_TO_PERIPH;
    hDmaSai1.Init.PeriphInc           = DMA_PINC_DISABLE;
    hDmaSai1.Init.MemInc              = DMA_MINC_ENABLE;
    hDmaSai1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hDmaSai1.Init.MemDataAlignment    = DMA_MDATAALIGN_HALFWORD;
    hDmaSai1.Init.Mode                = DMA_CIRCULAR;
    hDmaSai1.Init.Priority            = DMA_PRIORITY_HIGH;
    hDmaSai1.Init.FIFOMode            = DMA_FIFOMODE_ENABLE;
    hDmaSai1.Init.FIFOThreshold       = DMA_FIFO_THRESHOLD_FULL;
    hDmaSai1.Init.MemBurst            = DMA_MBURST_SINGLE;
    hDmaSai1.Init.PeriphBurst         = DMA_PBURST_SINGLE;

    HAL_DMA_Init(&hDmaSai1);
    __HAL_LINKDMA(&hSai1, hdmatx, hDmaSai1);

    HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);
}

/* Lire l'en-tête WAV et positionner le pointeur fichier sur les données */
static int WAV_ParseHeader(FIL* fp) {
    uint8_t hdr[44];
    UINT br;
    f_lseek(fp, 0);
    if (f_read(fp, hdr, 44, &br) != FR_OK || br != 44) return -1;
    if (hdr[0] != 'R' || hdr[1] != 'I' || hdr[2] != 'F' || hdr[3] != 'F') return -2;
    if (hdr[8] != 'W' || hdr[9] != 'A' || hdr[10] != 'V' || hdr[11] != 'E') return -3;

    uint32_t sample_rate = hdr[24] | (hdr[25]<<8) | (hdr[26]<<16) | (hdr[27]<<24);
    (void)sample_rate;   /* On suppose 44100 */

    data_size_bytes   = hdr[40] | (hdr[41]<<8) | (hdr[42]<<16) | (hdr[43]<<24);
    data_start_offset = 44;
    bytes_read_total  = 0;

    /* Durée : data_size / (sample_rate * channels * bytes_per_sample) */
    audio_info.duration_sec = data_size_bytes / (AUDIO_SAMPLE_RATE * AUDIO_CHANNELS * 2);
    return 0;
}

/* Charger la liste des fichiers WAV depuis 0:/music/ */
static void LoadSongList(void) {
    DIR dir;
    FILINFO fno;
    song_count = 0;
    if (f_opendir(&dir, "0:/music") != FR_OK) return;
    while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0] != '\0') {
        if (fno.fname[0] == '.') continue;
        int len = strlen(fno.fname);
        if (len > 4 &&
            (fno.fname[len-4] == '.' &&
            (fno.fname[len-3] == 'w' || fno.fname[len-3] == 'W') &&
            (fno.fname[len-2] == 'a' || fno.fname[len-2] == 'A') &&
            (fno.fname[len-1] == 'v' || fno.fname[len-1] == 'V'))) {
            if (song_count < MAX_SONGS) {
                strncpy(song_list[song_count], fno.fname, 63);
                song_count++;
            }
        }
    }
    f_closedir(&dir);
    audio_info.total_songs = song_count;
}

/* Remplir une moitié du buffer depuis la carte SD */
static void FillBuffer(int16_t* buf, uint32_t samples) {
    UINT br;
    uint32_t bytes_to_read = samples * 2; /* 16-bit = 2 octets */
    if (bytes_read_total >= data_size_bytes) {
        /* Fin du fichier : passer à la chanson suivante */
        memset(buf, 0, bytes_to_read);
        AudioPlayer_Next();
        return;
    }
    uint32_t remaining = data_size_bytes - bytes_read_total;
    if (bytes_to_read > remaining) bytes_to_read = remaining;
    f_read(&wav_file, buf, bytes_to_read, &br);
    if (br < samples * 2) memset((uint8_t*)buf + br, 0, samples * 2 - br);
    bytes_read_total += br;

    /* Mise à jour position */
    audio_info.position_sec = bytes_read_total / (AUDIO_SAMPLE_RATE * AUDIO_CHANNELS * 2);
}

/* ================================================================
   API publique
   ================================================================ */

void AudioPlayer_Init(void) {
    I2C4_Init();
    SAI1_Init();
    DMA_SAI1_Init();
    WM8994_Init_Audio();
    LoadSongList();
    audio_state = AUDIO_STATE_IDLE;
    audio_info.volume = AUDIO_DEFAULT_VOLUME;
    AudioPlayer_SetVolume(AUDIO_DEFAULT_VOLUME);
}

void AudioPlayer_Play(const char* filepath) {
    if (wav_open) { f_close(&wav_file); wav_open = 0; }
    HAL_SAI_DMAStop(&hSai1);

    char fullpath[80];
    snprintf(fullpath, sizeof(fullpath), "0:/music/%s", filepath);

    if (f_open(&wav_file, fullpath, FA_READ) != FR_OK) {
        audio_state = AUDIO_STATE_ERROR;
        return;
    }
    wav_open = 1;
    if (WAV_ParseHeader(&wav_file) != 0) {
        f_close(&wav_file); wav_open = 0;
        audio_state = AUDIO_STATE_ERROR;
        return;
    }
    f_lseek(&wav_file, data_start_offset);

    /* Pré-remplir le buffer entier */
    FillBuffer(audio_buffer, AUDIO_BUFFER_SIZE / 2);
    FillBuffer(audio_buffer + AUDIO_BUFFER_SIZE / 2, AUDIO_BUFFER_SIZE / 2);

    strncpy(audio_info.name, filepath, 63);
    audio_state = AUDIO_STATE_PLAYING;

    HAL_SAI_Transmit_DMA(&hSai1, (uint8_t*)audio_buffer, AUDIO_BUFFER_SIZE);
}

void AudioPlayer_Pause(void) {
    if (audio_state == AUDIO_STATE_PLAYING) {
        HAL_SAI_DMAPause(&hSai1);
        audio_state = AUDIO_STATE_PAUSED;
    }
}

void AudioPlayer_Resume(void) {
    if (audio_state == AUDIO_STATE_PAUSED) {
        HAL_SAI_DMAResume(&hSai1);
        audio_state = AUDIO_STATE_PLAYING;
    }
}

void AudioPlayer_Stop(void) {
    HAL_SAI_DMAStop(&hSai1);
    if (wav_open) { f_close(&wav_file); wav_open = 0; }
    audio_state = AUDIO_STATE_STOPPED;
    audio_info.position_sec = 0;
}

void AudioPlayer_Next(void) {
    if (song_count == 0) return;
    audio_info.current_index = (audio_info.current_index + 1) % song_count;
    AudioPlayer_Play(song_list[audio_info.current_index]);
}

void AudioPlayer_Prev(void) {
    if (song_count == 0) return;
    if (audio_info.current_index == 0)
        audio_info.current_index = song_count - 1;
    else
        audio_info.current_index--;
    AudioPlayer_Play(song_list[audio_info.current_index]);
}

void AudioPlayer_SetVolume(uint8_t vol) {
    if (vol > 100) vol = 100;
    audio_info.volume = vol;
    /* Convertir 0-100 en 0x00-0xFF pour WM8994 */
    uint16_t wm_vol = ((uint32_t)vol * 0x3F) / 100;
    WM8994_WriteReg(0x0025, 0x0100 | wm_vol);  /* Headphone L */
    WM8994_WriteReg(0x0026, 0x0100 | wm_vol);  /* Headphone R */
}

AudioState_t AudioPlayer_GetState(void) { return audio_state; }
AudioInfo_t* AudioPlayer_GetInfo(void)  { return &audio_info; }

/* DMA Half Transfer : remplir la 1ère moitié du buffer */
void AudioPlayer_DMA_HalfCallback(void) {
    if (audio_state == AUDIO_STATE_PLAYING)
        FillBuffer(audio_buffer, AUDIO_BUFFER_SIZE / 2);
}

/* DMA Full Transfer : remplir la 2ème moitié du buffer */
void AudioPlayer_DMA_CpltCallback(void) {
    if (audio_state == AUDIO_STATE_PLAYING)
        FillBuffer(audio_buffer + AUDIO_BUFFER_SIZE / 2, AUDIO_BUFFER_SIZE / 2);
}

/* Callbacks HAL SAI → redirigés vers nos fonctions */
void HAL_SAI_TxHalfCpltCallback(SAI_HandleTypeDef *hsai) {
    (void)hsai;
    AudioPlayer_DMA_HalfCallback();
}

void HAL_SAI_TxCpltCallback(SAI_HandleTypeDef *hsai) {
    (void)hsai;
    AudioPlayer_DMA_CpltCallback();
}

/* ISR DMA1 Stream0 */
void DMA1_Stream0_IRQHandler(void) {
    HAL_DMA_IRQHandler(&hDmaSai1);
}

#endif /* SIMULATOR */
