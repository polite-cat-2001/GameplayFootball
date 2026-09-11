#include "delayedhiddevice.hpp"

DelayedHIDDevice::DelayedHIDDevice(IHIDevice *source) : source(source), delayTicks(0), direction(0) {
  this->deviceType = source->GetDeviceType();
  this->identifier = source->GetIdentifier();
  for (int i = 0; i < e_ButtonFunction_Size; i++) {
    current[i] = false;
    previous[i] = false;
  }
}

DelayedHIDDevice::~DelayedHIDDevice() {
}

void DelayedHIDDevice::Process() {
  Sample sample;
  for (int i = 0; i < e_ButtonFunction_Size; i++) {
    sample.buttons[i] = source->GetButton((e_ButtonFunction)i);
  }
  sample.direction = source->GetDirection();
  history.push_back(sample);

  // Keep exactly delayTicks + 1 samples: front() is then delayTicks ticks old.
  while ((int)history.size() > delayTicks + 1) history.pop_front();

  for (int i = 0; i < e_ButtonFunction_Size; i++) previous[i] = current[i];

  if ((int)history.size() > delayTicks) {
    const Sample &delayed = history.front();
    for (int i = 0; i < e_ButtonFunction_Size; i++) current[i] = delayed.buttons[i];
    direction = delayed.direction;
  }
}

bool DelayedHIDDevice::GetButton(e_ButtonFunction buttonFunction) {
  return current[buttonFunction];
}

float DelayedHIDDevice::GetButtonValue(e_ButtonFunction buttonFunction) {
  return current[buttonFunction] ? 1.0f : 0.0f;
}

void DelayedHIDDevice::SetButton(e_ButtonFunction buttonFunction, bool state) {
  current[buttonFunction] = state;
}

bool DelayedHIDDevice::GetPreviousButtonState(e_ButtonFunction buttonFunction) {
  return previous[buttonFunction];
}

Vector3 DelayedHIDDevice::GetDirection() {
  return direction;
}
