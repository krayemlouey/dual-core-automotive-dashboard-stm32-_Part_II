#ifndef MAPSCREENVIEW_HPP
#define MAPSCREENVIEW_HPP

#include <gui_generated/mapscreen_screen/MapScreenViewBase.hpp>
#include <gui/mapscreen_screen/MapScreenPresenter.hpp>

class MapScreenView : public MapScreenViewBase
{
public:
    MapScreenView();
    virtual ~MapScreenView() {}
    virtual void setupScreen();
    virtual void tearDownScreen();
    virtual void handleTickEvent();
    virtual void handleDragEvent(const touchgfx::DragEvent& evt);
    virtual void handleClickEvent(const touchgfx::ClickEvent& evt);
protected:
    bool transition_done;
    int tick_count;

    touchgfx::Box btnZoomInBg;
    touchgfx::Box btnZoomInH;
    touchgfx::Box btnZoomInV;

    touchgfx::Box btnZoomOutBg;
    touchgfx::Box btnZoomOutH;
};

#endif // MAPSCREENVIEW_HPP
