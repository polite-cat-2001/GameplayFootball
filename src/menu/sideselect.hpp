// written by bastiaan konings schuiling 2008 - 2015
// this work is public domain. the code is undocumented, scruffy, untested, and should generally not be used for anything important.
// i do not offer support, so don't ask. to be used for inspiration :)

#ifndef _HPP_MENU_SIDESELECT
#define _HPP_MENU_SIDESELECT

#include "utils/gui2/windowmanager.hpp"

#include "utils/gui2/widgets/caption.hpp"
#include "utils/gui2/widgets/image.hpp"
#include "utils/gui2/page.hpp"

#include <string>
#include <vector>

using namespace blunted;

// One actor on the side-select screen: a controller plugged into this machine
// (LocalDevice) or a lobby peer connected over the network (RemotePeer). Both
// the offline controller-select and the mirrored LAN side screen render from
// this single model.
struct SideSelectParticipant {
  bool remote = false;     // false: local HID device, true: remote lobby peer
  int id = 0;              // controller index (local) or lobby player id (remote)
  int device = 0;          // 0 = keyboard, 1 = gamepad
  int joystickID = 0;      // local gamepad stable id, 0 for keyboard
  int side = 0;            // UI side: -1 home, 0 spectator/center, +1 away
  bool ready = false;
  bool canControl = false; // this client may change this participant
  bool isLocalPeer = false;
  bool isGamepad = false;
  int layout = 0;          // e_ControllerLayout (local devices only)
  std::string label;
};

// Data + policy behind the side-select screen. The page is pure presentation and
// input; every concrete backend decides who the participants are, how a
// side/ready change is applied and what leaving/committing means.
class SideSelectBackend {

  public:
    virtual ~SideSelectBackend() {}

    virtual bool IsLocal() const = 0;
    virtual bool IsHost() const = 0;
    virtual bool IsInGame() const = 0; // side select over a live match
    virtual std::vector<SideSelectParticipant> GetParticipants() = 0;

    virtual void SetSide(int id, int side) = 0;
    virtual void SetReady(int id, bool ready) = 0;
    virtual void SetDevice(int device) {}
    virtual void ToggleLayout(int id) {}
    virtual bool AllReady() = 0;

    virtual void Commit() = 0; // apply the selection (no navigation)
    virtual void Cancel() = 0; // leave the screen (no navigation)

    virtual void Process() {}
    virtual int PollTransition() { return -1; } // page id to switch to, else -1
    virtual bool PollClosed() { return false; } // overlay should GoBack
};

class LocalSideSelectBackend : public SideSelectBackend {

  public:
    LocalSideSelectBackend(bool inGame, bool resumeOnClose);

    bool IsLocal() const override { return true; }
    bool IsHost() const override { return true; }
    bool IsInGame() const override { return inGame; }
    std::vector<SideSelectParticipant> GetParticipants() override;

    void SetSide(int id, int side) override;
    void SetReady(int id, bool ready) override;
    void ToggleLayout(int id) override;
    bool AllReady() override;

    void Commit() override;
    void Cancel() override;

  protected:
    void BuildParticipants();
    void ApplyControllerSetup();

    bool inGame;
    bool resumeOnClose;
    bool built;
    std::vector<SideSelectParticipant> participants;
};

class NetworkSideSelectBackend : public SideSelectBackend {

  public:
    explicit NetworkSideSelectBackend(bool resume);

    bool IsLocal() const override { return false; }
    bool IsHost() const override;
    bool IsInGame() const override { return resume; }
    std::vector<SideSelectParticipant> GetParticipants() override;

    void SetSide(int id, int side) override;
    void SetReady(int id, bool ready) override;
    void SetDevice(int device) override;
    bool AllReady() override;

    void Commit() override;
    void Cancel() override;

    void Process() override;
    int PollTransition() override;
    bool PollClosed() override;

  protected:
    int GetLocalPlayerId();
    void SendAction(int type, int side, int value);

    bool resume;
    bool sawSideSelect;
    bool committed;
};

class SideSelectPage : public Gui2Page {

  public:
    SideSelectPage(Gui2WindowManager *windowManager, const Gui2PageData &pageData);
    virtual ~SideSelectPage();

    virtual void Process();
    virtual void ProcessKeyboardEvent(KeyboardEvent *event);
    virtual void ProcessJoystickEvent(JoystickEvent *event);
    virtual void ProcessWindowingEvent(WindowingEvent *event);

  protected:
    void RebuildRows();
    void RefreshDeviceIcons();
    void SetImagePositions();
    void SetReadyIcon(int row, bool ready);
    void DrawPixelLine(boost::intrusive_ptr<Image2D> img, int x0, int y0, int x1, int y1, const Vector3 &color);
    void ChangeSide(int participantId, int delta);
    bool CheckAllConfirmed(); // returns true if the page navigated away
    void Leave();

    SideSelectBackend *backend;
    bool inGame;
    bool resumeOnClose;
    bool committed;

    int sentDevice;
    unsigned long lastGamepadSideChange_ms;

    std::vector<SideSelectParticipant> participants;
    std::vector<Gui2Image*> rowImages;
    std::vector<Gui2Caption*> rowNames;
    std::vector<Gui2Image*> rowReadyIcons;
    std::vector<bool> rowReadyState;
    std::vector<int> rowDevice;
    std::vector<unsigned long> rowDelay;

    Gui2Image *background;
    Gui2Caption *homeCaption;
    Gui2Caption *awayCaption;
    Gui2Caption *phaseCaption;
    Gui2Caption *helpCaption;

};

#endif
