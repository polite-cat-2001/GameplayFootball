#ifndef _HPP_MENU_NETWORK_LOBBY
#define _HPP_MENU_NETWORK_LOBBY

#include "utils/gui2/windowmanager.hpp"

#include "utils/gui2/widgets/caption.hpp"

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
    virtual void ProcessWindowingEvent(WindowingEvent *event);

  protected:
    NetLobbyState GetState();
    std::vector<NetCatalogEntry> GetCatalog();
    uint32_t GetLocalPlayerId();
    bool IsHost();
    int GetChooserSide(uint32_t playerId);
    void SendAction(int type, int side, int value);
    void Leave();
    std::string SideName(int side);
    std::string TeamName(int teamId);

    Gui2Caption *titleCaption;
    Gui2Caption *phaseCaption;
    Gui2Caption *teamsCaption;
    Gui2Caption *cursorCaption;
    Gui2Caption *helpCaption;
    std::vector<Gui2Caption*> playerCaptions;

};

#endif
