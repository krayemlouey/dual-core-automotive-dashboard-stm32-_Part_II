#include <gui/containers/GaugeMedium.hpp>
#include <BitmapDatabase.hpp>
#include <images/SVGDatabase.hpp>
#include <touchgfx/Bitmap.hpp>
#include <gui/common/Constants.hpp>
#include <math.h>

GaugeMedium::GaugeMedium() :
    x(0),
    y(0),
    z(0.0f),
    width(0),
    height(0),
    relativeSpeed(250),
    relativePower(160),
    lastSpeedNum(0),
    lastPowerNum(0),
    blueGaugeId(0),
    greenGaugeId(0),
    orangeGaugeId(0),
    orangeGauge(false),
    isLight(false)
{
    // Set dark mode gauges
    // Decompress left blue gauge to RAM and set painter
    Bitmap::decompress(BITMAP_GAUGE_FG_BLUE_ID);
    blueGaugePainter.setBitmap(Bitmap(BITMAP_GAUGE_FG_BLUE_ID));
    gaugeFGLeft.setPainter(blueGaugePainter);
    // Decompress right green gauge to RAM and set painter
    Bitmap::decompress(BITMAP_GAUGE_FG_GREEN_ID);
    greenGaugePainter.setBitmap(Bitmap(BITMAP_GAUGE_FG_GREEN_ID));
    greenGaugePainter.setOffset(gaugeBGLeft.getX() - gaugeBGRight.getX(), 0);
    gaugeFGRight.setPainter(greenGaugePainter);
    // Decompress right orange gauge to RAM
    Bitmap::decompress(BITMAP_GAUGE_FG_ORANGE_ID);
    orangeGaugePainter.setBitmap(Bitmap(BITMAP_GAUGE_FG_ORANGE_ID));
    orangeGaugePainter.setOffset(gaugeBGLeft.getX() - gaugeBGRight.getX(), 0);

    // Set light mode gauges
    Bitmap::decompress(BITMAP_GAUGE_FG_BLUE_LIGHT_ID);      // Decompress dark blue gauge to RAM
    Bitmap::decompress(BITMAP_GAUGE_FG_GREEN_LIGHT_ID);     // Decompress light green gauge to RAM
    Bitmap::decompress(BITMAP_GAUGE_FG_ORANGE_LIGHT_ID);    // Decompress light orange gauge to RAM
}

GaugeMedium::~GaugeMedium()
{
    Bitmap::dynamicBitmapDelete(BITMAP_GAUGE_FG_BLUE_ID);
    Bitmap::dynamicBitmapDelete(BITMAP_GAUGE_FG_GREEN_ID);
    Bitmap::dynamicBitmapDelete(BITMAP_GAUGE_FG_ORANGE_ID);
    Bitmap::dynamicBitmapDelete(BITMAP_GAUGE_FG_BLUE_LIGHT_ID);
    Bitmap::dynamicBitmapDelete(BITMAP_GAUGE_FG_GREEN_LIGHT_ID);
    Bitmap::dynamicBitmapDelete(BITMAP_GAUGE_FG_ORANGE_LIGHT_ID);
}

void GaugeMedium::initialize()
{
    GaugeMediumBase::initialize();
}

void GaugeMedium::setSpeed(float speed)
{
    // Left gauge now acts as GEAR indicator
    int gear = 0;
    if (speed > 0 && speed <= 50) gear = 1;
    else if (speed > 50 && speed <= 100) gear = 2;
    else if (speed > 100 && speed <= 150) gear = 3;
    else if (speed > 150 && speed <= 200) gear = 4;
    else if (speed > 200) gear = 5;

    // Left gauge needle fills smoothly 0-500
    relativeSpeed = (int)(speed * 2.0f);

    if (lastSpeedNum != gear)
    {
        // Update value
        Unicode::snprintf(speedTextBuffer, SPEEDTEXT_SIZE, "%d", gear);
        Rect rect(0, 28, speedText.getWidth(), 110);
        speedText.invalidateRect(rect);
        lastSpeedNum = gear;
    }

    // Update indicator
    if (relativeSpeed <= 50)
    {
        x = (int)round(SPEED_X_START - 1.4 * relativeSpeed);
        y = Y_START;
        z = SPEED_Z_ANGLE_START;

        indicatorSpeed.invalidate();
        indicatorSpeed.setXY(x, y);
        indicatorSpeed.setRotation(z);
        indicatorSpeed.invalidate();
    }
    else if (relativeSpeed <= 60)
    {
        x = (int)round(SPEED_X_START_FIRST_TURN - (SPEED_X_START_FIRST_TURN - SPEED_X_MIDDLE_FIRST_TURN) / 10.0f * (relativeSpeed - 50));
        y = (int)round(Y_START_FIRST_TURN - (Y_START_FIRST_TURN - Y_MIDDLE_FIRST_TURN) / 10.0f * (relativeSpeed - 50));
        z = SPEED_Z_ANGLE_START + ((SPEED_Z_ANGLE_LOWER - SPEED_Z_ANGLE_START) / 20.0f) * (relativeSpeed - 50);

        indicatorSpeed.invalidate();
        indicatorSpeed.setXY(x, y);
        indicatorSpeed.setRotation(z);
        indicatorSpeed.invalidate();
    }
    else if (relativeSpeed <= 70)
    {
        x = (int)round(SPEED_X_MIDDLE_FIRST_TURN - (SPEED_X_MIDDLE_FIRST_TURN - SPEED_X_END_FIRST_TURN) / 10.0f * (relativeSpeed - 60));
        y = (int)round(Y_MIDDLE_FIRST_TURN - (Y_MIDDLE_FIRST_TURN - Y_END_FIRST_TURN) / 10.0f * (relativeSpeed - 60));
        z = SPEED_Z_ANGLE_START + ((SPEED_Z_ANGLE_LOWER - SPEED_Z_ANGLE_START) / 20.0f) * (relativeSpeed - 50);

        indicatorSpeed.invalidate();
        indicatorSpeed.setXY(x, y);
        indicatorSpeed.setRotation(z);
        indicatorSpeed.invalidate();
    }
    else if (relativeSpeed <= 240)
    {
        x = (int)round(SPEED_X_END_FIRST_TURN - ((SPEED_X_END_FIRST_TURN - SPEED_X_START_SECOND_TURN) / 170.0f * (relativeSpeed - 70)));
        y = (int)round(Y_END_FIRST_TURN - ((Y_END_FIRST_TURN - Y_START_SECOND_TURN) / 170.0f * (relativeSpeed - 70)));
        z = SPEED_Z_ANGLE_LOWER;

        indicatorSpeed.invalidateContent();
        indicatorSpeed.setXY(x, y);
        indicatorSpeed.setRotation(z);
        indicatorSpeed.invalidateContent();
    }
    else if (relativeSpeed <= 250)
    {
        x = (uint16_t)round(SPEED_X_START_SECOND_TURN - ((SPEED_X_START_SECOND_TURN - SPEED_MIDDLE_X) / 10.0f * (relativeSpeed - 240)));
        y = (uint16_t)round(Y_START_SECOND_TURN - ((Y_START_SECOND_TURN - Y_MIDDLE) / 10.0f * (relativeSpeed - 240)));
        z = SPEED_Z_ANGLE_LOWER + ((SPEED_Z_ANGLE_UPPER - SPEED_Z_ANGLE_LOWER) / 20.0f) * (relativeSpeed - 240);

        indicatorSpeed.invalidateContent();
        indicatorSpeed.setXY(x, y);
        indicatorSpeed.setRotation(z);
        indicatorSpeed.invalidateContent();
    }
    else if (relativeSpeed <= 260)
    {
        x = (uint16_t)round(SPEED_MIDDLE_X - ((SPEED_MIDDLE_X - SPEED_X_END_SECOND_TURN) / 10.0f * (relativeSpeed - 250)));
        y = (uint16_t)round(Y_MIDDLE - ((Y_MIDDLE - Y_END_SECOND_TURN) / 10.0f * (relativeSpeed - 250)));
        z = SPEED_Z_ANGLE_LOWER + ((SPEED_Z_ANGLE_UPPER - SPEED_Z_ANGLE_LOWER) / 20.0f) * (relativeSpeed - 240);

        indicatorSpeed.invalidateContent();
        indicatorSpeed.setXY(x, y);
        indicatorSpeed.setRotation(z);
        indicatorSpeed.invalidateContent();
    }
    else if (relativeSpeed <= 430)
    {
        x = (int)round(SPEED_X_END_SECOND_TURN + ((SPEED_X_START_LAST_TURN - SPEED_X_END_SECOND_TURN) / 170.0f * (relativeSpeed - 260)));
        y = (int)round(Y_END_SECOND_TURN + ((Y_START_LAST_TURN - Y_END_SECOND_TURN) / 170.0f * (relativeSpeed - 260)));
        z = SPEED_Z_ANGLE_UPPER;

        indicatorSpeed.invalidateContent();
        indicatorSpeed.setXY(x, y);
        indicatorSpeed.setRotation(z);
        indicatorSpeed.invalidateContent();
    }
    else if (relativeSpeed <= 440)
    {
        x = (int)round(SPEED_X_START_LAST_TURN - (SPEED_X_START_LAST_TURN - SPEED_X_MIDDLE_LAST_TURN) / 10.0f * (relativeSpeed - 430));
        y = (int)round(Y_START_LAST_TURN - (Y_START_LAST_TURN - Y_MIDDLE_LAST_TURN) / 10.0f * (relativeSpeed - 430));
        z = SPEED_Z_ANGLE_UPPER + ((SPEED_Z_ANGLE_END - SPEED_Z_ANGLE_UPPER) / 20.0f) * (relativeSpeed - 430);

        indicatorSpeed.invalidateContent();
        indicatorSpeed.setXY(x, y);
        indicatorSpeed.setRotation(z);
        indicatorSpeed.invalidateContent();
    }
    else if (relativeSpeed <= 450)
    {
        x = (int)round(SPEED_X_MIDDLE_LAST_TURN - (SPEED_X_MIDDLE_LAST_TURN - SPEED_X_END_LAST_TURN) / 10.0f * (relativeSpeed - 440));
        y = (int)round(Y_MIDDLE_LAST_TURN - (Y_MIDDLE_LAST_TURN - Y_END_LAST_TURN) / 10.0f * (relativeSpeed - 440));
        z = SPEED_Z_ANGLE_UPPER + ((SPEED_Z_ANGLE_END - SPEED_Z_ANGLE_UPPER) / 20.0f) * (relativeSpeed - 430);

        indicatorSpeed.invalidateContent();
        indicatorSpeed.setXY(x, y);
        indicatorSpeed.setRotation(z);
        indicatorSpeed.invalidateContent();
    }
    else if (relativeSpeed <= 500)
    {
        x = (int)round(SPEED_X_END - 1.4 * (500 - relativeSpeed));
        y = Y_END;
        z = SPEED_Z_ANGLE_END;

        indicatorSpeed.invalidateContent();
        indicatorSpeed.setXY(x, y);
        indicatorSpeed.setRotation(z);
        indicatorSpeed.invalidateContent();
    }

    // Update gauge
    updateSpeedGauge();
}

void GaugeMedium::setPower(float power)
{
    // Right gauge now acts as SPEED indicator
    float visualPower = power * (30.0f / 250.0f); // Map 0-250 input to 0-30
    relativePower = (int)(visualPower * 10); // Max 300

    if (lastPowerNum != round(power))
    {
        // Update value
        Unicode::snprintf(powerTextBuffer, POWERTEXT_SIZE, "%d", (int)round(power));
        Rect rect(0, 28, powerText.getWidth(), 110);
        powerText.invalidateRect(rect);
        lastPowerNum = (uint8_t)round(power);
    }

    // Update indicator
    if (relativePower <= 30)
    {
        x = (int)round(POWER_X_START + 1.4 * (50.0 / 30.0) * relativePower);
        y = Y_START;
        z = POWER_Z_ANGLE_START;

        indicatorPower.invalidate();
        indicatorPower.setXY(x, y);
        indicatorPower.setRotation(z);
        indicatorPower.invalidate();
    }
    else if (relativePower <= 36)
    {
        x = (int)round(POWER_X_START_FIRST_TURN + (POWER_X_MIDDLE_FIRST_TURN - POWER_X_START_FIRST_TURN) / 6.0f * (relativePower - 30));
        y = (int)round(Y_START_FIRST_TURN - (Y_START_FIRST_TURN - Y_MIDDLE_FIRST_TURN) / 6.0f * (relativePower - 30));
        z = POWER_Z_ANGLE_START - ((POWER_Z_ANGLE_START - POWER_Z_ANGLE_LOWER) / 12.0f) * (relativePower - 30);

        indicatorPower.invalidate();
        indicatorPower.setXY(x, y);
        indicatorPower.setRotation(z);
        indicatorPower.invalidate();
    }
    else if (relativePower <= 42)
    {
        x = (int)round(POWER_X_MIDDLE_FIRST_TURN + (POWER_X_END_FIRST_TURN - POWER_X_MIDDLE_FIRST_TURN) / 6.0f * (relativePower - 36));
        y = (int)round(Y_MIDDLE_FIRST_TURN - (Y_MIDDLE_FIRST_TURN - Y_END_FIRST_TURN) / 6.0f * (relativePower - 36));
        z = POWER_Z_ANGLE_START - ((POWER_Z_ANGLE_START - POWER_Z_ANGLE_LOWER) / 12.0f) * (relativePower - 30);

        indicatorPower.invalidate();
        indicatorPower.setXY(x, y);
        indicatorPower.setRotation(z);
        indicatorPower.invalidate();
    }
    else if (relativePower <= 144)
    {
        x = (int)round(POWER_X_END_FIRST_TURN + ((POWER_X_START_SECOND_TURN - POWER_X_END_FIRST_TURN) / 102.0f * (relativePower - 42)));
        y = (int)round(Y_END_FIRST_TURN - ((Y_END_FIRST_TURN - Y_START_SECOND_TURN) / 102.0f * (relativePower - 42)));
        z = POWER_Z_ANGLE_LOWER;

        indicatorPower.invalidateContent();
        indicatorPower.setXY(x, y);
        indicatorPower.setRotation(z);
        indicatorPower.invalidateContent();
    }
    else if (relativePower <= 150)
    {
        x = (int)round(POWER_X_START_SECOND_TURN + ((POWER_MIDDLE_X - POWER_X_START_SECOND_TURN) / 6.0f * (relativePower - 144)));
        y = (int)round(Y_START_SECOND_TURN - ((Y_START_SECOND_TURN - Y_MIDDLE) / 6.0f * (relativePower - 144)));
        z = POWER_Z_ANGLE_LOWER + ((POWER_Z_ANGLE_UPPER - POWER_Z_ANGLE_LOWER) / 12.0f) * (relativePower - 144);

        indicatorPower.invalidateContent();
        indicatorPower.setXY(x, y);
        indicatorPower.setRotation(z);
        indicatorPower.invalidateContent();
    }
    else if (relativePower <= 156)
    {
        x = (int)round(POWER_MIDDLE_X - ((POWER_MIDDLE_X - POWER_X_END_SECOND_TURN) / 6.0f * (relativePower - 150)));
        y = (int)round(Y_MIDDLE - ((Y_MIDDLE - Y_END_SECOND_TURN) / 6.0f * (relativePower - 150)));
        z = POWER_Z_ANGLE_LOWER + ((POWER_Z_ANGLE_UPPER - POWER_Z_ANGLE_LOWER) / 12.0f) * (relativePower - 144);

        indicatorPower.invalidateContent();
        indicatorPower.setXY(x, y);
        indicatorPower.setRotation(z);
        indicatorPower.invalidateContent();
    }
    else if (relativePower <= 258)
    {
        x = (int)round(POWER_X_END_SECOND_TURN - ((POWER_X_END_SECOND_TURN - POWER_X_START_LAST_TURN) / 102.0f * (relativePower - 156)));
        y = (int)round(Y_END_SECOND_TURN + ((Y_START_LAST_TURN - Y_END_SECOND_TURN) / 102.0f * (relativePower - 156)));
        z = POWER_Z_ANGLE_UPPER;

        indicatorPower.invalidateContent();
        indicatorPower.setXY(x, y);
        indicatorPower.setRotation(z);
        indicatorPower.invalidateContent();
    }
    else if (relativePower <= 264)
    {
        x = (int)round(POWER_X_START_LAST_TURN - (POWER_X_START_LAST_TURN - POWER_X_MIDDLE_LAST_TURN) / 6.0f * (relativePower - 258));
        y = (int)round(Y_START_LAST_TURN - (Y_START_LAST_TURN - Y_MIDDLE_LAST_TURN) / 6.0f * (relativePower - 258));
        z = POWER_Z_ANGLE_UPPER + ((POWER_Z_ANGLE_END - POWER_Z_ANGLE_UPPER) / 12.0f) * (relativePower - 258);

        indicatorPower.invalidateContent();
        indicatorPower.setXY(x, y);
        indicatorPower.setRotation(z);
        indicatorPower.invalidateContent();
    }
    else if (relativePower <= 270)
    {
        x = (int)round(POWER_X_MIDDLE_LAST_TURN - (POWER_X_MIDDLE_LAST_TURN - POWER_X_END_LAST_TURN) / 6.0f * (relativePower - 264));
        y = (int)round(Y_MIDDLE_LAST_TURN - (Y_MIDDLE_LAST_TURN - Y_END_LAST_TURN) / 6.0f * (relativePower - 264));
        z = POWER_Z_ANGLE_UPPER + ((POWER_Z_ANGLE_END - POWER_Z_ANGLE_UPPER) / 12.0f) * (relativePower - 258);

        indicatorPower.invalidate();
        indicatorPower.setXY(x, y);
        indicatorPower.setRotation(z);
        indicatorPower.invalidate();
    }
    else if (relativePower <= 300)
    {
        x = (int)round(POWER_X_END + 1.4 * (50.0 / 30.0) * (300 - relativePower));
        y = Y_END;
        z = POWER_Z_ANGLE_END;

        indicatorPower.invalidate();
        indicatorPower.setXY(x, y);
        indicatorPower.setRotation(z);
        indicatorPower.invalidate();
    }

    // Update gauge
    updatePowerGauge();
}

bool GaugeMedium::isOrange()
{
    return orangeGauge;
}

void GaugeMedium::setColor(int BGcolor)
{
    float speedRotation = indicatorSpeed.getRotation();
    float powerRotation = indicatorPower.getRotation();

    isLight = BGcolor > Constants::DARK_BG_COLOR ? true : false;

    if (isLight)
    {
        gaugeBGLeft.setSVG(SVG_GAUGE_BASE_LIGHT_ID);
        gaugeBGRight.setSVG(SVG_GAUGE_BASE_LIGHT_ID);
        gaugeBGEndTopLeft.setSVG(SVG_GAUGE_BASE_END_LIGHT_ID);
        gaugeBGEndTopRight.setSVG(SVG_GAUGE_BASE_END_LIGHT_ID);
        gaugeFGEndBottomLeft.setSVG(SVG_GAUGE_BLUE_END_LIGHT_ID);
        gaugeFGEndBottomRightGreen.setSVG(SVG_GAUGE_GREEN_END_LIGHT_ID);
        gaugeFGEndBottomRightOrange.setSVG(SVG_GAUGE_ORANGE_END_LIGHT_ID);
        gaugeBGDarkTopLeft.setColor(0xACC7E5);
        gaugeBGLightTopLeft.setColor(Constants::LIGHT_GREY_COLOR);
        gaugeFGDarkBottomLeft.setColor(0x84C1E8);
        gaugeFGLightBottomLeft.setColor(Constants::LIGHT_BLUE_COLOR);
        gaugeBGDarkTopRight.setColor(0xACC7E5);
        gaugeBGLightTopRight.setColor(Constants::LIGHT_GREY_COLOR);
        gaugeFGDarkBottomRight.setColor(orangeGauge ? 0xC9BFAB : 0x84C9C2);
        gaugeFGLightBottomRight.setColor(orangeGauge ? Constants::LIGHT_ORANGE_COLOR : Constants::LIGHT_GREEN_COLOR);

        gaugeFill1.setColor(Constants::LIGHT_BG_COLOR);
        gaugeFill2.setColor(Constants::LIGHT_BG_COLOR);

        topDivider.setColor(0xACC7E5);
        lowerDivider.setColor(0xACC7E5);

        indicatorSpeed.setSVG(SVG_INDICATOR_LIGHT_ID);
        indicatorPower.setSVG(SVG_INDICATOR_LIGHT_ID);

        speedText.setColor(Constants::LIGHT_BLUE_COLOR);
        orangeGauge ? powerText.setColor(Constants::LIGHT_ORANGE_COLOR) : powerText.setColor(Constants::DARK_GREEN_COLOR);
        speedLabel.setColor(Constants::LIGHT_MAIN_TEXTCOLOR);
        powerLabel.setColor(Constants::LIGHT_MAIN_TEXTCOLOR);
        speedLabel0.setColor(Constants::LIGHT_GREY_COLOR);
        speedLabel25.setColor(Constants::LIGHT_GREY_COLOR);
        speedLabel50.setColor(Constants::LIGHT_GREY_COLOR);
        powerLabel0.setColor(Constants::LIGHT_GREY_COLOR);
        powerLabel15.setColor(Constants::LIGHT_GREY_COLOR);
        powerLabel30.setColor(Constants::LIGHT_GREY_COLOR);

        blueGaugePainter.setBitmap(Bitmap(BITMAP_GAUGE_FG_BLUE_LIGHT_ID));
        greenGaugePainter.setBitmap(Bitmap(BITMAP_GAUGE_FG_GREEN_LIGHT_ID));
        orangeGaugePainter.setBitmap(Bitmap(BITMAP_GAUGE_FG_ORANGE_LIGHT_ID));
    }
    else
    {
        gaugeBGLeft.setSVG(SVG_GAUGE_BASE_ID);
        gaugeBGRight.setSVG(SVG_GAUGE_BASE_ID);
        gaugeBGEndTopLeft.setSVG(SVG_GAUGE_BASE_END_ID);
        gaugeBGEndTopRight.setSVG(SVG_GAUGE_BASE_END_ID);
        gaugeFGEndBottomLeft.setSVG(SVG_GAUGE_BLUE_END_ID);
        gaugeFGEndBottomRightGreen.setSVG(SVG_GAUGE_GREEN_END_ID);
        gaugeFGEndBottomRightOrange.setSVG(SVG_GAUGE_ORANGE_END_ID);
        gaugeBGDarkTopLeft.setColor(0x0E2034);
        gaugeBGLightTopLeft.setColor(Constants::DARK_GREY_COLOR);
        gaugeFGDarkBottomLeft.setColor(0x0B415D);
        gaugeFGLightBottomLeft.setColor(Constants::DARK_BLUE_COLOR);
        gaugeBGDarkTopRight.setColor(0x0E2034);
        gaugeBGLightTopRight.setColor(Constants::DARK_GREY_COLOR);
        gaugeFGDarkBottomRight.setColor(orangeGauge ? 0x3E372E : 0x0B4D42);
        gaugeFGLightBottomRight.setColor(orangeGauge ? Constants::DARK_ORANGE_COLOR : Constants::DARK_GREEN_COLOR);

        gaugeFill1.setColor(Constants::DARK_BG_COLOR);
        gaugeFill2.setColor(Constants::DARK_BG_COLOR);

        topDivider.setColor(0x0E2034);
        lowerDivider.setColor(0x0E2034);

        indicatorSpeed.setSVG(SVG_INDICATOR_ID);
        indicatorPower.setSVG(SVG_INDICATOR_ID);

        speedText.setColor(Constants::DARK_BLUE_COLOR);
        orangeGauge ? powerText.setColor(Constants::DARK_ORANGE_COLOR) : powerText.setColor(Constants::DARK_GREEN_COLOR);
        speedLabel.setColor(Constants::DARK_MAIN_TEXTCOLOR);
        powerLabel.setColor(Constants::DARK_MAIN_TEXTCOLOR);
        speedLabel0.setColor(Constants::DARK_GREY_COLOR);
        speedLabel25.setColor(Constants::DARK_GREY_COLOR);
        speedLabel50.setColor(Constants::DARK_GREY_COLOR);
        powerLabel0.setColor(Constants::DARK_GREY_COLOR);
        powerLabel15.setColor(Constants::DARK_GREY_COLOR);
        powerLabel30.setColor(Constants::DARK_GREY_COLOR);

        blueGaugePainter.setBitmap(Bitmap(BITMAP_GAUGE_FG_BLUE_ID));
        greenGaugePainter.setBitmap(Bitmap(BITMAP_GAUGE_FG_GREEN_ID));
        orangeGaugePainter.setBitmap(Bitmap(BITMAP_GAUGE_FG_ORANGE_ID));
    }

    gaugeBGRight.setRotationCenter(75, 218);
    gaugeBGRight.setRotation(180);
    gaugeBGEndTopRight.setScaleX(-1);
    gaugeBGEndTopRight.setImagePosition(14, 0);
    indicatorSpeed.setImagePosition(3, 14);
    indicatorSpeed.setRotationCenter(25, 25);
    indicatorSpeed.setRotation(speedRotation);
    indicatorPower.setImagePosition(3, 14);
    indicatorPower.setRotationCenter(25, 25);
    indicatorPower.setRotation(powerRotation);
}

void GaugeMedium::updateTextAreas()
{
    speedLabel.setTypedText(speedLabel.getTypedText());
    powerLabel.setTypedText(powerLabel.getTypedText());
    speedLabel0.setTypedText(speedLabel0.getTypedText());
    speedLabel25.setTypedText(speedLabel25.getTypedText());
    speedLabel50.setTypedText(speedLabel50.getTypedText());
    powerLabel0.setTypedText(powerLabel0.getTypedText());
    powerLabel15.setTypedText(powerLabel15.getTypedText());
    powerLabel30.setTypedText(powerLabel30.getTypedText());

    speedLabel.invalidate();
    powerLabel.invalidate();
    speedLabel0.invalidate();
    speedLabel25.invalidate();
    speedLabel50.invalidate();
    powerLabel0.invalidate();
    powerLabel15.invalidate();
    powerLabel30.invalidate();
}

void GaugeMedium::setOrangeGauge()
{
    orangeGauge = true;

    powerText.setColor(isLight ? Constants::LIGHT_ORANGE_COLOR : Constants::DARK_ORANGE_COLOR);
    Rect rect(0, 28, powerText.getWidth(), 110);
    powerText.invalidateRect(rect);

    gaugeFGDarkBottomRight.setColor(isLight ? 0xC9BFAB : 0x3E372E);
    gaugeFGLightBottomRight.setColor(isLight ? 0xE58E12 : 0xFF9518);
    gaugeFGBottomRight.invalidateContent();

    gaugeFGEndBottomRightGreen.setVisible(false);
    gaugeFGEndBottomRightOrange.setVisible(true);

    gaugeFGRight.setPainter(orangeGaugePainter);
    gaugeFGRight.invalidateContent();
}

void GaugeMedium::setGreenGauge()
{
    orangeGauge = false;

    powerText.setColor(isLight ? Constants::LIGHT_GREEN_COLOR : Constants::DARK_GREEN_COLOR);
    Rect rect(0, 28, powerText.getWidth(), 110);
    powerText.invalidateRect(rect);

    gaugeFGDarkBottomRight.setColor(isLight ? 0x84C9C2 : 0x0B4D42);
    gaugeFGLightBottomRight.setColor(isLight ? 0x00B05F : Constants::DARK_GREEN_COLOR);
    gaugeFGBottomRight.invalidateContent();

    gaugeFGEndBottomRightGreen.setVisible(true);
    gaugeFGEndBottomRightOrange.setVisible(false);

    gaugeFGRight.setPainter(greenGaugePainter);
    gaugeFGRight.invalidateContent();
}

void GaugeMedium::updateSpeedGauge()
{
    if (relativeSpeed <= 50)
    {
        x = indicatorSpeed.getX() + (indicatorSpeed.getWidth() / 2);
        y = gaugeFGBottomLeft.getY();
        width = (385 - indicatorSpeed.getX()) - (indicatorSpeed.getWidth() / 2);
        height = gaugeFGBottomLeft.getHeight();

        gaugeFGBottomLeft.setXY(x, y);
        gaugeFGBottomLeft.setWidthHeight(width, height);
        gaugeFGTopLeft.setWidth(0);
        gaugeFGLeft.setArc(SPEED_NO_ARC, SPEED_NO_ARC);
    }
    else if (relativeSpeed <= 60)
    {
        x = gaugeBGBottomLeft.getX();
        y = gaugeBGBottomLeft.getY();
        width = gaugeBGBottomLeft.getWidth() + 1;
        height = gaugeBGBottomLeft.getHeight();

        gaugeFGBottomLeft.setXY(x, y);
        gaugeFGBottomLeft.setWidthHeight(width, height);
        gaugeFGTopLeft.setWidth(0);
        gaugeFGLeft.setArc(SPEED_NO_ARC, SPEED_START_ARC + (relativeSpeed - 50) * (SPEED_END_ARC - SPEED_START_ARC) / 400.f);
    }
    else if (relativeSpeed <= 449)
    {
        width = indicatorSpeed.getX() + (int)(indicatorSpeed.getWidth() / 2.0) - gaugeBGTopLeft.getX();

        gaugeFGBottomLeft.setWidth(gaugeBGBottomLeft.getWidth() + 1);
        gaugeFGTopLeft.setWidth(width);
        gaugeFGLeft.setArc(SPEED_NO_ARC, SPEED_START_ARC + (relativeSpeed - 50) * (SPEED_END_ARC - SPEED_START_ARC) / 400.f);
    }
    else if (relativeSpeed <= 500)
    {
        width = indicatorSpeed.getX() + (int)(indicatorSpeed.getWidth() / 2.0) - gaugeBGTopLeft.getX();

        gaugeFGTopLeft.setWidth(width);
        gaugeFGLeft.setArc(SPEED_NO_ARC, SPEED_FULL_ARC);
    }
}

void GaugeMedium::updatePowerGauge()
{
    if (relativePower > 220 && !orangeGauge)
    {
        setOrangeGauge();
    }
    else if (relativePower <= 220 && orangeGauge)
    {
        setGreenGauge();
    }

    if (relativePower <= 30)
    {
        width = indicatorPower.getX() + (indicatorPower.getWidth() / 2) - gaugeBGBottomRight.getX();

        gaugeFGBottomRight.setWidth(width);
        gaugeFGTopRight.setWidth(0);
        gaugeFGRight.setArc(POWER_NO_ARC, POWER_NO_ARC);
    }
    else if (relativePower <= 36)
    {
        width = gaugeBGBottomRight.getWidth() + 1;
        height = gaugeBGBottomRight.getHeight();

        gaugeFGBottomRight.setWidthHeight(width, height);
        gaugeFGTopRight.setWidth(0);
        gaugeFGRight.setArc(POWER_NO_ARC, POWER_START_ARC - (relativePower - 30) * (POWER_START_ARC - POWER_END_ARC) / 240.f);
    }
    else if (relativePower <= 269)
    {
        gaugeFGBottomRight.setWidth(gaugeBGBottomRight.getWidth() + 1);
        gaugeFGTopRight.setWidth(0);
        gaugeFGRight.setArc(POWER_NO_ARC, POWER_START_ARC + (relativePower - 30) * (POWER_END_ARC - POWER_START_ARC) / 240.f);
    }
    else if (relativePower <= 300)
    {
        x = indicatorPower.getX() + (indicatorPower.getWidth() / 2);
        width = gaugeBGTopRight.getX() + gaugeBGTopRight.getWidth() - x + 1;

        gaugeFGTopRight.setX(x);
        gaugeFGTopRight.setWidth(width);
        gaugeFGRight.setArc(POWER_NO_ARC, POWER_FULL_ARC);
    }
}
