#include "nethiddevice.hpp"

NetHIDDevice::NetHIDDevice(const std::string &identifier, e_HIDeviceType type, unsigned int ownerId) : ownerId(ownerId), pending(false) {
  this->deviceType = type;
  this->identifier = identifier;
  direction = Vector3(0);
  pendingDirection = Vector3(0);
  lastInputTime_ms = 0;
  for (int i = 0; i < e_ButtonFunction_Size; i++) {
    current[i] = false;
    previous[i] = false;
    pendingButtons[i] = false;
  }
}

NetHIDDevice::~NetHIDDevice() {
}

void NetHIDDevice::Process() {
  boost::mutex::scoped_lock lock(mutex);

  for (int i = 0; i < e_ButtonFunction_Size; i++) previous[i] = current[i];

  if (pending) {
    for (int i = 0; i < e_ButtonFunction_Size; i++) current[i] = pendingButtons[i];
    direction = pendingDirection;
    pending = false;
    lastInputTime_ms = 0;
  } else {
    // The game ticks at 100 Hz; if no frame arrived for ~200 ms (packet loss or
    // a stalled client), release everything instead of leaving keys stuck.
    lastInputTime_ms++;
    if (lastInputTime_ms > 20) {
      for (int i = 0; i < e_ButtonFunction_Size; i++) current[i] = false;
      direction = Vector3(0);
    }
  }
}

bool NetHIDDevice::GetButton(e_ButtonFunction buttonFunction) {
  boost::mutex::scoped_lock lock(mutex);
  return current[buttonFunction];
}

float NetHIDDevice::GetButtonValue(e_ButtonFunction buttonFunction) {
  boost::mutex::scoped_lock lock(mutex);
  return current[buttonFunction] ? 1.0f : 0.0f;
}

void NetHIDDevice::SetButton(e_ButtonFunction buttonFunction, bool state) {
  boost::mutex::scoped_lock lock(mutex);
  current[buttonFunction] = state;
}

bool NetHIDDevice::GetPreviousButtonState(e_ButtonFunction buttonFunction) {
  boost::mutex::scoped_lock lock(mutex);
  return previous[buttonFunction];
}

Vector3 NetHIDDevice::GetDirection() {
  boost::mutex::scoped_lock lock(mutex);
  return direction;
}

void NetHIDDevice::SetInput(const NetInputFrame &frame) {
  boost::mutex::scoped_lock lock(mutex);
  for (int i = 0; i < e_ButtonFunction_Size; i++) {
    pendingButtons[i] = (frame.buttons & (1u << i)) != 0;
  }
  pendingDirection = frame.direction;
  pending = true;
}

void NetHIDDevice::Clear() {
  boost::mutex::scoped_lock lock(mutex);
  pending = false;
  direction = Vector3(0);
  for (int i = 0; i < e_ButtonFunction_Size; i++) {
    current[i] = false;
    previous[i] = false;
  }
}
