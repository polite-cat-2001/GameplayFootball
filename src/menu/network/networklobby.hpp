#ifndef _HPP_MENU_NETWORK_LOBBY
#define _HPP_MENU_NETWORK_LOBBY

#include "utils/gui2/windowmanager.hpp"

#include "utils/gui2/widgets/caption.hpp"
#include "utils/gui2/widgets/image.hpp"
#include "utils/gui2/widgets/grid.hpp"
#include "utils/gui2/widgets/iconselector.hpp"
#include "utils/gui2/widgets/button.hpp"

#include "utils/gui2/page.hpp"

#include "net/netmessages.hpp"

#include <vector>

using namespace blunted;

// Team-selection phase of a mirrored LAN lobby. Side selection lives in the
// shared SideSelectPage; this page is entered once the host advanced the lobby
// to the Teams phase, and falls back to SideSelectPage if the roster changes.
class NetworkLobbyPage : public Gui2Page {

  public:
    NetworkLobbyPage(Gui2WindowManager *windowManager, const Gui2PageData &pageData);
    virtual ~NetworkLobbyPage();

    virtual void Process();
    virtual void ProcessKeyboardEvent(KeyboardEvent *event);
    virtual void ProcessJoystickEvent(JoystickEvent *event);
    virtual void ProcessWindowingEvent(WindowingEvent *event);

  protected:
    NetLobbyState GetState();
    uint32_t GetLocalPlayerId();
    bool IsHost();
    int GetChooserSide(uint32_t playerId);
    void SendAction(int type, int side, int value, int value2 = 0);
    void Leave();

    // team-selection phase
    void BuildTeamPanels();
    void ApplyTeamState();
    void SelectEntryById(Gui2IconSelector *selector, int id);
    void OnCountryChanged(int side);
    void OnLeagueChanged(int side);
    void OnTeamChanged(int side);
    void OnReadyClicked(int side);

    void ConfigureTeamsInput();
    void RestoreInput();
    int FindLocalGamepadId();
    bool GamepadPresent(int id);

    // Escape/B: step back through the selectors (Ready -> team -> league ->
    // country); only at the country selector does it leave the lobby.
    void StepBack();

    Gui2Image *background;
    Gui2Caption *homePanelCaption;
    Gui2Caption *awayPanelCaption;
    Gui2Caption *helpCaption;

    bool teamsBuilt;
    bool deviceLostSent;
    int sentDevice;      // last device type reported to the lobby (0 keyboard, 1 gamepad)
    int lastLocalDevice; // last device seen in the lobby state, drives input reconfig
    int localGamepadId;
    bool defaultSent[2];
    Gui2Image *teamBg[2];
    Gui2Grid *teamGrid[2];
    Gui2IconSelector *countrySelect[2];
    Gui2IconSelector *leagueSelect[2];
    Gui2IconSelector *teamSelect[2];
    Gui2Button *readyButton[2];
    int lastCountryId[2];
    int lastLeagueId[2];
    int lastTeamId[2];

};

#endif
