#include <gui/mapscreen_screen/MapEngine.hpp>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <cstring>
#ifndef SIMULATOR
#include "../../../../../Middlewares/Third_Party/FatFs/src/ff.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Variables globales pour la compilation
static float current_lat = 35.8256f;
static float current_lon = 10.6369f;
int current_zoom = 15;
int offset_x = 0;
int offset_y = 0;

#ifndef SIMULATOR
static FATFS fs;
#endif

void MapEngine_Init(void) {
#ifndef SIMULATOR
    f_mount(&fs, "0:/", 0);
#endif
}

void MapEngine_SetPosition(float lat, float lon) {
    current_lat = lat;
    current_lon = lon;
}

void MapEngine_Pan(int dx, int dy) {
    offset_x += dx * 2;
    offset_y += dy * 2;
}

#ifndef SIMULATOR
static int lon2tilex(float lon, int z) {
    return (int)(floor((lon + 180.0) / 360.0 * pow(2.0, z)));
}

static int lat2tiley(float lat, int z) {
    return (int)(floor((1.0 - log(tan(lat * M_PI / 180.0) + 1.0 / cos(lat * M_PI / 180.0)) / M_PI) / 2.0 * pow(2.0, z)));
}
#endif


#include <touchgfx/hal/Config.hpp>

// Cache pour les tuiles afin de ne pas relire la carte SD à chaque frame
static int cached_z = -1;
static int cached_tx_center = -1;
static int cached_ty_center = -1;

LOCATION_PRAGMA_NOLOAD("TouchGFX_Framebuffer")
static uint8_t cached_tiles[3][3][256 * 256 * 3] LOCATION_ATTRIBUTE_NOLOAD("TouchGFX_Framebuffer");

void MapEngine_Render(uint8_t* framebuffer, uint16_t width, uint16_t height) {
#ifndef SIMULATOR
    int z = current_zoom;
    int tx_center = lon2tilex(current_lon, z);
    int ty_center = lat2tiley(current_lat, z);

    // Mettre à jour le cache si nécessaire
    if (z != cached_z || tx_center != cached_tx_center || ty_center != cached_ty_center) {
        cached_z = z;
        cached_tx_center = tx_center;
        cached_ty_center = ty_center;
        
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                int tx = tx_center + dx;
                int ty = ty_center + dy;
                FIL file;
                char filepath[64];
                sprintf(filepath, "0:/tiles/%d/%d/%d.bmp", z, tx, ty);
                
                if (f_open(&file, filepath, FA_READ) == FR_OK) {
                    UINT br;
                    f_lseek(&file, 54);
                    f_read(&file, cached_tiles[dy + 1][dx + 1], 256 * 256 * 3, &br);
                    f_close(&file);
                } else {
                    memset(cached_tiles[dy + 1][dx + 1], 0, 256 * 256 * 3);
                }
            }
        }
    }

    // Dessiner depuis le cache
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            int x_pos = (width / 2 - 128) + (dx * 256) + offset_x;
            int y_pos = (height / 2 - 128) + (dy * 256) + offset_y;
            
            if (x_pos > -256 && x_pos < width && y_pos > -256 && y_pos < height) {
                for (int y = 0; y < 256; y++) {
                    int draw_y = y_pos + y;
                    if (draw_y >= 0 && draw_y < height) {
                        uint8_t* line_data = cached_tiles[dy + 1][dx + 1] + ((255 - y) * 256 * 3);
                        
                        uint8_t* dst = framebuffer + (draw_y * 832 * 3) + (std::max(0, x_pos) * 3);
                        int start_x = (x_pos < 0) ? -x_pos : 0;
                        int count_x = 256 - start_x;
                        if (x_pos + 256 > width) count_x -= (x_pos + 256 - width);
                        
                        if (count_x > 0) {
                            for (int i = 0; i < count_x; i++) {
                                dst[i*3 + 0] = line_data[(start_x + i)*3 + 2]; // R
                                dst[i*3 + 1] = line_data[(start_x + i)*3 + 1]; // G
                                dst[i*3 + 2] = line_data[(start_x + i)*3 + 0]; // B
                            }
                        }
                    }
                }
            }
        }
    }
#endif
}

#ifdef __cplusplus
}
#endif
