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

    // side-selection phase
    int SideOffset(int side);
    int SpatialIndex(int side);
    int SideFromSpatialIndex(int spatial);
    void ChangeSide(int delta);
    void ToggleReady();
    void SetReadyIndicator(int slot, bool ready);
    void DrawPixelLine(boost::intrusive_ptr<Image2D> img, int x0, int y0, int x1, int y1, const Vector3 &color);
    void HideSlot(int slot);
    void SetSidePhaseVisible(bool on);

    // team-selection phase
    void BuildTeamPanels();
    void ApplyTeamState();
    void ResetApplied(int side);
    void SelectEntryById(Gui2IconSelector *selector, int id);
    int FirstCountryIndex(Gui2IconSelector *selector);
    void OnCountryChanged(int side);
    void OnLeagueChanged(int side);
    void OnTeamChanged(int side);
    void OnReadyClicked(int side);

    void ConfigureTeamsInput();
    void RestoreInput();
    int FindLocalGamepadId();
    bool GamepadPresent(int id);

    Gui2Image *background;
    Gui2Caption *phaseCaption;
    Gui2Caption *side1Caption;
    Gui2Caption *side2Caption;
    Gui2Caption *team1Caption;
    Gui2Caption *team2Caption;
    Gui2Caption *helpCaption;

    std::vector<Gui2Image*> playerImages;
    std::vector<Gui2Caption*> playerNames;
    std::vector<Gui2Image*> playerReadyIcons;
    std::vector<bool> playerReadyState;
    std::vector<int> playerDeviceState;
    int sentDevice;
    unsigned long lastGamepadSideChange_ms;
    int localGamepadId;
    bool deviceLostSent;

    bool teamsBuilt;
    bool teamsVisible;
    bool defaultSent[2];
    Gui2Image *teamBg[2];
    Gui2Grid *teamGrid[2];
    Gui2IconSelector *countrySelect[2];
    Gui2IconSelector *leagueSelect[2];
    Gui2IconSelector *teamSelect[2];
    Gui2Button *readyButton[2];
    Gui2Caption *homePanelCaption;
    Gui2Caption *awayPanelCaption;
    int lastCountryId[2];
    int lastLeagueId[2];
    int lastTeamId[2];

};

#endif
