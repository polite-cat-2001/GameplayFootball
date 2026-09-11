#include "netbuffer.hpp"

#include <cstring>

void NetBuffer::PutU8(uint8_t value) {
  data.push_back(value);
}

void NetBuffer::PutU16(uint16_t value) {
  data.push_back((uint8_t)(value & 0xFF));
  data.push_back((uint8_t)((value >> 8) & 0xFF));
}

void NetBuffer::PutU32(uint32_t value) {
  data.push_back((uint8_t)(value & 0xFF));
  data.push_back((uint8_t)((value >> 8) & 0xFF));
  data.push_back((uint8_t)((value >> 16) & 0xFF));
  data.push_back((uint8_t)((value >> 24) & 0xFF));
}

void NetBuffer::PutU64(uint64_t value) {
  for (int i = 0; i < 8; i++) {
    data.push_back((uint8_t)((value >> (i * 8)) & 0xFF));
  }
}

void NetBuffer::PutBool(bool value) {
  PutU8(value ? 1 : 0);
}

void NetBuffer::PutFloat(float value) {
  uint32_t bits;
  std::memcpy(&bits, &value, sizeof(bits));
  PutU32(bits);
}

void NetBuffer::PutString(const std::string &value) {
  PutU32((uint32_t)value.size());
  data.insert(data.end(), value.begin(), value.end());
}

void NetBuffer::PutVector3(const blunted::Vector3 &value) {
  PutFloat(value.coords[0]);
  PutFloat(value.coords[1]);
  PutFloat(value.coords[2]);
}

void NetBuffer::PutQuaternion(const blunted::Quaternion &value) {
  PutFloat(value.elements[0]);
  PutFloat(value.elements[1]);
  PutFloat(value.elements[2]);
  PutFloat(value.elements[3]);
}

uint8_t NetBuffer::GetU8() {
  if (readPos + 1 > data.size()) {
    failed = true;
    return 0;
  }
  return data[readPos++];
}

uint16_t NetBuffer::GetU16() {
  if (readPos + 2 > data.size()) {
    failed = true;
    return 0;
  }
  uint16_t value = (uint16_t)data[readPos] | ((uint16_t)data[readPos + 1] << 8);
  readPos += 2;
  return value;
}

uint32_t NetBuffer::GetU32() {
  if (readPos + 4 > data.size()) {
    failed = true;
    return 0;
  }
  uint32_t value = (uint32_t)data[readPos] |
                   ((uint32_t)data[readPos + 1] << 8) |
                   ((uint32_t)data[readPos + 2] << 16) |
                   ((uint32_t)data[readPos + 3] << 24);
  readPos += 4;
  return value;
}

uint64_t NetBuffer::GetU64() {
  if (readPos + 8 > data.size()) {
    failed = true;
    return 0;
  }
  uint64_t value = 0;
  for (int i = 0; i < 8; i++) {
    value |= (uint64_t)data[readPos + i] << (i * 8);
  }
  readPos += 8;
  return value;
}

bool NetBuffer::GetBool() {
  return GetU8() != 0;
}

float NetBuffer::GetFloat() {
  uint32_t bits = GetU32();
  float value;
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

std::string NetBuffer::GetString() {
  uint32_t length = GetU32();
  if (failed || readPos + length > data.size()) {
    failed = true;
    return std::string();
  }
  std::string value((const char *)&data[readPos], length);
  readPos += length;
  return value;
}

blunted::Vector3 NetBuffer::GetVector3() {
  blunted::Vector3 value;
  value.coords[0] = GetFloat();
  value.coords[1] = GetFloat();
  value.coords[2] = GetFloat();
  return value;
}

blunted::Quaternion NetBuffer::GetQuaternion() {
  blunted::Quaternion value;
  value.elements[0] = GetFloat();
  value.elements[1] = GetFloat();
  value.elements[2] = GetFloat();
  value.elements[3] = GetFloat();
  return value;
}
