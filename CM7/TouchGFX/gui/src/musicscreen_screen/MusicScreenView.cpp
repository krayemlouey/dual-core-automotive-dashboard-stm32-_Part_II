/*
 * MusicScreenView.cpp
 * Implémentation de l'écran lecteur de musique.
 * Relaie les clics bouton vers le Presenter, et met à jour l'UI depuis l'état audio.
 */
#include <gui/musicscreen_screen/MusicScreenView.hpp>
#include <touchgfx/Unicode.hpp>

MusicScreenView::MusicScreenView()
    : tickCounter(0)
{
    /* Initialiser le buffer wildcard avec un nom vide */
    songNameBuf[0] = 0;
}

void MusicScreenView::setupScreen()
{
    MusicScreenViewBase::setupScreen();

    /* Brancher notre buffer unicode sur le widget SongName */
    SongName.setWildcard(songNameBuf);
    SongName.invalidate();

    /* Remettre la barre de progression à 0 */
    lineProgress1.setValue(0);
    lineProgress1.invalidate();

    tickCounter = 0;
}

void MusicScreenView::tearDownScreen()
{
    MusicScreenViewBase::tearDownScreen();
}

/* ─── Handlers de boutons (générés par le Designer) ──────────────────── */

void MusicScreenView::playClicked()
{
    presenter->playPauseClicked();
}

void MusicScreenView::nextClicked()
{
    presenter->nextClicked();
}

void MusicScreenView::previousClicked()
{
    presenter->previousClicked();
}

/* ─── Mise à jour de l'UI depuis le Presenter ────────────────────────── */

void MusicScreenView::updateAudioInfo(const char*  songName,
                                       uint32_t     positionSec,
                                       uint32_t     durationSec,
                                       bool         isPlaying)
{
    /* Nom de la chanson : convertir UTF-8 → UnicodeChar pour le wildcard */
    touchgfx::Unicode::fromUTF8(
        reinterpret_cast<const uint8_t*>(songName),
        songNameBuf,
        63);
    SongName.setWildcard(songNameBuf);
    SongName.invalidate();

    /* Barre de progression 0 – 100 */
    if (durationSec > 0)
    {
        int progress = static_cast<int>((positionSec * 100u) / durationSec);
        if (progress > 100) progress = 100;
        lineProgress1.setValue(progress);
    }
    else
    {
        lineProgress1.setValue(0);
    }
    lineProgress1.invalidate();

    /* Le bouton ToggleButton gère son état visuel de lui-même */
    (void)isPlaying;
}
