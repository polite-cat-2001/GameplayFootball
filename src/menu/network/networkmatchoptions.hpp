#ifndef _HPP_MENU_NETWORK_MATCHOPTIONS
#define _HPP_MENU_NETWORK_MATCHOPTIONS

#include "utils/gui2/windowmanager.hpp"

#include "utils/gui2/widgets/caption.hpp"
#include "utils/gui2/widgets/image.hpp"
#include "utils/gui2/widgets/grid.hpp"
#include "utils/gui2/widgets/slider.hpp"
#include "utils/gui2/widgets/button.hpp"

#include "utils/gui2/page.hpp"

#include "net/netmessages.hpp"

using namespace blunted;

// Kickoff options between team selection and the match: AI difficulty and match
// duration. Only the host may change them (its simulation uses them); every peer
// sees the mirrored values from LobbyState.
class NetworkMatchOptionsPage : public Gui2Page {

  public:
    NetworkMatchOptionsPage(Gui2WindowManager *windowManager, const Gui2PageData &pageData);
    virtual ~NetworkMatchOptionsPage();

    virtual void Process();
    virtual void ProcessKeyboardEvent(KeyboardEvent *event);
    virtual void ProcessJoystickEvent(JoystickEvent *event);
    virtual void ProcessWindowingEvent(WindowingEvent *event);

  protected:
    NetLobbyState GetState();
    bool IsHost();
    void SendOption(int field, float value);
    void SendBackToTeams();
    void StartHostMatch();

    Gui2Slider *difficultySlider;
    Gui2Slider *durationSlider;
    Gui2Button *startButton;
    Gui2Caption *statusCaption;

    bool matchStartTriggered;

};

#endif
