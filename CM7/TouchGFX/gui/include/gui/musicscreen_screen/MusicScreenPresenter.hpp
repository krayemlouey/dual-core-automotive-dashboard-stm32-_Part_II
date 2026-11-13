#ifndef MUSICSCREENPRESENTER_HPP
#define MUSICSCREENPRESENTER_HPP

#include <gui/model/ModelListener.hpp>
#include <mvp/Presenter.hpp>

using namespace touchgfx;

class MusicScreenView;

class MusicScreenPresenter : public touchgfx::Presenter, public ModelListener
{
public:
    MusicScreenPresenter(MusicScreenView& v);

    /**
     * activate() est appelé automatiquement à l'entrée dans l'écran.
     */
    virtual void activate();

    /**
     * deactivate() est appelé automatiquement à la sortie de l'écran.
     */
    virtual void deactivate();

    virtual ~MusicScreenPresenter() {}

    /* Appelé par ModelListener quand l'état audio est mis à jour */
    virtual void audioInfoUpdated() override;

    /* Commandes relayées depuis la View */
    void playPauseClicked();
    void nextClicked();
    void previousClicked();

private:
    MusicScreenPresenter();

    MusicScreenView& view;
};

#endif // MUSICSCREENPRESENTER_HPP
