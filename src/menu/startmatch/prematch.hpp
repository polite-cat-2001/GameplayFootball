// written by bastiaan konings schuiling 2008 - 2015
// this work is public domain. the code is undocumented, scruffy, untested, and should generally not be used for anything important.
// i do not offer support, so don't ask. to be used for inspiration :)

#ifndef _HPP_MENU_PREMATCH
#define _HPP_MENU_PREMATCH

#include "utils/gui2/windowmanager.hpp"

#include "utils/gui2/page.hpp"
#include "utils/gui2/widgets/grid.hpp"
#include "utils/gui2/widgets/button.hpp"
#include "utils/gui2/widgets/slider.hpp"
#include "utils/gui2/widgets/image.hpp"
#include "utils/gui2/widgets/caption.hpp"

#include "../pagefactory.hpp"

#include "../../data/matchdata.hpp"

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

    virtual void ProcessWindowingEvent(WindowingEvent *event);

  protected:
    void BuildTabs();
    void BuildContents();
    void ShowContent(int tab);
    void FocusFirstContent();
    bool IsInContent(Gui2View *view);

    MatchData *matchData;
    TeamData *teamData[2];

    Gui2Grid *tabGrid;
    std::vector<Gui2Button*> tabButtons;

    Gui2Grid *contentGrid;
    // one container per tab; only the selected one is visible
    std::vector<Gui2View*> contentViews;

    Gui2Slider *difficultySlider;
    Gui2Slider *matchDurationSlider;

    int activeTab;

};

#endif
