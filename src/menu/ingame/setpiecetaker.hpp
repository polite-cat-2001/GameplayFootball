// written by bastiaan konings schuiling 2008 - 2015
// this work is public domain. the code is undocumented, scruffy, untested, and should generally not be used for anything important.
// i do not offer support, so don't ask. to be used for inspiration :)

#ifndef _HPP_MENU_SETPIECETAKER
#define _HPP_MENU_SETPIECETAKER

#include "utils/gui2/windowmanager.hpp"

#include "utils/gui2/page.hpp"
#include "utils/gui2/widgets/button.hpp"
#include "utils/gui2/widgets/caption.hpp"

#include "../../onthepitch/match.hpp"

#include <vector>

using namespace blunted;

// Set-piece taker menu (#30): opened by the human who is actually taking the current set piece
// (Select / Share / Tab). Picks another active player of the taking team, including the keeper; the
// choice changes the actual taker on the host (or travels to it over the input channel on a thin
// client), and control stays with the peer that opened the menu.
class SetPieceTakerPage : public Gui2Page {

  public:
    SetPieceTakerPage(Gui2WindowManager *windowManager, const Gui2PageData &pageData);
    virtual ~SetPieceTakerPage();

    virtual void Exit();
    virtual void Process();
    virtual void ProcessWindowingEvent(WindowingEvent *event);

  protected:
    void MoveSelection(int delta);
    void OnPlayerChosen(Player *player);
    void CloseMenu();

    Match *match;
    IHIDevice *ownerDevice; // the taker's own device: only it may navigate (#30)
    int savedActiveJoystick;
    bool savedKeyboardActive;
    unsigned long lastMoveTime_ms; // debounce gamepad stick repeat
    std::vector<Gui2Button*> buttons;
    int selectedIndex;

};

#endif
