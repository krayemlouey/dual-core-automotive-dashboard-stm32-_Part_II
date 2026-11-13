#ifndef DASHBOARDMEDIUMPRESENTER_HPP
#define DASHBOARDMEDIUMPRESENTER_HPP

#include <gui/model/ModelListener.hpp>
#include <mvp/Presenter.hpp>

using namespace touchgfx;

class DashboardMediumView;

class DashboardMediumPresenter : public touchgfx::Presenter, public ModelListener
{
public:
    DashboardMediumPresenter(DashboardMediumView& v);

    virtual void activate();
    virtual void deactivate();

    virtual ~DashboardMediumPresenter() {}

    uint8_t getHour();
    uint8_t getMinute();
    uint8_t getSecond();
    uint16_t getBPM();
    float getDistance();
    float getSpeed();
    float getTemperature();
    float getFuel();
    float getPower();
    float getLidar();
    void setSelectedButton(GaugeButton btn);
    GaugeButton getSelectedButton();
    uint8_t getBatteryPct();
    void setLight(bool light);
    bool getLight();
    void setSelectedScreenSize(ScreenSize screenSize);
    ScreenSize getSelectedScreenSize();

    /* ── Alertes Jetson Nano ───────────────────────────────────── */
    const char* getAlertText();
    uint8_t     getAlertLevel();
    uint8_t     getAlertTarget();
    uint8_t     getEthernetOk();

private:
    DashboardMediumPresenter();

    DashboardMediumView& view;
};

#endif // DASHBOARDMEDIUMPRESENTER_HPP
