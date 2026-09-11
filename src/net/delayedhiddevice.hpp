#ifndef _HPP_DELAYEDHIDDEVICE
#define _HPP_DELAYEDHIDDEVICE

#include <deque>
#include <string>

#include <boost/thread/mutex.hpp>

#include "hid/ihidevice.hpp"

// Host-side fairness wrapper: delays the host's own (local) input by N game
// ticks so every peer's input is applied 2U after the button press (U = worst
// one-way delay). Samples the wrapped device every Process() and replays a
// sample that is `delayTicks` old. Does not own the wrapped device.
class DelayedHIDDevice : public IHIDevice {

  public:
    DelayedHIDDevice(IHIDevice *source);
    virtual ~DelayedHIDDevice();

    void SetDelayTicks(int ticks) { delayTicks = ticks < 0 ? 0 : ticks; }
    int GetDelayTicks() const { return delayTicks; }

    // The wrapped device (owned by the global controllers list).
    IHIDevice *GetSource() const { return source; }

    virtual void LoadConfig() { source->LoadConfig(); }
    virtual void SaveConfig() { source->SaveConfig(); }

    virtual void Process();

    virtual bool GetButton(e_ButtonFunction buttonFunction);
    virtual float GetButtonValue(e_ButtonFunction buttonFunction);
    virtual void SetButton(e_ButtonFunction buttonFunction, bool state);
    virtual bool GetPreviousButtonState(e_ButtonFunction buttonFunction);
    virtual Vector3 GetDirection();

    virtual unsigned int GetOwnerId() const { return source->GetOwnerId(); }

  private:
    struct Sample {
      bool buttons[e_ButtonFunction_Size];
      Vector3 direction;
    };

    IHIDevice *source;
    int delayTicks;
    std::deque<Sample> history;

    bool current[e_ButtonFunction_Size];
    bool previous[e_ButtonFunction_Size];
    Vector3 direction;
};

#endif
