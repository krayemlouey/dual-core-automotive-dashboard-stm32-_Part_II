/**
  ******************************************************************************
  * @file    stream_receiver.c
  * @brief   Recepteur de flux video TCP Jetson Nano pour STM32H747I-DISCO.
  *
  *          Fonctionnalites :
  *          1. Serveur TCP LwIP port 5000 (IP fixe 192.168.1.50)
  *          2. Reception et defragmentation des paquets TCP (Header 15 o + JPEG)
  *          3. Decompression JPEG via LibJPEG vers buffer RGB888 (RAM_D2 0x30000000)
  *          4. Mise a jour temps reel des alertes dans la memoire partagee SRAM4
  *             (0x38000020 alert_text, 0x38000060 alert_level, 0x38000061 ethernet_ok)
  *             -> Affiche directement dans l'interface TouchGFX sans JAMAIS
  *             la suspendre ni eteindre le retro-eclairage LCD !
  ******************************************************************************
  */

#include "stream_receiver.h"
#include "lwip/tcp.h"
#include "lwip/ip_addr.h"
#include "lwip/pbuf.h"
#include "jpeglib.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "main.h"
#include <stdio.h>
#include <string.h>

/* --- VARIABLES GLOBALES DU MODULE ---------------------------------------- */
static struct tcp_pcb   *s_server_pcb   = NULL;
static struct tcp_pcb   *s_client_pcb   = NULL;
static StreamState_t     s_stream_state = {0};

/* Buffers de reception TCP */
static uint8_t           s_header_buf[STREAM_HEADER_SIZE];
static uint32_t          s_header_bytes_rx = 0;

static uint8_t           s_jpeg_rx_buf[MAX_JPEG_PAYLOAD_SIZE];
static uint32_t          s_jpeg_expected_len = 0;
static uint32_t          s_jpeg_bytes_rx     = 0;

/* Buffer de l'image video decodee en RGB888 (320 x 240 x 3 = 230 400 octets en RAM_D2) */
static uint8_t          *s_decoded_rgb = (uint8_t*)0x30000000UL;
static volatile bool     s_new_frame_ready = false;
static SemaphoreHandle_t s_frame_mutex = NULL;

/* Pointeurs vers la zone de memoire partagee SRAM4 lue par TouchGFX Model::tick() */
#define SRAM4_ALERT_TEXT        ((volatile char*)   0x38000020UL)
#define SRAM4_ALERT_LEVEL       ((volatile uint8_t*)0x38000060UL)
#define SRAM4_ETHERNET_OK       ((volatile uint8_t*)0x38000061UL)
#define SRAM4_ALERT_TARGET      ((volatile uint8_t*)0x38000062UL)

/* --- MISE A JOUR DES ALERTES DANS LE TABLEAU DE BORD TOUCHGFX ----------- */
static void StreamReceiver_UpdateDashboardAlerts(void)
{
    uint32_t now = HAL_GetTick();
    bool is_timed_out = (s_stream_state.last_packet_tick == 0) ||
                        ((now - s_stream_state.last_packet_tick) > 3000);

    if (is_timed_out) {
        s_stream_state.connected = false;
    }

    if (!s_stream_state.connected || is_timed_out) {
        /* Ne pas écraser si une alerte série est en cours */
        if (*SRAM4_ALERT_LEVEL != 2) {
            *SRAM4_ETHERNET_OK = 0;
            *SRAM4_ALERT_LEVEL = 0;
            *SRAM4_ALERT_TARGET = 0;
        }
    } else {
        *SRAM4_ETHERNET_OK = 1;

        if (s_stream_state.alert_flag == 1) {
            /* Alerte cigarette detectee ! */
            *SRAM4_ALERT_LEVEL = 2;  /* Rouge vif / Clignotement */
            *SRAM4_ALERT_TARGET = 0; /* LED centrale */

            uint32_t conf_pct = ((uint32_t)s_stream_state.confidence * 100) / 255;
            char msg[64];
            snprintf(msg, sizeof(msg), "CIGARETTE %lu%%", conf_pct);
            strncpy((char*)SRAM4_ALERT_TEXT, msg, 63);
            ((char*)SRAM4_ALERT_TEXT)[63] = '\0';
        } else {
            /* Surveillance active, pas d'alerte */
            *SRAM4_ALERT_LEVEL = 0;
            *SRAM4_ALERT_TARGET = 0;
            strncpy((char*)SRAM4_ALERT_TEXT, "SURVEILLANCE", 63);
            ((char*)SRAM4_ALERT_TEXT)[63] = '\0';
        }
    }
}

/* --- DECODAGE JPEG VIA LibJPEG ------------------------------------------- */
static bool JPEG_DecodeToRGB(const uint8_t *pJpegData, uint32_t jpegSize, uint8_t *pDstRgb)
{
    if (!pJpegData || jpegSize == 0 || !pDstRgb) return false;
    if (pJpegData[0] != 0xFF || pJpegData[1] != 0xD8) return false;

    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, (unsigned char*)pJpegData, jpegSize);

    if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) {
        jpeg_destroy_decompress(&cinfo);
        return false;
    }

    cinfo.out_color_space = JCS_RGB;
    jpeg_start_decompress(&cinfo);

    int row_stride = cinfo.output_width * cinfo.output_components;
    while (cinfo.output_scanline < cinfo.output_height && cinfo.output_scanline < VIDEO_HEIGHT) {
        unsigned char *buffer_array[1];
        buffer_array[0] = pDstRgb + (cinfo.output_scanline * row_stride);
        jpeg_read_scanlines(&cinfo, buffer_array, 1);
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    s_new_frame_ready = true;
    return true;
}

/* --- TRAITEMENT DU FLUX D'OCTETS TCP ------------------------------------- */
static void StreamReceiver_ProcessBytes(const uint8_t *pData, uint16_t len)
{
    uint16_t offset = 0;

    while (offset < len) {
        if (s_header_bytes_rx < STREAM_HEADER_SIZE) {
            uint32_t needed = STREAM_HEADER_SIZE - s_header_bytes_rx;
            uint32_t avail  = len - offset;
            uint32_t to_copy = (avail < needed) ? avail : needed;

            memcpy(&s_header_buf[s_header_bytes_rx], &pData[offset], to_copy);
            s_header_bytes_rx += to_copy;
            offset += to_copy;

            if (s_header_bytes_rx == STREAM_HEADER_SIZE) {
                uint32_t magic = ((uint32_t)s_header_buf[0] << 24) |
                                 ((uint32_t)s_header_buf[1] << 16) |
                                 ((uint32_t)s_header_buf[2] << 8)  |
                                 ((uint32_t)s_header_buf[3]);

                if (magic != STREAM_MAGIC_WORD) {
                    /* Glisser d'un octet pour retrouver la synchro */
                    memmove(&s_header_buf[0], &s_header_buf[1], STREAM_HEADER_SIZE - 1);
                    s_header_bytes_rx--;
                    continue;
                }

                uint8_t  msg_type = s_header_buf[4];
                uint32_t jpeg_sz  = ((uint32_t)s_header_buf[9] << 24) |
                                    ((uint32_t)s_header_buf[10] << 16) |
                                    ((uint32_t)s_header_buf[11] << 8)  |
                                    ((uint32_t)s_header_buf[12]);

                s_stream_state.alert_flag       = s_header_buf[13];
                s_stream_state.confidence       = s_header_buf[14];
                s_stream_state.last_packet_tick = HAL_GetTick();

                /* Repercuter immediatement dans le tableau de bord TouchGFX */
                StreamReceiver_UpdateDashboardAlerts();

                if (msg_type == MSG_TYPE_IMAGE && jpeg_sz > 0 && jpeg_sz <= MAX_JPEG_PAYLOAD_SIZE) {
                    s_jpeg_expected_len = jpeg_sz;
                    s_jpeg_bytes_rx     = 0;
                } else {
                    s_jpeg_expected_len = 0;
                    s_header_bytes_rx   = 0;
                }
            }
        } else if (s_jpeg_bytes_rx < s_jpeg_expected_len) {
            uint32_t needed = s_jpeg_expected_len - s_jpeg_bytes_rx;
            uint32_t avail  = len - offset;
            uint32_t to_copy = (avail < needed) ? avail : needed;

            memcpy(&s_jpeg_rx_buf[s_jpeg_bytes_rx], &pData[offset], to_copy);
            s_jpeg_bytes_rx += to_copy;
            offset += to_copy;

            if (s_jpeg_bytes_rx == s_jpeg_expected_len) {
                if (xSemaphoreTake(s_frame_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                    JPEG_DecodeToRGB(s_jpeg_rx_buf, s_jpeg_expected_len, s_decoded_rgb);
                    s_stream_state.frame_count++;
                    xSemaphoreGive(s_frame_mutex);

                    uint32_t conf_pct = ((uint32_t)s_stream_state.confidence * 100) / 255;
                    printf("[TCP:5000 Frame #%lu] %lu o | Alerte=%d | Confiance=%u (%lu%%)\r\n",
                           s_stream_state.frame_count, s_jpeg_expected_len,
                           s_stream_state.alert_flag, s_stream_state.confidence, conf_pct);
                }

                s_header_bytes_rx   = 0;
                s_jpeg_expected_len = 0;
            }
        }
    }
}

/* --- CALLBACKS LWIP TCP -------------------------------------------------- */
static err_t stream_tcp_recv_cb(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    (void)arg;

    if (p == NULL) {
        tcp_close(tpcb);
        s_client_pcb = NULL;
        s_stream_state.connected = false;
        StreamReceiver_UpdateDashboardAlerts();
        return ERR_OK;
    }

    if (err == ERR_OK) {
        struct pbuf *q;
        for (q = p; q != NULL; q = q->next) {
            StreamReceiver_ProcessBytes((const uint8_t *)q->payload, q->len);
        }
        tcp_recved(tpcb, p->tot_len);
    }

    pbuf_free(p);
    return ERR_OK;
}

static void stream_tcp_err_cb(void *arg, err_t err)
{
    (void)arg; (void)err;
    s_client_pcb = NULL;
    s_stream_state.connected = false;
    StreamReceiver_UpdateDashboardAlerts();
}

static err_t stream_tcp_accept_cb(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    (void)arg; (void)err;

    if (newpcb == NULL) return ERR_VAL;

    if (s_client_pcb != NULL) {
        tcp_abort(s_client_pcb);
    }

    s_client_pcb = newpcb;
    s_stream_state.connected = true;
    s_stream_state.last_packet_tick = HAL_GetTick();

    printf("[TCP:5000] >>> CLIENT JETSON CONNECTE ! <<<\r\n");

    tcp_recv(s_client_pcb, stream_tcp_recv_cb);
    tcp_err(s_client_pcb,  stream_tcp_err_cb);

    StreamReceiver_UpdateDashboardAlerts();
    return ERR_OK;
}

/* --- FONCTIONS PUBLIQUES ------------------------------------------------- */
int StreamReceiver_Init(void)
{
    /* S'assurer que le retro-eclairage LCD reste bien allume (PJ12 sur STM32H747I-DISCO) */
    HAL_GPIO_WritePin(LCD_BL_GPIO_Port, LCD_BL_Pin, GPIO_PIN_SET);

    if (s_frame_mutex == NULL) {
        s_frame_mutex = xSemaphoreCreateMutex();
    }

    /* Creation de la socket serveur TCP port 5000 */
    s_server_pcb = tcp_new();
    if (s_server_pcb == NULL) return -1;

    err_t err = tcp_bind(s_server_pcb, IP_ADDR_ANY, STREAM_SERVER_PORT);
    if (err != ERR_OK) {
        tcp_close(s_server_pcb);
        s_server_pcb = NULL;
        return -2;
    }

    s_server_pcb = tcp_listen(s_server_pcb);
    if (s_server_pcb == NULL) return -3;

    tcp_accept(s_server_pcb, stream_tcp_accept_cb);
    printf("[TCP:5000] Serveur TCP pret en ecoute sur le port 5000 (IP 192.168.1.50)\r\n");

    return 0;
}

const StreamState_t* StreamReceiver_GetState(void)
{
    return &s_stream_state;
}

void StreamReceiver_RenderFrame(uint32_t *pFb)
{
    (void)pFb;
    /* TouchGFX gere l'affichage de l'ecran sans scintillement.
     * Le buffer s_decoded_rgb contient l'image decodee prete a l'emploi. */
}

void StreamReceiver_Task(void *argument)
{
    (void)argument;

    /* Delai initial pour laisser LwIP et le PHY Ethernet demarrer */
    vTaskDelay(pdMS_TO_TICKS(3000));

    /* Initialisation du serveur TCP LwIP */
    StreamReceiver_Init();

    uint32_t last_fps_tick  = HAL_GetTick();
    uint32_t last_frame_cnt = 0;

    for (;;) {
        uint32_t now = HAL_GetTick();

        /* Calcul du FPS */
        if ((now - last_fps_tick) >= 1000) {
            uint32_t delta_frames = s_stream_state.frame_count - last_frame_cnt;
            s_stream_state.fps = (float)delta_frames * 1000.0f / (float)(now - last_fps_tick);
            last_frame_cnt = s_stream_state.frame_count;
            last_fps_tick  = now;

            /* Verification du timeout Jetson (3 secondes) */
            StreamReceiver_UpdateDashboardAlerts();
        }

        /* NE JAMAIS SUSPENDRE TOUCHGFX :
         * L'interface de votre tableau de bord reste 100% active, fluide et visible !
         * Le retro-eclairage LCD reste allume en permanence. */

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
