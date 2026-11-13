#ifndef MODEL_HPP
#define MODEL_HPP

#include <touchgfx/hal/types.hpp>
#include <texts/TextKeysAndLanguages.hpp>

class ModelListener;

enum GaugeButton
{
    bpm,
    time,
    distance
};

enum ScreenSize
{
    smallSize,
    mediumSize,
    largeSize
};

class Model
{
public:
    Model();

    void bind(ModelListener* listener)
    {
        modelListener = listener;
    }

    void tick();

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

    /* ── Alertes Jetson Nano ───────────────────────────────────────── */
    const char* getAlertText();   /* Texte de l'alerte (max 63 chars + \0) */
    uint8_t     getAlertLevel();  /* 0=OK, 1=WARNING, 2=DANGER             */
    uint8_t     getAlertTarget(); /* 0=speed, 1=temp, 2=fuel               */
    uint8_t     getEthernetOk();  /* 1 si lien Ethernet Jetson actif       */

    void setSelectedGaugeButton(GaugeButton btn);
    GaugeButton getSelectedGaugeButton();
    void setBatteryPct(uint8_t batteryLevel);
    uint8_t getBatteryPct();
    void setLight(bool light);
    bool getLight();
    void setSelectedWeatherItem(int itemNo);
    int getSelectedWeatherItem();
    void setSelectedScreenSize(ScreenSize screenSize);
    ScreenSize getSelectedScreenSize();
    int getHourTemperature(int hr);
    int getHourWindSpeed(int hr);

    /* ─── Audio player API ─────────────────────────────────────────── */
    void audioActivate();
    void audioPause();
    void audioPlayPause();
    void audioNext();
    void audioPrev();

    const char* getAudioSongName();
    uint32_t    getAudioPosition();
    uint32_t    getAudioDuration();
    bool        isAudioPlaying();

protected:
    ModelListener* modelListener;

private:
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint16_t bpm;
    float distance;
    float speed;
    float temperature;
    float fuel;
    float power;
    float lidar;

    /* ── Alertes Jetson Nano ───────────────────────────────────────── */
    char    alertText[64];
    uint8_t alertLevel;
    uint8_t alertTarget;
    uint8_t ethernetOk;

    GaugeButton gaugeButtonSelected;
    uint8_t batteryPct;
    bool isLight;
    int selectedWeatherItem;
    ScreenSize selectedScreenSize;
    int count;

    /* ─── Audio tick counter ───────────────────────────────────────── */
    uint16_t audioTickCounter;
    static const uint16_t AUDIO_UPDATE_INTERVAL = 56;
};

#endif // MODEL_HPP

