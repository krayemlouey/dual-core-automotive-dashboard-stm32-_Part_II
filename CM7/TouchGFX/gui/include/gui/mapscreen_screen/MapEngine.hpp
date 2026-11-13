#ifndef MAP_ENGINE_HPP
#define MAP_ENGINE_HPP

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void MapEngine_Init(void);
void MapEngine_SetPosition(float lat, float lon);
void MapEngine_Pan(int dx, int dy);
extern int offset_x;
extern int offset_y;
extern int current_zoom;
void MapEngine_Render(uint8_t* framebuffer, uint16_t screen_w, uint16_t screen_h);
void latlon_to_tile(float lat, float lon, int zoom, int* tx, int* ty);
void latlon_to_pixel(float lat, float lon, int zoom, int* px, int* py);

#ifdef __cplusplus
}
#endif

#endif // MAP_ENGINE_HPP
