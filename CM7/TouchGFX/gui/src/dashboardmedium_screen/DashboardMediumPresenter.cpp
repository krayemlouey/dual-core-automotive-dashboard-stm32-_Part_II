#include <gui/dashboardmedium_screen/DashboardMediumView.hpp>
#include <gui/dashboardmedium_screen/DashboardMediumPresenter.hpp>

DashboardMediumPresenter::DashboardMediumPresenter(DashboardMediumView& v) : view(v) {}

void DashboardMediumPresenter::activate()   {}
void DashboardMediumPresenter::deactivate() {}

uint8_t  DashboardMediumPresenter::getHour()     { return model->getHour();     }
uint8_t  DashboardMediumPresenter::getMinute()   { return model->getMinute();   }
uint8_t  DashboardMediumPresenter::getSecond()   { return model->getSecond();   }
uint16_t DashboardMediumPresenter::getBPM()      { return model->getBPM();      }
float    DashboardMediumPresenter::getDistance() { return model->getDistance(); }
float    DashboardMediumPresenter::getSpeed()    { return model->getSpeed();    }
float    DashboardMediumPresenter::getTemperature() { return model->getTemperature(); }
float    DashboardMediumPresenter::getFuel()     { return model->getFuel();     }
float    DashboardMediumPresenter::getPower()    { return model->getPower();    }
float    DashboardMediumPresenter::getLidar()    { return model->getLidar();    }

void DashboardMediumPresenter::setSelectedButton(GaugeButton btn) { model->setSelectedGaugeButton(btn); }
GaugeButton DashboardMediumPresenter::getSelectedButton()         { return model->getSelectedGaugeButton(); }

uint8_t DashboardMediumPresenter::getBatteryPct() { return model->getBatteryPct(); }

void DashboardMediumPresenter::setLight(bool light)        { model->setLight(light);        }
bool DashboardMediumPresenter::getLight()                  { return model->getLight();       }

void       DashboardMediumPresenter::setSelectedScreenSize(ScreenSize s) { model->setSelectedScreenSize(s);    }
ScreenSize DashboardMediumPresenter::getSelectedScreenSize()             { return model->getSelectedScreenSize(); }

/* ── Alertes Jetson Nano ──────────────────────────────────────────── */
const char* DashboardMediumPresenter::getAlertText()   { return model->getAlertText();   }
uint8_t     DashboardMediumPresenter::getAlertLevel()  { return model->getAlertLevel();  }
uint8_t     DashboardMediumPresenter::getAlertTarget() { return model->getAlertTarget(); }
uint8_t     DashboardMediumPresenter::getEthernetOk()  { return model->getEthernetOk();  }

