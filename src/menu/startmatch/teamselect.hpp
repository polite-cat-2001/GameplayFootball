// written by bastiaan konings schuiling 2008 - 2015
// this work is public domain. the code is undocumented, scruffy, untested, and should generally not be used for anything important.
// i do not offer support, so don't ask. to be used for inspiration :)

#ifndef _HPP_MENU_TEAMSELECT
#define _HPP_MENU_TEAMSELECT

#include "utils/gui2/windowmanager.hpp"

#include "utils/gui2/widgets/menu.hpp"
#include "utils/gui2/widgets/root.hpp"
#include "utils/gui2/widgets/grid.hpp"
#include "utils/gui2/widgets/button.hpp"
#include "utils/gui2/widgets/image.hpp"
#include "utils/gui2/widgets/slider.hpp"
#include "utils/gui2/widgets/iconselector.hpp"

#include "../../onthepitch/match.hpp"

using namespace blunted;

class HIDGamepad;

// Selector population helpers shared with the LAN lobby team-select phase.
std::string GetNationalTeamsLeagueID();
void AddCountries(Gui2IconSelector *selector);
void AddLeagues(Gui2IconSelector *selector, const std::string &country_id);
void AddTeams(Gui2IconSelector *selector, const std::string &competition_id);

// Local kick-off team selection. Both sides have their own cursor so two local
// players can pick their team at the same time with their own device. When only
// one side is human (the other is the CPU) the two panels are picked in turn
// with the single device, as before.
class TeamSelectPage : public Gui2Page {

  public:
    TeamSelectPage(Gui2WindowManager *windowManager, const Gui2PageData &pageData);
    virtual ~TeamSelectPage();

    virtual void OnGainFocus() { /* stay focusable ourselves and route input per side */ }
    virtual void Process();
    virtual void ProcessKeyboardEvent(KeyboardEvent *event);
    virtual void ProcessJoystickEvent(JoystickEvent *event);
    virtual void ProcessWindowingEvent(WindowingEvent *event);

  protected:
    enum e_CursorRow { e_Row_Country = 0, e_Row_Competition = 1, e_Row_Team = 2, e_Row_Ready = 3 };

    void BuildSide(int s);
    void RestoreSelection(int s, int teamID);
    void ShowSide(int s);
    void HideSide(int s);
    void SetupCompetitionSelect(int s);
    void SetupTeamSelect(int s);
    void HighlightSide(int s);
    void MoveSideSelection(int s, int delta);
    void MoveSideRow(int s, int delta);
    void ActivateSide(int s);
    void CancelSide(int s);
    void SetSideReady(int s, bool ready);
    void RevealAway();
    bool IsNational(int s);
    HIDGamepad *FindGamepad(int gamepadID);
    void GoOptionsMenu();

    // s: 0 = home/left (player 1), 1 = away/right (player 2)
    Gui2IconSelector *countrySelect[2];
    Gui2IconSelector *competitionSelect[2];
    Gui2IconSelector *teamSelect[2];
    Gui2Button *readyButton[2];
    Gui2Grid *teamGrid[2];
    Gui2Image *teamBg[2];
    Gui2Caption *panelCaption[2];

    int cursorRow[2];
    bool sideReady[2];
    bool sideActive[2];   // side accepts input (both true when two local humans)
    int deviceKind[2];    // -1 none, 0 keyboard, 1 gamepad
    int deviceGamepad[2]; // HIDGamepad id when deviceKind == 1
    unsigned long lastMove_ms[2];
    unsigned long lastRowMove_ms[2];

    bool parallel;      // both sides are local humans with their own device

};

#endif
