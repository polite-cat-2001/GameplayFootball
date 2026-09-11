#ifndef _HPP_NETMATCHSESSION
#define _HPP_NETMATCHSESSION

#include <deque>
#include <boost/shared_ptr.hpp>

#include "nettypes.hpp"

class Match;
class NetServer;
class NetClient;
class DelayedHIDDevice;

// Explicit in-match network state, derived from the authoritative inputs
// (Match::pause + LobbyState.sideSelect) instead of scattered flags. Drives
// which overlay the menu layer must show.
enum e_NetMatchPhaseState {
  e_NetMatchPhaseState_Playing = 0, // no pause overlay
  e_NetMatchPhaseState_Paused,      // pause menu; resume vote in progress
  e_NetMatchPhaseState_SideSelect   // mirrored side selection; all-ready resumes
};

// Owns everything the thin client / host session does each game tick during a
// network match: input delay (fairness), snapshot relay, roster changes
// (disconnect/join), pause votes and environment sync. It never touches the GUI;
// the caller (GameTask) opens the overlay indicated by GetState().
class NetMatchSession {

  public:
    NetMatchSession();
    ~NetMatchSession();

    bool IsActive() const; // a NetServer or NetClient is present
    bool IsHost() const;
    bool IsClient() const;

    // Called when a Match has been created (host or client). The host binds
    // controllers and publishes the animation table + environment.
    void StartMatch(Match *match);
    void StopMatch();

    // One game tick of network logic. For the host this runs the simulation
    // (Match::Process); both roles must then lock their Put mutex, call
    // Match::PreparePutBuffers(), and finally call BroadcastSnapshot() (host).
    void Process(Match *match);

    // Host only: rate-limited snapshot capture + broadcast. Must run after
    // PreparePutBuffers so the render poses are captured.
    void BroadcastSnapshot(Match *match);

    e_NetMatchPhaseState GetState() const;

    // Host only: (re)bind input devices from the lobby. RebindControllers also
    // resumes the match (live side change after a disconnect or the pause menu).
    void SetupControllers(Match *match);
    void RebindControllers(Match *match);

  private:
    void ProcessHost(Match *match);
    void ProcessClient(Match *match);
    void HandleRosterChanges(Match *match);
    bool SideSelectActive() const;
    void ApplyInterpolatedSnapshot(Match *match);

    Match *match; // borrowed from GameTask
    boost::shared_ptr<DelayedHIDDevice> hostInputDelay;
    std::deque<NetInputFrame> clientInputQueue;
    std::deque<NetRawSnapshot> snapshotQueue; // client: decoded pending snapshots
    // Fixed-rate playout clock (client): advances with real time and is gently
    // pulled toward (newest host snapshot time - B). Decoupled from packet
    // arrival jitter, which would otherwise show up as shaky playback.
    double renderHostTime;
    unsigned long lastRenderClock_ms;
    bool renderClockInit;
    unsigned long lastSnapshotTime_ms;
};

#endif
