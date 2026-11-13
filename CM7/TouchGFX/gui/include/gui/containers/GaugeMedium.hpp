#ifndef GAUGEMEDIUM_HPP
#define GAUGEMEDIUM_HPP

#include <gui_generated/containers/GaugeMediumBase.hpp>
#include <touchgfx/widgets/canvas/PainterRGB888Bitmap.hpp>

class GaugeMedium : public GaugeMediumBase
{
public:
    GaugeMedium();
    virtual ~GaugeMedium();
    virtual void initialize();
    void setSpeed(float speed);
    void setPower(float power);
    bool isOrange();
    void setColor(int BGcolor);
    void updateTextAreas();
    void setOrangeGauge();
    void setGreenGauge();

private:
    void updateSpeedGauge();
    void updatePowerGauge();

    const uint8_t MEDIUM_SHIFT = 19;
    const uint16_t SPEED_X_START = 350;
    const uint16_t SPEED_X_START_FIRST_TURN = 280 - MEDIUM_SHIFT;
    const uint16_t SPEED_X_MIDDLE_FIRST_TURN = 262 - MEDIUM_SHIFT;
    const uint16_t SPEED_X_END_FIRST_TURN = 252 - MEDIUM_SHIFT;
    const uint16_t SPEED_X_START_SECOND_TURN = 151 - MEDIUM_SHIFT;
    const uint16_t SPEED_MIDDLE_X = 146 - MEDIUM_SHIFT;
    const uint16_t SPEED_X_END_SECOND_TURN = 151 - MEDIUM_SHIFT;
    const uint16_t SPEED_X_START_LAST_TURN = 252 - MEDIUM_SHIFT;
    const uint16_t SPEED_X_MIDDLE_LAST_TURN = 262 - MEDIUM_SHIFT;
    const uint16_t SPEED_X_END_LAST_TURN = 280 - MEDIUM_SHIFT;
    const uint16_t SPEED_X_END = SPEED_X_START;
    const uint16_t RELATIVE_VALUE = SPEED_X_START + 400;
    const uint16_t POWER_X_START = 400;
    const uint16_t POWER_X_START_FIRST_TURN = RELATIVE_VALUE - SPEED_X_START_FIRST_TURN;
    const uint16_t POWER_X_MIDDLE_FIRST_TURN = RELATIVE_VALUE - SPEED_X_MIDDLE_FIRST_TURN;
    const uint16_t POWER_X_END_FIRST_TURN = RELATIVE_VALUE - SPEED_X_END_FIRST_TURN;
    const uint16_t POWER_X_START_SECOND_TURN = RELATIVE_VALUE - SPEED_X_START_SECOND_TURN;
    const uint16_t POWER_MIDDLE_X = RELATIVE_VALUE - SPEED_MIDDLE_X;
    const uint16_t POWER_X_END_SECOND_TURN = RELATIVE_VALUE - SPEED_X_END_SECOND_TURN;
    const uint16_t POWER_X_START_LAST_TURN = RELATIVE_VALUE - SPEED_X_START_LAST_TURN;
    const uint16_t POWER_X_MIDDLE_LAST_TURN = RELATIVE_VALUE - SPEED_X_MIDDLE_LAST_TURN;
    const uint16_t POWER_X_END_LAST_TURN = RELATIVE_VALUE - SPEED_X_END_LAST_TURN;
    const uint16_t POWER_X_END = POWER_X_START;
    const uint16_t Y_START = 416;
    const uint16_t Y_START_FIRST_TURN = 416;
    const uint16_t Y_MIDDLE_FIRST_TURN = 412;
    const uint16_t Y_END_FIRST_TURN = 403;
    const uint16_t Y_START_SECOND_TURN = 228;
    const uint16_t Y_MIDDLE = 215;
    const uint16_t Y_END_SECOND_TURN = 200;
    const uint16_t Y_START_LAST_TURN = 27;
    const uint16_t Y_MIDDLE_LAST_TURN = 18;
    const uint16_t Y_END_LAST_TURN = 14;
    const uint16_t Y_END = 14;
    const float SPEED_Z_ANGLE_START = 90;
    const float SPEED_Z_ANGLE_LOWER = 150;
    const float SPEED_Z_ANGLE_UPPER = 210;
    const float SPEED_Z_ANGLE_END = 270;
    const float POWER_Z_ANGLE_START = 90;
    const float POWER_Z_ANGLE_LOWER = 30;
    const float POWER_Z_ANGLE_UPPER = -30;
    const float POWER_Z_ANGLE_END = -90;
    uint16_t x;
    uint16_t y;
    float z;
    int width;
    int height;
    const float SPEED_NO_ARC = 204.8f;
    const float SPEED_START_ARC = 207.5f;
    const float SPEED_END_ARC = 332.5f;
    const float SPEED_FULL_ARC = 335.2f;
    const float POWER_NO_ARC = 155.2f;
    const float POWER_START_ARC = 152.5f;
    const float POWER_END_ARC = 27.5f;
    const float POWER_FULL_ARC = 24.8f;
    int relativeSpeed;
    int relativePower;
    uint8_t lastSpeedNum;
    uint8_t lastPowerNum;
    BitmapId blueGaugeId;
    BitmapId greenGaugeId;
    BitmapId orangeGaugeId;
    PainterRGB888Bitmap blueGaugePainter;
    PainterRGB888Bitmap greenGaugePainter;
    PainterRGB888Bitmap orangeGaugePainter;
    bool orangeGauge;
    bool isLight;
};

#endif // GAUGEMEDIUM_HPP
