#ifndef _HPP_NETHIDDEVICE
#define _HPP_NETHIDDEVICE

#include <string>

#include "nettypes.hpp"

#include "hid/ihidevice.hpp"

// Host-side virtual input device: fed by InputFrame messages from one client
// and handed to the team exactly like a local keyboard/gamepad. The network
// thread writes the last received frame; the game thread consumes it in
// Process() so button edges behave as usual.
class NetHIDDevice : public IHIDevice {

  public:
    NetHIDDevice(const std::string &identifier, e_HIDeviceType type, unsigned int ownerId);
    virtual ~NetHIDDevice();

    virtual unsigned int GetOwnerId() const { return ownerId; }

    virtual void LoadConfig() {}
    virtual void SaveConfig() {}

    virtual void Process();

    virtual bool GetButton(e_ButtonFunction buttonFunction);
    virtual float GetButtonValue(e_ButtonFunction buttonFunction);
    virtual void SetButton(e_ButtonFunction buttonFunction, bool state);
    virtual bool GetPreviousButtonState(e_ButtonFunction buttonFunction);
    virtual Vector3 GetDirection();

    void SetInput(const NetInputFrame &frame); // safe from the network thread
    void Clear();

    // Set-piece taker menu (#30): returns the squad slot the client chose (-1 none) and only once
    // per distinct value, so a slot held across frames is applied a single time and a later change
    // by anyone else is not snapped back.
    int ConsumeSetPieceTakerSlot();

  private:
    unsigned int ownerId;
    bool current[e_ButtonFunction_Size];
    bool previous[e_ButtonFunction_Size];
    Vector3 direction;
    int takerSlot;
    int consumedTakerSlot;

    bool pending;
    bool pendingButtons[e_ButtonFunction_Size];
    Vector3 pendingDirection;
    int pendingTakerSlot;
    unsigned long lastInputTime_ms;
};

#endif
