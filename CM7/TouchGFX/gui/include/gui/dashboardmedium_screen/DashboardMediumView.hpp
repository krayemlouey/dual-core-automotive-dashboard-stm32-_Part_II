#ifndef DASHBOARDMEDIUMVIEW_HPP
#define DASHBOARDMEDIUMVIEW_HPP

#include <gui_generated/dashboardmedium_screen/DashboardMediumViewBase.hpp>
#include <gui/dashboardmedium_screen/DashboardMediumPresenter.hpp>
#include <touchgfx/widgets/TextAreaWithWildcard.hpp>
#include <touchgfx/widgets/Box.hpp>
#include <touchgfx/containers/Container.hpp>
#include <touchgfx/widgets/canvas/Line.hpp>
#include <touchgfx/widgets/canvas/PainterRGB888.hpp>

class DashboardMediumView : public DashboardMediumViewBase
{
public:
    DashboardMediumView();
    virtual ~DashboardMediumView() {}
    virtual void setupScreen();
    virtual void tearDownScreen();
    virtual void handleTickEvent();
    virtual void bpmButtonClicked();
    virtual void timeButtonClicked();
    virtual void distanceButtonClicked();
    virtual void weatherButtonClicked();
    virtual void modeButtonClicked();
    virtual void languageButtonClicked();
    virtual void closeLanguageSelectorClicked();
    virtual void languageChanged();

private:
    void setOrangeBattery();
    void setGreenBattery();
    void updateBatteryLevel();
    void updateTripInfo();
    void updateTheme();

    uint16_t oldBPM;
    uint8_t  oldMinute;
    uint8_t  oldSecond;
    int      oldDistance;
    bool     alreadyOrange;
    uint16_t oldBitmapID;
    uint16_t dynamicBmpLanguageSelector;
    bool     languageSelectorMoving;
    float    oldLidar;
    int      oldTemp;
    int      oldFuel;

    /* ── Widget LiDAR ───────────────────────────────────────────── */
    touchgfx::TextAreaWithOneWildcard lidarValue;
    static const uint16_t LIDARVALUE_SIZE = 8;
    touchgfx::Unicode::UnicodeChar lidarValueBuffer[LIDARVALUE_SIZE];

    /* ── Zone d'alerte Jetson Nano ─────────────────────────────── */
    touchgfx::Box                     alertBox;        /* fond coloré selon le niveau   */
    touchgfx::TextAreaWithOneWildcard  alertText;       /* texte de l'alerte             */
    static const uint16_t ALERTTEXT_SIZE = 64;
    touchgfx::Unicode::UnicodeChar alertTextBuffer[ALERTTEXT_SIZE];

    /* état interne alertes */
    char     oldAlertMsg[64];
    uint8_t  oldAlertLevel;

    /* ── Structure pour LED triangulaire (double périmètre, creuse au centre) ── */
    struct TriangleLed
    {
        touchgfx::Container     container;
        touchgfx::Line          left;
        touchgfx::Line          right;
        touchgfx::Line          bottom;
        touchgfx::Line          innerLeft;
        touchgfx::Line          innerRight;
        touchgfx::Line          innerBottom;
        touchgfx::PainterRGB888 painter;

        void setup(int16_t x, int16_t y, int16_t w = 28, int16_t h = 24);
        void setColor(touchgfx::colortype c);
    };

    /* Les 3 LEDs triangulaires :
     * 1. ledTemp  : sous la jauge de température (X=77, Y=450)
     * 2. ledFuel  : sous la jauge de carburant   (X=700, Y=450)
     * 3. ledSpeed : au-dessus, entre 0 et 250    (X=386, Y=56)
     */
    TriangleLed ledTemp;
    TriangleLed ledFuel;
    TriangleLed ledSpeed;

    uint8_t     ledBlinkCounter;  /* compteur de ticks pour clignoter */
    bool        ledBlinkState;    /* état ON/OFF du clignotement      */
};

#endif // DASHBOARDMEDIUMVIEW_HPP
