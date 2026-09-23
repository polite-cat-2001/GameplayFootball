// written by bastiaan konings schuiling 2008 - 2015
// this work is public domain. the code is undocumented, scruffy, untested, and should generally not be used for anything important.
// i do not offer support, so don't ask. to be used for inspiration :)

#ifndef _HPP_MENU_GAMEPLAN
#define _HPP_MENU_GAMEPLAN

#include "utils/gui2/windowmanager.hpp"

#include "utils/gui2/page.hpp"
#include "utils/gui2/widgets/root.hpp"
#include "utils/gui2/widgets/grid.hpp"
#include "utils/gui2/widgets/button.hpp"
#include "utils/gui2/widgets/image.hpp"
#include "utils/gui2/widgets/caption.hpp"
#include "utils/gui2/events.hpp"

#include "../onthepitch/match.hpp"

#include "../data/teamdata.hpp"

#include <map>
#include <utility>
#include <vector>

using namespace blunted;

// Bottom section bar of the plan screen. Only Positions drives the pitch editor
// for now; Tactics and Roles switch to a placeholder until they are built.
enum e_GamePlanSection {
  e_GamePlanSection_Tactics,
  e_GamePlanSection_Positions,
  e_GamePlanSection_Roles,
  e_GamePlanSection_Size
};

// One interactive squad slot on the plan: either a pitch position or a bench
// row. `index` is the index into TeamData::playerData / Team::GetAllPlayers().
struct PlanEntry {
  Gui2Button *button;
  Gui2Caption *roleCaption;
  Gui2Caption *ratingCaption;  // bench only: rating sits to the right of the name
  Gui2Caption *fatigueCaption; // condition %, right of the rating (all entries)
  Gui2Caption *nameCaption;    // pitch only: centered name drawn over the button
  Gui2Image *photo;           // pitch only: placeholder portrait
  e_PlayerRole role;          // display role (pending subs included)
  int teamID;
  int index;                  // slot
  int playerID;               // in-match: runtime player id, -1 pre-match
  bool onPitch;
  bool selectable;            // false: left the pitch, cannot be picked
  Vector3 pos;                // screen position (percent) of the entry center
  float cardCenterX;
  float roleY;
  float nameY;
  float cardW;                // pitch card geometry, for in-place preview moves
  float cardPhotoH;
};

// One editable team: its pitch plus a bench column to the right of it.
struct PlanPanel {
  PlanPanel() : teamID(0), teamData(0), team(0), editable(false),
                px(0), py(0), pw(0), ph(0), bx(0), by(0), bw(0),
                cursorIndex(-1), heldIndex(-1), benchCount(0), benchScroll(0),
                benchMaxVisible(1), benchStep(4.0f), benchRowHeight(2.5f), voteReady(false),
                activeSection(-1), barCursor(0), barFocused(false), sectionCaption(0), barCenterX(50.0f),
                benchHeader(0), schemeCursor(0), committedScheme(-1) {}

  int teamID;
  TeamData *teamData;
  Team *team;      // in-match only, else 0
  bool editable;

  // panel geometry (percent of screen)
  float px, py, pw, ph; // pitch
  float bx, by, bw;     // bench column

  // per-team cursor state
  int cursorIndex; // entry position
  int heldIndex;   // entry position
  int benchCount;
  int benchScroll;
  int benchMaxVisible;
  float benchStep;
  float benchRowHeight;
  std::vector<int> entryIndices;               // positions into GamePlanPage::entries
  std::vector<std::pair<int, int> > pendingSubs; // (outSlot, inSlot), normalized
  bool voteReady;
  unsigned long lastMove_ms;

  // Per-side section bar (Tactics / Positions / Roles) under this panel.
  int activeSection;  // -1 == neutral: bar focused, no section opened yet
  int barCursor;      // highlighted section on this side's bar
  bool barFocused;    // this side's device is currently on its bar
  std::vector<Gui2Button*> sectionButtons;
  Gui2Caption *sectionCaption;
  float barCenterX;

  // Tactics section: the bench column becomes a scheme list. Scrolling previews
  // the highlighted scheme on the pitch; Enter commits, Back returns to the bar.
  Gui2Caption *benchHeader;             // "Subs" or "Scheme"
  std::vector<Gui2Button*> schemeButtons;
  int schemeCursor;                     // highlighted scheme
  int committedScheme;                  // scheme the formation currently matches, or -1
  std::map<int, FormationEntry> pitchBase; // slot -> actual (non-preview) formation
  float pitchBottomLimit;               // pitch card bottom clamp, from the real GK
};

// Game plan screen. Normally one editable team (+ read-only opponent). In a
// local two-player match both teams are editable at once, each driven by its own
// device (two cursors); leaving the screen is a vote.
class GamePlanPage : public Gui2Page {

  public:
    GamePlanPage(Gui2WindowManager *windowManager, const Gui2PageData &pageData);
    virtual ~GamePlanPage();

    virtual void Process();
    virtual void ProcessWindowingEvent(WindowingEvent *event);
    virtual void ProcessKeyboardEvent(KeyboardEvent *event);
    virtual void ProcessJoystickEvent(JoystickEvent *event);

  protected:
    bool InMatch();

    void SetupPanels();
    void BuildPlan();
    void BuildSectionBar(PlanPanel &panel);
    void BuildOpponentReady(float centerX);
    void BuildEntries();
    void BuildPanel(PlanPanel &panel);
    void BuildOpponent(int teamID, float x, float y, float w, float h);
    void ClearEntries();
    void LayoutBench(PlanPanel &panel);
    void Rebuild(int focusTeam, int focusSlot);

    int FindNextEntry(PlanPanel &panel, int from, const Vector3 &direction);
    void SelectCursor(PlanPanel &panel, int entryPosition);
    void PerformAction(PlanPanel &panel, int a, int b);
    void QueueSubstitution(PlanPanel &panel, int outIndex, int inIndex);
    bool CancelQueued(PlanPanel &panel, int outIndex, int inIndex);
    bool IsPending(PlanPanel &panel, int entryIndex, bool &isOut);
    void NormalizePending(PlanPanel &panel);

    void HandlePanelInput(PlanPanel &panel, const Vector3 &direction, bool accept, bool back, unsigned long now_ms);
    int PanelForDevice(int controllerIndex, bool keyboard);
    void TryLeave();
    void RefreshExitStatus();

    // Per-side bottom section bar. Down from the pitch (or Back) drops to the
    // bar, Left/Right pick a section, Enter opens it, Back on the bar leaves.
    // Each side's bar is driven by that side's device; single layout also uses
    // GUI focus for the activate/Enter pipeline.
    void FocusSectionBar(PlanPanel &panel);
    void FocusContent(PlanPanel &panel);
    void MoveSectionFocus(PlanPanel &panel, int delta);
    void SectionClicked(int panelIndex, int section);
    void RefreshSectionBar(PlanPanel &panel);
    int PanelIndex(PlanPanel &panel);
    bool PositionsActive(const PlanPanel &panel) const;
    bool TacticsActive(const PlanPanel &panel) const;
    void SetSection(PlanPanel &panel, int section);
    std::string SectionName(int section);
    std::string SectionDescription(int section);

    // Tactics section: scheme list in place of the bench column.
    void BuildSchemeList(PlanPanel &panel);
    void LayoutSchemes(PlanPanel &panel);
    void RefreshSchemes(PlanPanel &panel);
    void MoveSchemeCursor(PlanPanel &panel, int delta);
    void CommitScheme(PlanPanel &panel);
    void PreviewSchemes(PlanPanel &panel);
    Vector3 PitchAnchor(PlanPanel &panel, const FormationEntry &entry);
    void PositionPitchCard(PlanEntry &entry, PlanPanel &panel, float anchorX, float anchorY, float maxBottom);
    void SchemeClicked(int panelIndex, int scheme);
    void SendPlanScheme(int side, int scheme);

    void Refresh();
    void RefreshPanel(PlanPanel &panel);
    void CenterCaption(Gui2Caption *caption, float centerX, float y, float height, const std::string &text);
    float EntryFatigue(PlanPanel &panel, const PlanEntry &entry);
    void PositionPitchCaption(PlanEntry &entry, const std::string &roleText, const std::string &fatigueText);

    void EntryClicked(int entryPosition);

    void UpdateInfo();
    void UpdateInfoDetail(Gui2Image *photo, Gui2Caption *badge, Gui2Caption *name, int entryPosition, bool visible);
    e_PlayerRole GetEntryRole(int entryPosition);

    TeamData *GetTeamDataFor(int teamID);
    Team *GetTeamFor(int teamID);

    void SendPlanSwap(int side, int dbA, int dbB);

    std::vector<PlanEntry> entries;
    // Read-only opponent cards. Unlike `entries`, these are plain views (no
    // interaction), but they must be tracked and freed on rebuild or they stack.
    std::vector<Gui2View*> opponentViews;
    std::vector<PlanPanel> panels;
    bool dualPanel;
    int moveCooldown_ms;

    // input mapping: controller index (0 = keyboard, 1.. = gamepads) -> panel
    int panelDevice[2];

    bool rebuildPending;
    int rebuildFocusTeam;
    int rebuildFocusSlot;

    // Pre-match network plan: each peer opens it locally and edits only its own
    // team; edits are relayed through the host and the page rebuilds when the
    // shared MatchData changes.
    bool networkPrematch;
    unsigned int seenPlanRevision;

    // Single-panel placement of the read-only opponent (mirrored when the peer's
    // team is the away side, so own team sits on the right).
    float oppX;
    float oppW;

    // Single layout only: non-editable opponent (AI or remote peer) shows a
    // static READY instead of a section bar.
    Gui2Caption *opponentReady;

    Gui2Caption *header;
    Gui2Image *infoPhotoA;
    Gui2Image *infoPhotoB;
    Gui2Caption *infoBadgeA;
    Gui2Caption *infoBadgeB;
    Gui2Caption *infoNameA;
    Gui2Caption *infoNameB;

};

#endif
