#include <gui/dashboardmedium_screen/DashboardMediumView.hpp>
#include <images/BitmapDatabase.hpp>
#include <images/SVGDatabase.hpp>
#include <gui/common/Constants.hpp>
#include <touchgfx/Color.hpp>
#include <cstring>

DashboardMediumView::DashboardMediumView() :
    oldBPM(0), oldMinute(0), oldSecond(0), oldDistance(0),
    alreadyOrange(false), oldBitmapID(0),
    dynamicBmpLanguageSelector(0), languageSelectorMoving(false),
    oldLidar(-999.0f), oldTemp(-1), oldFuel(-1),
    oldAlertLevel(0xFF),
    ledBlinkCounter(0), ledBlinkState(false)
{
    oldAlertMsg[0] = '\0';
}

void DashboardMediumView::TriangleLed::setup(int16_t x, int16_t y, int16_t w, int16_t h)
{
    container.setPosition(x, y, w, h);
    painter.setColor(touchgfx::Color::getColorFromRGB(0, 0, 0)); /* Éteinte en noir au repos */

    float apexX      = w / 2.0f;
    float baseLeftX  = 1.0f;
    float baseRightX = (float)(w - 2);
    float baseY      = (float)(h - 2);

    /* Périmètre extérieur */
    left.setPosition(0, 0, w, h);
    left.setPainter(painter);
    left.setStart(apexX, 1.0f);
    left.setEnd(baseLeftX, baseY);
    left.setLineWidth(2.0f);
    container.add(left);

    right.setPosition(0, 0, w, h);
    right.setPainter(painter);
    right.setStart(apexX, 1.0f);
    right.setEnd(baseRightX, baseY);
    right.setLineWidth(2.0f);
    container.add(right);

    bottom.setPosition(0, 0, w, h);
    bottom.setPainter(painter);
    bottom.setStart(baseLeftX, baseY);
    bottom.setEnd(baseRightX, baseY);
    bottom.setLineWidth(2.0f);
    container.add(bottom);

    /* Périmètre intérieur (creux au centre) */
    float innerApexY      = 6.0f;
    float innerBaseLeftX  = 5.0f;
    float innerBaseRightX = (float)(w - 6);
    float innerBaseY      = (float)(h - 5);

    innerLeft.setPosition(0, 0, w, h);
    innerLeft.setPainter(painter);
    innerLeft.setStart(apexX, innerApexY);
    innerLeft.setEnd(innerBaseLeftX, innerBaseY);
    innerLeft.setLineWidth(1.5f);
    container.add(innerLeft);

    innerRight.setPosition(0, 0, w, h);
    innerRight.setPainter(painter);
    innerRight.setStart(apexX, innerApexY);
    innerRight.setEnd(innerBaseRightX, innerBaseY);
    innerRight.setLineWidth(1.5f);
    container.add(innerRight);

    innerBottom.setPosition(0, 0, w, h);
    innerBottom.setPainter(painter);
    innerBottom.setStart(innerBaseLeftX, innerBaseY);
    innerBottom.setEnd(innerBaseRightX, innerBaseY);
    innerBottom.setLineWidth(1.5f);
    container.add(innerBottom);
}

void DashboardMediumView::TriangleLed::setColor(touchgfx::colortype c)
{
    painter.setColor(c);
    left.invalidate();
    right.invalidate();
    bottom.invalidate();
    innerLeft.invalidate();
    innerRight.invalidate();
    innerBottom.invalidate();
}

void DashboardMediumView::setupScreen()
{
    switch (presenter->getSelectedButton())
    {
    case bpm:      gaugeButtonsGroup.setSelected(bpmButton);      bpmButtonClicked();      break;
    case distance: gaugeButtonsGroup.setSelected(distanceButton); distanceButtonClicked(); break;
    default: break;
    }
    updateTripInfo();
    updateTheme();
    updateBatteryLevel();
    if (presenter->getPower() > 22) { setOrangeBattery(); gauge.setOrangeGauge(); }
    else                            { setGreenBattery();  gauge.setGreenGauge();  }
    languageSelector.setTouchable(true);
    gauge.setSpeed(presenter->getSpeed());
    gauge.setPower(presenter->getSpeed());
    presenter->setSelectedScreenSize(mediumSize);
    dynamicBmpLanguageSelector = Bitmap::dynamicBitmapCreate(languageSelector.getWidth(), languageSelector.getHeight(), Bitmap::RGB888);
    languageSelector.setCacheBitmap(dynamicBmpLanguageSelector);
    languageSelector.enableCachedMode(true);
    languageSelector.updateCache();
    DashboardMediumViewBase::setupScreen();

    /* ── Les icônes température (image4) et carburant (image3) restent BIEN VISIBLES ── */
    image3.setVisible(true);
    image3.invalidate();
    image4.setVisible(true);
    image4.invalidate();

    /* ── Masquer les deux boutons coeur (bpmButton) et position (distanceButton) ── */
    bpmButton.setVisible(false);
    bpmButton.invalidate();
    distanceButton.setVisible(false);
    distanceButton.invalidate();

    /* ── Widget LiDAR ───────────────────────────────────────────── */
    lidarValue.setPosition(383, 324, 140, 90);
    lidarValue.setColor(touchgfx::Color::getColorFromRGB(181, 215, 253));
    lidarValue.setTypedText(bpmValue.getTypedText());
    lidarValue.setWildcard(lidarValueBuffer);
    add(lidarValue);

    lidarValueBuffer[0] = '-'; lidarValueBuffer[1] = '-';
    lidarValueBuffer[2] = '-'; lidarValueBuffer[3] = 0;
    lidarValue.invalidate();

    /* ── Zone d'alerte Jetson Nano ──────────────────────────────────
     * Position : à gauche de l'écriture LiDAR.
     * Dimensions agrandies en hauteur : 114 x 46 px (rectangle haut).
     * Ne touche pas le LiDAR (LiDAR à X=383, alertBox finit à X=348).
     * Ne touche pas la boîte de vitesse ni la jauge.               */
    alertBox.setPosition(234, 348, 114, 46);
    alertBox.setColor(touchgfx::Color::getColorFromRGB(25, 35, 45));
    add(alertBox);

    alertText.setPosition(234, 356, 114, 30);
    alertText.setColor(touchgfx::Color::getColorFromRGB(160, 190, 220));
    alertText.setTypedText(touchgfx::TypedText(T_DASHBOARDMEDIUMDIST));
    alertText.setWildcard(alertTextBuffer);

    const char* initMsg = "JETSON";
    int idx = 0;
    while (initMsg[idx] && idx < (int)(ALERTTEXT_SIZE - 1)) {
        alertTextBuffer[idx] = (touchgfx::Unicode::UnicodeChar)initMsg[idx];
        idx++;
    }
    alertTextBuffer[idx] = 0;
    add(alertText);
    alertBox.invalidate();
    alertText.invalidate();

    /* ── Les 3 LEDs triangulaires (double périmètre, creux au milieu) ──
     * 1. ledTemp  : sous la jauge de température (X=77, Y=452, 28x24)
     * 2. ledFuel  : sous la jauge de carburant   (X=700, Y=452, 28x24)
     * 3. ledSpeed : au-dessus, entre 0 et 250    (X=386, Y=56, 28x24)
     * Éteintes en noir au repos. Clignotent en rouge selon le problème critique. */
    ledTemp.setup(77, 452, 28, 24);
    add(ledTemp.container);
    ledTemp.container.invalidate();

    ledFuel.setup(700, 452, 28, 24);
    add(ledFuel.container);
    ledFuel.container.invalidate();

    ledSpeed.setup(386, 56, 28, 24);
    add(ledSpeed.container);
    ledSpeed.container.invalidate();
}

void DashboardMediumView::tearDownScreen()
{
    DashboardMediumViewBase::tearDownScreen();
    Bitmap::dynamicBitmapDelete(dynamicBmpLanguageSelector);
}

void DashboardMediumView::handleTickEvent()
{
    gauge.setSpeed(presenter->getSpeed());
    gauge.setPower(presenter->getSpeed());

    // Temperature
    int newTemp = (int)presenter->getTemperature();
    if (newTemp != oldTemp) {
        gaugetemp.setValue(newTemp);
        gaugetemp.invalidate();
        oldTemp = newTemp;
    }

    // LiDAR : affichage distance
    float newLidar = presenter->getLidar();
    if (newLidar >= 0.0f) {
        if ((int)newLidar != (int)oldLidar) {
            Unicode::snprintf(lidarValueBuffer, LIDARVALUE_SIZE, "%d", (int)newLidar);
            lidarValue.invalidate();
            oldLidar = newLidar;
        }
    } else {
        if (oldLidar >= 0.0f || lidarValueBuffer[0] != '-') {
            lidarValueBuffer[0] = '-';
            lidarValueBuffer[1] = '-';
            lidarValueBuffer[2] = '-';
            lidarValueBuffer[3] = 0;
            lidarValue.invalidate();
            oldLidar = -1.0f;
        }
    }

    // Fuel
    int newFuel = (int)presenter->getFuel();
    if (newFuel != oldFuel) {
        gaugefuel.setValue(newFuel);
        gaugefuel.invalidate();
        oldFuel = newFuel;
    }

    // ── Zone d'alerte Jetson Nano ─────────────────────────────────
    const char* newMsg   = presenter->getAlertText();
    uint8_t     newLevel = presenter->getAlertLevel();

    bool msgChanged   = (strncmp(newMsg, oldAlertMsg, 63) != 0);
    bool levelChanged = (newLevel != oldAlertLevel);

    if (msgChanged || levelChanged)
    {
        /* Convertir char* en UnicodeChar (ASCII only), avec affichage "JETSON" si vide */
        const char* displayMsg = (newMsg[0] != '\0') ? newMsg : "JETSON";
        int i = 0;
        while (displayMsg[i] && i < (int)(ALERTTEXT_SIZE - 1)) {
            alertTextBuffer[i] = (touchgfx::Unicode::UnicodeChar)displayMsg[i];
            i++;
        }
        alertTextBuffer[i] = 0;

        /* Couleur du fond selon le niveau d'alerte */
        if (newLevel == 2)
        {
            /* DANGER — rouge */
            alertBox.setColor(touchgfx::Color::getColorFromRGB(180, 20, 20));
            alertText.setColor(touchgfx::Color::getColorFromRGB(255, 255, 255));
        }
        else if (newLevel == 1)
        {
            /* WARNING — orange */
            alertBox.setColor(touchgfx::Color::getColorFromRGB(200, 100, 0));
            alertText.setColor(touchgfx::Color::getColorFromRGB(255, 240, 200));
        }
        else
        {
            /* OK / vide — gris discret */
            alertBox.setColor(touchgfx::Color::getColorFromRGB(30, 30, 30));
            alertText.setColor(touchgfx::Color::getColorFromRGB(180, 180, 180));
        }

        alertBox.invalidate();
        alertText.invalidate();

        strncpy(oldAlertMsg, newMsg, 63);
        oldAlertMsg[63] = '\0';
        oldAlertLevel   = newLevel;
    }

    /* ── Analyse autonome de la télémétrie véhicule (STM32) ──────────────────
     * 1. Vitesse > 180 km/h                -> Triangle vitesse clignote en rouge
     * 2. Température >= 75% (> 3/4 jauge)  -> Triangle température clignote en rouge
     * 3. Carburant <= 15% (proche de 0)    -> Triangle carburant clignote en rouge
     * 4. Alerte critique externe (level 2) -> Triangle associé clignote en rouge
     */
    float curSpeed = presenter->getSpeed();
    float curTemp  = presenter->getTemperature();
    float curFuel  = presenter->getFuel();

    bool warnSpeed = (curSpeed > 180.0f) || (newLevel == 2 && presenter->getAlertTarget() == 0);
    bool warnTemp  = (curTemp >= 75.0f)  || (newLevel == 2 && presenter->getAlertTarget() == 1);
    bool warnFuel  = (curFuel >= 0.0f && curFuel <= 15.0f) || (newLevel == 2 && presenter->getAlertTarget() == 2);

    bool anyWarn = warnSpeed || warnTemp || warnFuel;

    if (anyWarn)
    {
        ledBlinkCounter++;
        if (ledBlinkCounter >= 12) /* ~250ms à ~45-50fps */
        {
            ledBlinkCounter = 0;
            ledBlinkState = !ledBlinkState;
            touchgfx::colortype redColor   = touchgfx::Color::getColorFromRGB(255, 0, 0);
            touchgfx::colortype blackColor = touchgfx::Color::getColorFromRGB(0, 0, 0);

            ledSpeed.setColor((warnSpeed && ledBlinkState) ? redColor : blackColor);
            ledTemp.setColor((warnTemp   && ledBlinkState) ? redColor : blackColor);
            ledFuel.setColor((warnFuel   && ledBlinkState) ? redColor : blackColor);
        }
    }
    else
    {
        /* Hors statut critique : toutes les LEDs restent éteintes en noir */
        if (ledBlinkState || ledBlinkCounter != 0)
        {
            ledBlinkState = false;
            ledBlinkCounter = 0;
            touchgfx::colortype blackColor = touchgfx::Color::getColorFromRGB(0, 0, 0);
            ledTemp.setColor(blackColor);
            ledFuel.setColor(blackColor);
            ledSpeed.setColor(blackColor);
        }
    }

    if (gauge.isOrange() && !alreadyOrange)       setOrangeBattery();
    else if (!gauge.isOrange() && alreadyOrange)  setGreenBattery();
    updateBatteryLevel();
    updateTripInfo();
    if (languageSelectorMoving && !languageSelector.isMoveAnimationRunning())
    {
        languageSelectorMoving = false;
        languageSelector.enableCachedMode(false);
    }
}

void DashboardMediumView::bpmButtonClicked()
{
    presenter->setSelectedButton(bpm);
    bpmText.invalidate(); timeText.invalidate(); distanceText.invalidate();
    bpmText.setVisible(true);
    timeText.setVisible(false);
    distanceText.setVisible(false);
    bpmText.invalidate(); timeText.invalidate(); distanceText.invalidate();
}

void DashboardMediumView::timeButtonClicked()
{
    presenter->setSelectedButton(time);
    bpmText.invalidate(); timeText.invalidate(); distanceText.invalidate();
    bpmText.setVisible(false);
    timeText.setVisible(true);
    distanceText.setVisible(false);
    bpmText.invalidate(); timeText.invalidate(); distanceText.invalidate();
}

void DashboardMediumView::distanceButtonClicked()
{
    presenter->setSelectedButton(distance);
    bpmText.invalidate(); timeText.invalidate(); distanceText.invalidate();
    bpmText.setVisible(false);
    timeText.setVisible(false);
    distanceText.setVisible(true);
    bpmText.invalidate(); timeText.invalidate(); distanceText.invalidate();
}

void DashboardMediumView::weatherButtonClicked()
{
    // Weather screen not available in this project
}

void DashboardMediumView::modeButtonClicked()
{
    presenter->setLight(!presenter->getLight());
    updateTheme();
}

void DashboardMediumView::languageButtonClicked()
{
    languageSelectorMoving = true;
    languageSelector.enableCachedMode(true);
    languageSelector.updateCache();
    languageSelector.startMoveAnimation(396, 0, 40, EasingEquations::sineEaseInOut);
}

void DashboardMediumView::closeLanguageSelectorClicked()
{
    languageSelectorMoving = true;
    languageSelector.enableCachedMode(true);
    languageSelector.updateCache();
    languageSelector.startMoveAnimation(800, 0, 40, EasingEquations::sineEaseInOut);
}

void DashboardMediumView::languageChanged()
{
    gauge.updateTextAreas();
}

void DashboardMediumView::setOrangeBattery()
{
    batteryIcon.setBitmap(presenter->getLight() ? BITMAP_BATTERY_LIGHTNING_LIGHT_ID : BITMAP_BATTERY_LIGHTNING_ID);
    batteryIcon.invalidate();
    alreadyOrange = true;
}

void DashboardMediumView::setGreenBattery()
{
    batteryIcon.setBitmap(presenter->getLight() ? BITMAP_BATTERY_LEAF_LIGHT_ID : BITMAP_BATTERY_LEAF_ID);
    batteryIcon.invalidate();
    alreadyOrange = false;
}

void DashboardMediumView::updateBatteryLevel()
{
    const uint8_t batteryLvl = presenter->getBatteryPct();
    if (presenter->getLight())
    {
        if      (batteryLvl > 100/6*5) batteryLevel.setBitmap(alreadyOrange ? BITMAP_BATTERY_LEVEL_FULL_POWER_LIGHT_ID : BITMAP_BATTERY_LEVEL_FULL_ECO_LIGHT_ID);
        else if (batteryLvl > 100/6*4) batteryLevel.setBitmap(alreadyOrange ? BITMAP_BATTERY_LEVEL_4_POWER_LIGHT_ID    : BITMAP_BATTERY_LEVEL_4_ECO_LIGHT_ID);
        else if (batteryLvl > 100/6*3) batteryLevel.setBitmap(alreadyOrange ? BITMAP_BATTERY_LEVEL_3_POWER_LIGHT_ID    : BITMAP_BATTERY_LEVEL_3_ECO_LIGHT_ID);
        else if (batteryLvl > 100/6*2) batteryLevel.setBitmap(alreadyOrange ? BITMAP_BATTERY_LEVEL_2_POWER_LIGHT_ID    : BITMAP_BATTERY_LEVEL_2_ECO_LIGHT_ID);
        else if (batteryLvl > 100/6*1) batteryLevel.setBitmap(alreadyOrange ? BITMAP_BATTERY_LEVEL_1_POWER_LIGHT_ID    : BITMAP_BATTERY_LEVEL_1_ECO_LIGHT_ID);
        else                           batteryLevel.setBitmap(BITMAP_BATTERY_LEVEL_LOW_LIGHT_ID);
    }
    else
    {
        if      (batteryLvl > 100/6*5) batteryLevel.setBitmap(alreadyOrange ? BITMAP_BATTERY_LEVEL_FULL_POWER_ID : BITMAP_BATTERY_LEVEL_FULL_ECO_ID);
        else if (batteryLvl > 100/6*4) batteryLevel.setBitmap(alreadyOrange ? BITMAP_BATTERY_LEVEL_4_POWER_ID    : BITMAP_BATTERY_LEVEL_4_ECO_ID);
        else if (batteryLvl > 100/6*3) batteryLevel.setBitmap(alreadyOrange ? BITMAP_BATTERY_LEVEL_3_POWER_ID    : BITMAP_BATTERY_LEVEL_3_ECO_ID);
        else if (batteryLvl > 100/6*2) batteryLevel.setBitmap(alreadyOrange ? BITMAP_BATTERY_LEVEL_2_POWER_ID    : BITMAP_BATTERY_LEVEL_2_ECO_ID);
        else if (batteryLvl > 100/6*1) batteryLevel.setBitmap(alreadyOrange ? BITMAP_BATTERY_LEVEL_1_POWER_ID    : BITMAP_BATTERY_LEVEL_1_ECO_ID);
        else                           batteryLevel.setBitmap(BITMAP_BATTERY_LEVEL_LOW_ID);
    }
    if (oldBitmapID != batteryLevel.getBitmapId())
    {
        batteryLevel.invalidate();
        oldBitmapID = batteryLevel.getBitmapId();
    }
}

void DashboardMediumView::updateTripInfo()
{
    uint16_t newBPM; float newDistance;
    switch (presenter->getSelectedButton())
    {
    case bpm:
        newBPM = presenter->getBPM();
        if (oldBPM != newBPM) { Unicode::snprintf(bpmValueBuffer, BPMVALUE_SIZE, "%d", newBPM); bpmValue.invalidate(); oldBPM = newBPM; }
        break;
    case time:
        newDistance = presenter->getDistance();
        if ((int)(newDistance * 10) != oldDistance)
        {
            Unicode::snprintfFloat(timeTextBuffer1, TIMETEXTBUFFER1_SIZE, "%.1f", newDistance);
            timeTextBuffer2[0] = 0;
            timeText.invalidate();
            oldDistance = (int)(newDistance * 10);
        }
        break;
    case distance:
        newDistance = presenter->getDistance();
        if ((int)(newDistance*10) != oldDistance)
        {
            Unicode::snprintfFloat(distanceValueBuffer, DISTANCEVALUE_SIZE, "%.1f", newDistance);
            distanceValue.invalidate(); oldDistance = (int)(newDistance*10);
        }
        break;
    default: break;
    }
}

void DashboardMediumView::updateTheme()
{
    if (presenter->getLight())
    {
        gauge.setColor(Constants::LIGHT_BG_COLOR);
        backgroundColor.setColor(Constants::LIGHT_BG_COLOR);
        leftLines.setSVG(SVG_BACKGROUND_HEXAGON_LIGHT_ID);
        rightLines.setSVG(SVG_BACKGROUND_HEXAGON_LIGHT_ID);
        rightLines.setScaleX(-1); rightLines.setImagePosition(540, 0);
        languageSelector.setColor(Constants::LIGHT_BG_COLOR);
        bpmButton.setBitmaps(BITMAP_ICON_BPM_NORMAL_LIGHT_ID, BITMAP_ICON_BPM_NORMAL_LIGHT_ID, BITMAP_ICON_BPM_ACTIVE_LIGHT_ID, BITMAP_ICON_BPM_ACTIVE_LIGHT_ID);
        distanceButton.setBitmaps(BITMAP_ICON_DISTANCE_NORMAL_LIGHT_ID, BITMAP_ICON_DISTANCE_NORMAL_LIGHT_ID, BITMAP_ICON_DISTANCE_ACTIVE_LIGHT_ID, BITMAP_ICON_DISTANCE_ACTIVE_LIGHT_ID);
        bpmValue.setColor(Constants::LIGHT_MAIN_TEXTCOLOR); bpmSuffix.setColor(Constants::LIGHT_MAIN_TEXTCOLOR);
        timeText.setColor(Constants::LIGHT_MAIN_TEXTCOLOR); distanceValue.setColor(Constants::LIGHT_MAIN_TEXTCOLOR);
        distanceSuffix.setColor(Constants::LIGHT_MAIN_TEXTCOLOR);
        weatherButton.setBitmaps(BITMAP_BUTTON_NORMAL_LIGHT_ID, BITMAP_BUTTON_PRESSED_LIGHT_ID, BITMAP_ICON_WEATHER_LIGHT_ID, BITMAP_ICON_WEATHER_LIGHT_ID);
        batteryButton.setBitmaps(BITMAP_BUTTON_NORMAL_LIGHT_ID, BITMAP_BUTTON_PRESSED_LIGHT_ID, BITMAP_BATTERY_BACKGROUND_ID, BITMAP_BATTERY_BACKGROUND_ID);
        batteryButton.setIconXY(29, 48);
        modeButton.setBitmaps(BITMAP_BUTTON_SMALL_LEFT_NORMAL_LIGHT_ID, BITMAP_BUTTON_SMALL_LEFT_PRESSED_LIGHT_ID, BITMAP_ICON_MODE_LIGHT_ID, BITMAP_ICON_MODE_LIGHT_ID);
        modeButton.setIconXY(98, 20);
        sizeButton.setBitmaps(BITMAP_BUTTON_SMALL_RIGHT_NORMAL_LIGHT_ID, BITMAP_BUTTON_SMALL_RIGHT_PRESSED_LIGHT_ID, BITMAP_ICON_FORMAT_LIGHT_ID, BITMAP_ICON_FORMAT_LIGHT_ID);
        sizeButton.setIconXY(28, 20);
        lockButton.setBitmaps(BITMAP_BUTTON_NORMAL_LIGHT_ID, BITMAP_BUTTON_PRESSED_LIGHT_ID, BITMAP_ICON_LOCK_LIGHT_ID, BITMAP_ICON_LOCK_LIGHT_ID);
        languageButton.setBitmaps(BITMAP_BUTTON_NORMAL_LIGHT_ID, BITMAP_BUTTON_PRESSED_LIGHT_ID, BITMAP_ICON_LANGUAGE_LIGHT_ID, BITMAP_ICON_LANGUAGE_LIGHT_ID);
    }
    else
    {
        gauge.setColor(Constants::DARK_BG_COLOR);
        backgroundColor.setColor(Constants::DARK_BG_COLOR);
        leftLines.setSVG(SVG_BACKGROUND_HEXAGON_ID);
        rightLines.setSVG(SVG_BACKGROUND_HEXAGON_ID);
        rightLines.setScaleX(-1); rightLines.setImagePosition(540, 0);
        languageSelector.setColor(Constants::DARK_BG_COLOR);
        bpmButton.setBitmaps(BITMAP_ICON_BPM_NORMAL_ID, BITMAP_ICON_BPM_NORMAL_ID, BITMAP_ICON_BPM_ACTIVE_ID, BITMAP_ICON_BPM_ACTIVE_ID);
        distanceButton.setBitmaps(BITMAP_ICON_DISTANCE_NORMAL_ID, BITMAP_ICON_DISTANCE_NORMAL_ID, BITMAP_ICON_DISTANCE_ACTIVE_ID, BITMAP_ICON_DISTANCE_ACTIVE_ID);
        bpmValue.setColor(Constants::DARK_MAIN_TEXTCOLOR); bpmSuffix.setColor(Constants::DARK_MAIN_TEXTCOLOR);
        timeText.setColor(Constants::DARK_MAIN_TEXTCOLOR); distanceValue.setColor(Constants::DARK_MAIN_TEXTCOLOR);
        distanceSuffix.setColor(Constants::DARK_MAIN_TEXTCOLOR);
        weatherButton.setBitmaps(BITMAP_BUTTON_NORMAL_ID, BITMAP_BUTTON_PRESSED_ID, BITMAP_ICON_WEATHER_ID, BITMAP_ICON_WEATHER_ID);
        batteryButton.setBitmaps(BITMAP_BUTTON_NORMAL_ID, BITMAP_BUTTON_PRESSED_ID, BITMAP_BATTERY_BACKGROUND_ID, BITMAP_BATTERY_BACKGROUND_ID);
        batteryButton.setIconXY(29, 48);
        modeButton.setBitmaps(BITMAP_BUTTON_SMALL_LEFT_NORMAL_ID, BITMAP_BUTTON_SMALL_LEFT_PRESSED_ID, BITMAP_ICON_MODE_ID, BITMAP_ICON_MODE_ID);
        modeButton.setIconXY(98, 20);
        sizeButton.setBitmaps(BITMAP_BUTTON_SMALL_RIGHT_NORMAL_ID, BITMAP_BUTTON_SMALL_RIGHT_PRESSED_ID, BITMAP_ICON_FORMAT_ID, BITMAP_ICON_FORMAT_ID);
        sizeButton.setIconXY(28, 20);
        lockButton.setBitmaps(BITMAP_BUTTON_NORMAL_ID, BITMAP_BUTTON_PRESSED_ID, BITMAP_ICON_LOCK_ID, BITMAP_ICON_LOCK_ID);
        languageButton.setBitmaps(BITMAP_BUTTON_NORMAL_ID, BITMAP_BUTTON_PRESSED_ID, BITMAP_ICON_LANGUAGE_ID, BITMAP_ICON_LANGUAGE_ID);
    }
    presenter->getPower() > 22 ? setOrangeBattery() : setGreenBattery();
    updateBatteryLevel();
    invalidate();
}
