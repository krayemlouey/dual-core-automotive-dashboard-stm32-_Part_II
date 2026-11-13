#ifndef MUSICSCREENVIEW_HPP
#define MUSICSCREENVIEW_HPP

#include <gui_generated/musicscreen_screen/MusicScreenViewBase.hpp>
#include <gui/musicscreen_screen/MusicScreenPresenter.hpp>

class MusicScreenView : public MusicScreenViewBase
{
public:
    MusicScreenView();
    virtual ~MusicScreenView() {}
    virtual void setupScreen();
    virtual void tearDownScreen();

    /* Appelé depuis le Presenter quand l'état audio change */
    void updateAudioInfo(const char* songName,
                         uint32_t    positionSec,
                         uint32_t    durationSec,
                         bool        isPlaying);

protected:
    /* Handlers d'interaction TouchGFX (générés dans la base) */
    virtual void playClicked()     override;
    virtual void nextClicked()     override;
    virtual void previousClicked() override;

private:
    /* Buffer pour le wildcard du nom de chanson */
    touchgfx::Unicode::UnicodeChar songNameBuf[64];

    /* Tick counter pour rafraîchir la barre de progression (~1 fps) */
    uint16_t tickCounter;

    static const uint16_t REFRESH_INTERVAL = 56; /* ~1 s à 56 Hz */
};

#endif // MUSICSCREENVIEW_HPP
