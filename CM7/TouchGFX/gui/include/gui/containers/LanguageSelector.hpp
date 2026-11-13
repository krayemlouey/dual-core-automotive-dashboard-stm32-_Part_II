#ifndef LANGUAGESELECTOR_HPP
#define LANGUAGESELECTOR_HPP

#include <gui_generated/containers/LanguageSelectorBase.hpp>
#include <texts/TextKeysAndLanguages.hpp>

class LanguageSelector : public LanguageSelectorBase
{
public:
    LanguageSelector();
    virtual ~LanguageSelector() {}
    virtual void initialize();
    virtual void englishButtonClicked();
    virtual void chineseButtonClicked();
    virtual void japaneseButtonClicked();
    virtual void arabicButtonClicked();

    void setColor(int color);

private:
    void setLanguage(LanguageId selectedLanguage);

    bool isLight;
};

#endif // LANGUAGESELECTOR_HPP
