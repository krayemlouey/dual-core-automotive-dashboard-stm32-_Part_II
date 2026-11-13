/*
 * MusicScreenPresenter.cpp
 * Relaie les commandes de la View vers le Model (AudioPlayer),
 * et transmet les mises à jour de l'état audio vers la View.
 */
#include <gui/musicscreen_screen/MusicScreenView.hpp>
#include <gui/musicscreen_screen/MusicScreenPresenter.hpp>

MusicScreenPresenter::MusicScreenPresenter(MusicScreenView& v)
    : view(v)
{
}

void MusicScreenPresenter::activate()
{
    /* Démarre la lecture dès l'entrée dans l'écran si aucune chanson n'est en cours */
    model->audioActivate();
}

void MusicScreenPresenter::deactivate()
{
    /* Pause propre à la sortie de l'écran */
    model->audioPause();
}

/* ─── Commandes View → Model ────────────────────────────────────────── */

void MusicScreenPresenter::playPauseClicked()
{
    model->audioPlayPause();
}

void MusicScreenPresenter::nextClicked()
{
    model->audioNext();
}

void MusicScreenPresenter::previousClicked()
{
    model->audioPrev();
}

/* ─── Callback Model → View ─────────────────────────────────────────── */

void MusicScreenPresenter::audioInfoUpdated()
{
    view.updateAudioInfo(
        model->getAudioSongName(),
        model->getAudioPosition(),
        model->getAudioDuration(),
        model->isAudioPlaying()
    );
}
