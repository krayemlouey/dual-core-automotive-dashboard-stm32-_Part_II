#include <gui/common/FrontendApplication.hpp>

const uint32_t BITMAP_CACHE_SIZE = 900 * 1024;
LOCATION_PRAGMA_NOLOAD("TouchGFX_Framebuffer")
uint8_t bitmapCache[BITMAP_CACHE_SIZE] LOCATION_ATTRIBUTE_NOLOAD("TouchGFX_Framebuffer");

FrontendApplication::FrontendApplication(Model& m, FrontendHeap& heap)
    : FrontendApplicationBase(m, heap)
{
    HAL::getInstance()->setFrameRateCompensation(true);

    Bitmap::setCache((uint16_t*)bitmapCache, BITMAP_CACHE_SIZE, 128);
}
