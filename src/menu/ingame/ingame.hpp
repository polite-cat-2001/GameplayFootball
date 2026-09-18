// written by bastiaan konings schuiling 2008 - 2015
// this work is public domain. the code is undocumented, scruffy, untested, and should generally not be used for anything important.
// i do not offer support, so don't ask. to be used for inspiration :)

#ifndef _HPP_MENU_INGAME
#define _HPP_MENU_INGAME

#include "utils/gui2/windowmanager.hpp"

#include "../cameramenu.hpp"

#include "utils/gui2/widgets/menu.hpp"
#include "utils/gui2/widgets/root.hpp"
#include "utils/gui2/widgets/grid.hpp"
#include "utils/gui2/widgets/button.hpp"

#include "../../onthepitch/match.hpp"

#include <set>

using namespace blunted;

class IngamePage : public Gui2Page {

  public:
    IngamePage(Gui2WindowManager *windowManager, const Gui2PageData &pageData);
    virtual ~IngamePage();

    void GoGamePlan();
    void GoControllerSelect();
    void GoSideSelect();
    void GoCameraSettings();
    void GoVisualOptions();
    void GoSystemSettings();
    void GoReplay();
    void GoPreQuit();
    void VoteResume();

    virtual void Process();
    virtual void ProcessKeyboardEvent(KeyboardEvent *event);
    virtual void ProcessJoystickEvent(JoystickEvent *event);
    virtual void ProcessWindowingEvent(WindowingEvent *event);

  protected:
    bool IsNetworkMatch();
    bool LocalResumeReady();
    int GetResumeReadyCount();
    int GetPeerCount();

    // Local two-player: "Continue" is a per-side vote, like the LAN resume.
    bool localTwoPlayers;
    std::set<int> localResumeVotes; // controller ids of the sides that agreed
    bool ContinueButtonFocused();
    void ToggleLocalResumeVote(int controllerID);
    void UpdateContinueCaption();

    int teamID; // team that activated the ingame menu
    Gui2Button *buttonContinue;

};


class PreQuitPage : public Gui2Page {

  public:
    PreQuitPage(Gui2WindowManager *windowManager, const Gui2PageData &pageData);
    virtual ~PreQuitPage();

    void GoMenu();

  protected:

};

#endif
