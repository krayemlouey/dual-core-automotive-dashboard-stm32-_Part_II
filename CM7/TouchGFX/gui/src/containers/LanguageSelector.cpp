#include <gui/containers/LanguageSelector.hpp>
#include <BitmapDatabase.hpp>

LanguageSelector::LanguageSelector() :
    isLight(false)
{
}

void LanguageSelector::initialize()
{
    switch (Texts::getLanguage())
    {
    case GB: englishButtonClicked(); break;
    case CN: chineseButtonClicked(); break;
    case JP: japaneseButtonClicked(); break;
    case AR: arabicButtonClicked(); break;
    default: break;
    }
    LanguageSelectorBase::initialize();
}

void LanguageSelector::englishButtonClicked()  { setLanguage(GB); }
void LanguageSelector::chineseButtonClicked()  { setLanguage(CN); }
void LanguageSelector::japaneseButtonClicked() { setLanguage(JP); }
void LanguageSelector::arabicButtonClicked()   { setLanguage(AR); }

void LanguageSelector::setColor(int color)
{
    isLight = color > 0xC19 ? true : false;
    background.setColor(color);
    outline.setColor(isLight ? 0xACC7E5 : 0x0E2034);
    languageText.setColor(isLight ? 0x1F4672 : 0xB5D7FD);
    englishText.setColor(isLight ? 0x1F4672 : 0xB5D7FD);
    chineseText.setColor(isLight ? 0x1F4672 : 0xB5D7FD);
    japaneseText.setColor(isLight ? 0x1F4672 : 0xB5D7FD);
    arabicText.setColor(isLight ? 0x1F4672 : 0xB5D7FD);

    if (isLight)
    {
        closeButton.setIconBitmaps(BITMAP_CLOSE_ICON_NORMAL_LIGHT_ID, BITMAP_CLOSE_ICON_PRESSED_LIGHT_ID);
        englishButton.setBitmaps(BITMAP_SIDE_MENU_BUTTON_NORMAL_LIGHT_ID, BITMAP_SIDE_MENU_BUTTON_PRESSED_LIGHT_ID, BITMAP_FLAG_ICON_ENGLISH_ID, BITMAP_FLAG_ICON_ENGLISH_ID);
        chineseButton.setBitmaps(BITMAP_SIDE_MENU_BUTTON_NORMAL_LIGHT_ID, BITMAP_SIDE_MENU_BUTTON_PRESSED_LIGHT_ID, BITMAP_FLAG_ICON_CHINESE_ID, BITMAP_FLAG_ICON_CHINESE_ID);
        japaneseButton.setBitmaps(BITMAP_SIDE_MENU_BUTTON_NORMAL_LIGHT_ID, BITMAP_SIDE_MENU_BUTTON_PRESSED_LIGHT_ID, BITMAP_FLAG_ICON_JAPANESE_ID, BITMAP_FLAG_ICON_JAPANESE_ID);
        arabicButton.setBitmaps(BITMAP_SIDE_MENU_BUTTON_NORMAL_LIGHT_ID, BITMAP_SIDE_MENU_BUTTON_PRESSED_LIGHT_ID, BITMAP_FLAG_ICON_ARABIC_ID, BITMAP_FLAG_ICON_ARABIC_ID);
    }
    else
    {
        closeButton.setIconBitmaps(BITMAP_CLOSE_ICON_NORMAL_ID, BITMAP_CLOSE_ICON_PRESSED_ID);
        englishButton.setBitmaps(BITMAP_SIDE_MENU_BUTTON_NORMAL_ID, BITMAP_SIDE_MENU_BUTTON_PRESSED_ID, BITMAP_FLAG_ICON_ENGLISH_ID, BITMAP_FLAG_ICON_ENGLISH_ID);
        chineseButton.setBitmaps(BITMAP_SIDE_MENU_BUTTON_NORMAL_ID, BITMAP_SIDE_MENU_BUTTON_PRESSED_ID, BITMAP_FLAG_ICON_CHINESE_ID, BITMAP_FLAG_ICON_CHINESE_ID);
        japaneseButton.setBitmaps(BITMAP_SIDE_MENU_BUTTON_NORMAL_ID, BITMAP_SIDE_MENU_BUTTON_PRESSED_ID, BITMAP_FLAG_ICON_JAPANESE_ID, BITMAP_FLAG_ICON_JAPANESE_ID);
        arabicButton.setBitmaps(BITMAP_SIDE_MENU_BUTTON_NORMAL_ID, BITMAP_SIDE_MENU_BUTTON_PRESSED_ID, BITMAP_FLAG_ICON_ARABIC_ID, BITMAP_FLAG_ICON_ARABIC_ID);
    }
    englishButton.setIconXY(24, 16);
    chineseButton.setIconXY(24, 16);
    japaneseButton.setIconXY(24, 16);
    arabicButton.setIconXY(24, 16);
    setLanguage(Texts::getLanguage());
}

void LanguageSelector::setLanguage(LanguageId selectedLanguage)
{
    Texts::setLanguage(selectedLanguage);
    switch (selectedLanguage)
    {
    case GB:
        englishSelectedIcon.setBitmap(isLight ? BITMAP_RADIO_BUTTON_SELECTED_LIGHT_ID : BITMAP_RADIO_BUTTON_SELECTED_ID);
        chineseSelectedIcon.setBitmap(isLight ? BITMAP_RADIO_BUTTON_NORMAL_LIGHT_ID : BITMAP_RADIO_BUTTON_NORMAL_ID);
        japaneseSelectedIcon.setBitmap(isLight ? BITMAP_RADIO_BUTTON_NORMAL_LIGHT_ID : BITMAP_RADIO_BUTTON_NORMAL_ID);
        arabicSelectedIcon.setBitmap(isLight ? BITMAP_RADIO_BUTTON_NORMAL_LIGHT_ID : BITMAP_RADIO_BUTTON_NORMAL_ID);
        break;
    case CN:
        englishSelectedIcon.setBitmap(isLight ? BITMAP_RADIO_BUTTON_NORMAL_LIGHT_ID : BITMAP_RADIO_BUTTON_NORMAL_ID);
        chineseSelectedIcon.setBitmap(isLight ? BITMAP_RADIO_BUTTON_SELECTED_LIGHT_ID : BITMAP_RADIO_BUTTON_SELECTED_ID);
        japaneseSelectedIcon.setBitmap(isLight ? BITMAP_RADIO_BUTTON_NORMAL_LIGHT_ID : BITMAP_RADIO_BUTTON_NORMAL_ID);
        arabicSelectedIcon.setBitmap(isLight ? BITMAP_RADIO_BUTTON_NORMAL_LIGHT_ID : BITMAP_RADIO_BUTTON_NORMAL_ID);
        break;
    case JP:
        englishSelectedIcon.setBitmap(isLight ? BITMAP_RADIO_BUTTON_NORMAL_LIGHT_ID : BITMAP_RADIO_BUTTON_NORMAL_ID);
        chineseSelectedIcon.setBitmap(isLight ? BITMAP_RADIO_BUTTON_NORMAL_LIGHT_ID : BITMAP_RADIO_BUTTON_NORMAL_ID);
        japaneseSelectedIcon.setBitmap(isLight ? BITMAP_RADIO_BUTTON_SELECTED_LIGHT_ID : BITMAP_RADIO_BUTTON_SELECTED_ID);
        arabicSelectedIcon.setBitmap(isLight ? BITMAP_RADIO_BUTTON_NORMAL_LIGHT_ID : BITMAP_RADIO_BUTTON_NORMAL_ID);
        break;
    case AR:
        englishSelectedIcon.setBitmap(isLight ? BITMAP_RADIO_BUTTON_NORMAL_LIGHT_ID : BITMAP_RADIO_BUTTON_NORMAL_ID);
        chineseSelectedIcon.setBitmap(isLight ? BITMAP_RADIO_BUTTON_NORMAL_LIGHT_ID : BITMAP_RADIO_BUTTON_NORMAL_ID);
        japaneseSelectedIcon.setBitmap(isLight ? BITMAP_RADIO_BUTTON_NORMAL_LIGHT_ID : BITMAP_RADIO_BUTTON_NORMAL_ID);
        arabicSelectedIcon.setBitmap(isLight ? BITMAP_RADIO_BUTTON_SELECTED_LIGHT_ID : BITMAP_RADIO_BUTTON_SELECTED_ID);
        break;
    default:
        break;
    }
    englishSelectedIcon.invalidateContent();
    chineseSelectedIcon.invalidateContent();
    japaneseSelectedIcon.invalidateContent();
    arabicSelectedIcon.invalidateContent();
    englishText.invalidate();
    chineseText.invalidate();
    japaneseText.invalidate();
    arabicText.invalidate();
    languageText.invalidate();
    updateCache();
    emitLanguageChangedCallback();
}
