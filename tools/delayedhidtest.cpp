#include "net/delayedhiddevice.hpp"

#include <cstdio>
#include <cstdlib>

namespace {

void Check(bool condition, const char *message) {
  if (condition) return;
  std::fprintf(stderr, "delayedhidtest failed: %s\n", message);
  std::abort();
}

class FakeHIDDevice : public IHIDevice {
 public:
  FakeHIDDevice() : direction(0) {
    deviceType = e_HIDeviceType_Gamepad;
    identifier = "delayedhidtest";
    for (int i = 0; i < e_ButtonFunction_Size; ++i) {
      buttons[i] = false;
      previousButtons[i] = false;
    }
  }

  void LoadConfig() override {}
  void SaveConfig() override {}
  void Process() override {}
  bool GetButton(e_ButtonFunction function) override { return buttons[function]; }
  float GetButtonValue(e_ButtonFunction function) override { return buttons[function] ? 1.0f : 0.0f; }
  void SetButton(e_ButtonFunction function, bool state) override { buttons[function] = state; }
  bool GetPreviousButtonState(e_ButtonFunction function) override { return previousButtons[function]; }
  Vector3 GetDirection() override { return direction; }

 private:
  bool buttons[e_ButtonFunction_Size];
  bool previousButtons[e_ButtonFunction_Size];
  Vector3 direction;
};

void ButtonReleaseIsNotDrainedUntilItLeavesTheDelayQueue() {
  FakeHIDDevice source;
  DelayedHIDDevice delayed(&source);
  delayed.SetDelayTicks(2);

  Check(delayed.IsInputDrained(), "an idle device starts drained");

  source.SetButton(e_ButtonFunction_ShortPass, true);
  delayed.Process();
  Check(!delayed.IsInputDrained(), "physical confirm press is pending");

  source.SetButton(e_ButtonFunction_ShortPass, false);
  delayed.Process();
  Check(!delayed.IsInputDrained(), "release has not passed the buffered press");
  delayed.Process();
  Check(!delayed.IsInputDrained(), "buffered press has reached gameplay");
  delayed.Process();
  Check(!delayed.IsInputDrained(), "gameplay still reports the previous pressed state");
  delayed.Process();
  Check(delayed.IsInputDrained(), "press and release have fully drained");
}

} // namespace

int main() {
  ButtonReleaseIsNotDrainedUntilItLeavesTheDelayQueue();
}
