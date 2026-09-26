// written by bastiaan konings schuiling 2008 - 2015
// this work is public domain. the code is undocumented, scruffy, untested, and should generally not be used for anything important.
// i do not offer support, so don't ask. to be used for inspiration :)

#ifndef _HPP_MENU_PREMATCH
#define _HPP_MENU_PREMATCH

#include "utils/gui2/windowmanager.hpp"

#include "utils/gui2/events.hpp"
#include "utils/gui2/page.hpp"
#include "utils/gui2/widgets/grid.hpp"
#include "utils/gui2/widgets/button.hpp"
#include "utils/gui2/widgets/slider.hpp"
#include "utils/gui2/widgets/image.hpp"
#include "utils/gui2/widgets/caption.hpp"
#include "utils/gui2/widgets/pulldown.hpp"

#include "../pagefactory.hpp"

#include "../../data/matchdata.hpp"
#include "../../net/netmessages.hpp"

#include <set>
#include <vector>

class PreMatchCaptainPreview;

using namespace blunted;

// Pre-match hub: a PES-style tab strip whose tabs configure the upcoming match
// (kits, kick-off/lineups, game plan, options, camera, system settings) in the
// engine's own Gui2 style.
class PreMatchPage : public Gui2Page {

  public:
    PreMatchPage(Gui2WindowManager *windowManager, const Gui2PageData &pageData);
    virtual ~PreMatchPage();

    void SelectTab(int tab);
    void TabClicked(int tab);
    void GoStartMatch();
    void OpenPage(int pageID);

    virtual void Process();
    virtual void ProcessKeyboardEvent(KeyboardEvent *event);
    virtual void ProcessJoystickEvent(JoystickEvent *event);
    virtual void ProcessWindowingEvent(WindowingEvent *event);

  protected:
    void BuildTabs();
    void BuildContents();
    void BuildKitTab(Gui2Grid *kit);
    void ShowContent(int tab);
    void FocusFirstContent();
    bool IsInContent(Gui2View *view);
    bool IsInOverlay(Gui2View *view);
    bool AnyKitPulldownOpen();

    // Kit tab: per-side pulldown of the kits that actually exist for the club.
    // This is a local-only preference (each peer can pick both sides); it is
    // written to the MenuTask stock that Team::InitPlayers reads on this peer.
    void OnKitChanged(int team);
    void ApplyKit(int team, int kit);
    Gui2Pulldown *kitPulldown[2];
    std::vector<int> availableKits[2];
    bool suppressKitSignal;

    // Network hub: host-authoritative options, host-only game plan, and
    // confirmations from both peers before leaving the hub or starting.
    bool IsNetworkHost() const;
    NetLobbyState GetNetworkState();
    void SendHubVote(int vote);
    void SendMatchOption(int field, float value);
    void DoHostStartMatch();
    void UpdateNetworkStatus();

    bool networkMatch;
    int localVote; // this peer's own hub vote (toggle source, not the mirrored one)
    Gui2Caption *statusCaption;
    Gui2Button *startButton;

    // Local two-player: the same start vote, but per local side (its controller).
    bool localTwoPlayers;
    std::set<int> localStartVotes; // controller ids of the sides that agreed
    bool suppressLocalEscape; // the raw Back handler already consumed this one
    bool StartButtonFocused();
    void FocusKickOffTab();
    bool CancelLocalStartVote(int controllerID);
    void ToggleLocalStartVote(int controllerID);
    void UpdateLocalStatus();
    void DoLocalStartMatch();

    MatchData *matchData;
    TeamData *teamData[2];

    Gui2Grid *tabGrid;
    std::vector<Gui2Button*> tabButtons;

    Gui2Grid *contentGrid;
    // one container per tab; only the selected one is visible
    std::vector<Gui2View*> contentViews;

    Gui2Slider *difficultySlider;
    Gui2Slider *matchDurationSlider;

    PreMatchCaptainPreview *captainPreview[2];

    int activeTab;

};

#endif
