#include <gui/mapscreen_screen/MapScreenView.hpp>
#include <touchgfx/hal/HAL.hpp>
#include <gui/mapscreen_screen/MapEngine.hpp>
#include <cstdio>
#ifndef SIMULATOR
#include "main.h"
#include "../../../../../Middlewares/Third_Party/FatFs/src/ff.h"
#endif

// Définition de la structure partagée (DOIT Être IDENTIQUE à CM4/Core/Src/main.c)
typedef struct {
    float speed;           // offset 0
    float temperature;     // offset 4
    float fuel;            // offset 8
    float gps_lat;         // offset 12
    float gps_lon;         // offset 16
    uint8_t gps_updated;   // offset 20
    uint8_t demo_mode;     // offset 21
    uint8_t padding[2];    // offset 22
    float lidar_cm;        // offset 24
    uint32_t can_rx_count; // offset 28
} SharedData_t;

MapScreenView::MapScreenView() : transition_done(false), tick_count(0)
{
}

void MapScreenView::setupScreen()
{
    MapScreenViewBase::setupScreen();
    
    // On utilise mapCanvas comme zone de capture tactile
    mapCanvas.setVisible(true);
    mapCanvas.setAlpha(0); // Transparent mais capture le tactile
    mapCanvas.setTouchable(true);
    
    // Zoom In Button (+)
    btnZoomInBg.setPosition(720, 80, 60, 60);
    btnZoomInBg.setColor(touchgfx::Color::getColorFromRGB(50, 50, 50));
    btnZoomInBg.setAlpha(200);

    btnZoomInH.setPosition(735, 107, 30, 6);
    btnZoomInH.setColor(touchgfx::Color::getColorFromRGB(255, 255, 255));
    
    btnZoomInV.setPosition(747, 95, 6, 30);
    btnZoomInV.setColor(touchgfx::Color::getColorFromRGB(255, 255, 255));

    add(btnZoomInBg);
    add(btnZoomInH);
    add(btnZoomInV);

    // Zoom Out Button (-)
    btnZoomOutBg.setPosition(720, 260, 60, 60);
    btnZoomOutBg.setColor(touchgfx::Color::getColorFromRGB(50, 50, 50));
    btnZoomOutBg.setAlpha(200);

    btnZoomOutH.setPosition(735, 287, 30, 6);
    btnZoomOutH.setColor(touchgfx::Color::getColorFromRGB(255, 255, 255));

    add(btnZoomOutBg);
    add(btnZoomOutH);
    
    transition_done = false;
    tick_count = 0;
}

void MapScreenView::tearDownScreen()
{
    MapScreenViewBase::tearDownScreen();
}

void MapScreenView::handleDragEvent(const touchgfx::DragEvent& evt)
{
    MapScreenViewBase::handleDragEvent(evt);
    MapEngine_Pan(evt.getDeltaX(), evt.getDeltaY());
}

void MapScreenView::handleClickEvent(const touchgfx::ClickEvent& evt)
{
    MapScreenViewBase::handleClickEvent(evt);
    
    if (evt.getType() == touchgfx::ClickEvent::PRESSED) {
        int x = evt.getX();
        int y = evt.getY();
        
        // Zones de Zoom agrandies (Tout le côté droit)
        if (x > 600) {
            if (y < 240) { // Moitié haute : Zoom In
                if (current_zoom < 16) current_zoom++;
            } else { // Moitié basse : Zoom Out
                if (current_zoom > 14) current_zoom--;
            }
        }
        
        gpsInfoText.setColor(touchgfx::Color::getColorFromRGB(255, 0, 0)); 
    } else {
        gpsInfoText.setColor(touchgfx::Color::getColorFromRGB(255, 255, 255));
    }
    gpsInfoText.invalidate();
}

void MapScreenView::handleTickEvent()
{
    MapScreenViewBase::handleTickEvent();
    
    if (tick_count < 1000) {
        tick_count++;
    }
    
    // Phase d'attente minimale pour la stabilité de la transition
    if (tick_count < 5) {
        return;
    }

    if (tick_count == 5) {
#ifndef SIMULATOR
        MapEngine_Init();
#endif
    }

#ifndef SIMULATOR
    volatile float* ptr_lat = (volatile float*)0x3800000C;
    volatile float* ptr_lon = (volatile float*)0x38000010;
    float lat = *ptr_lat;
    float lon = *ptr_lon;
    MapEngine_SetPosition(lat, lon);
#else
    float lat = 35.8256f;
    float lon = 10.6369f;
#endif

    // Affichage des coordonnées + Zoom (Ex: "35/10 Z15")
    char temp_buf[16];
    int lat_int = (int)lat;
    int lon_int = (int)lon;
    snprintf(temp_buf, sizeof(temp_buf), "%d/%d Z%d", lat_int, lon_int, current_zoom);
    
    Unicode::fromUTF8((const uint8_t*)temp_buf, gpsInfoTextBuffer1, GPSINFOTEXTBUFFER1_SIZE);
    gpsInfoText.invalidate();
    
    // Rendu de la carte uniquement si changement ou au début
    static float last_lat = 0;
    static float last_lon = 0;
    static int last_offset_x = 0;
    static int last_offset_y = 0;
    static int force_redraw = 2; // Pour les premières frames

    static int last_zoom = -1;
    static int delay_render = 0;

    bool zoom_changed = (current_zoom != last_zoom);
    if (zoom_changed && last_zoom != -1) {
        if (delay_render == 0) {
            delay_render = 3; // Attendre 3 frames pour laisser le temps au bouton de s'animer
        }
    }

    bool changed = false;
    if (delay_render > 0) {
        delay_render--;
        if (delay_render == 0) {
            changed = true; // Forcer le rendu après le délai
        }
    } else {
        changed = (lat != last_lat || lon != last_lon || offset_x != last_offset_x || offset_y != last_offset_y || zoom_changed);
    }
    
    if (changed || force_redraw > 0) {
        if (force_redraw > 0) force_redraw--;
        
        last_lat = lat;
        last_lon = lon;
        last_offset_x = offset_x;
        last_offset_y = offset_y;
        last_zoom = current_zoom;

        uint8_t* fb = (uint8_t*)touchgfx::HAL::getInstance()->lockFrameBuffer();
        if (fb) {
#ifndef SIMULATOR
            uint16_t width = touchgfx::HAL::DISPLAY_WIDTH;
            uint16_t height = touchgfx::HAL::DISPLAY_HEIGHT;
            MapEngine_Render(fb, width, height);
            SCB_CleanInvalidateDCache_by_Addr((uint32_t*)fb, width * height * 3);
#endif
            touchgfx::HAL::getInstance()->unlockFrameBuffer();
        }
        
        // Invalidate UI widgets to redraw over the map
        btnZoomInBg.invalidate();
        btnZoomInH.invalidate();
        btnZoomInV.invalidate();
        btnZoomOutBg.invalidate();
        btnZoomOutH.invalidate();
        gpsMarker.invalidate();
        button1.invalidate();
    }
}
