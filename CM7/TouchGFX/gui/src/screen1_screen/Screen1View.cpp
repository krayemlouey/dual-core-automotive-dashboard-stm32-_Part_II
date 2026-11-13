#include <gui/screen1_screen/Screen1View.hpp>

Screen1View::Screen1View()
{

}

void Screen1View::setupScreen()
{
    Screen1ViewBase::setupScreen();
    tickCounter = 0;
}

void Screen1View::tearDownScreen()
{
    Screen1ViewBase::tearDownScreen();
}

void Screen1View::handleTickEvent()
{
    if (tickCounter <= 100)
    {
        lineProgress1.setValue(tickCounter);
        tickCounter++;
        
        if (tickCounter > 100)
        {
            // Transition to DashboardMedium only once after progress reaches 100
            static_cast<FrontendApplication*>(touchgfx::Application::getInstance())->gotoDashboardMediumScreenNoTransition();
        }
    }
}
