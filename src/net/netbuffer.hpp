#ifndef _HPP_NETBUFFER
#define _HPP_NETBUFFER

#include <cstdint>
#include <string>
#include <vector>

#include "base/math/vector3.hpp"
#include "base/math/quaternion.hpp"

void NetWriteU32LE(uint8_t *bytes, uint32_t value);
uint32_t NetReadU32LE(const uint8_t *bytes);

class NetBuffer {

  public:
    NetBuffer() : readPos(0), failed(false) {}

    void PutU8(uint8_t value);
    void PutU16(uint16_t value);
    void PutU32(uint32_t value);
    void PutU64(uint64_t value);
    void PutBool(bool value);
    void PutFloat(float value);
    void PutString(const std::string &value);
    void PutVector3(const blunted::Vector3 &value);
    void PutQuaternion(const blunted::Quaternion &value);

    uint8_t GetU8();
    uint16_t GetU16();
    uint32_t GetU32();
    uint64_t GetU64();
    bool GetBool();
    float GetFloat();
    std::string GetString();
    blunted::Vector3 GetVector3();
    blunted::Quaternion GetQuaternion();

    bool Failed() const { return failed; }
    size_t Remaining() const { return readPos <= data.size() ? data.size() - readPos : 0; }
    void ResetRead() { readPos = 0; failed = false; }
    void Clear() { data.clear(); readPos = 0; failed = false; }

    const std::vector<uint8_t> &Data() const { return data; }
    std::vector<uint8_t> &Data() { return data; }

  private:
    std::vector<uint8_t> data;
    size_t readPos;
    bool failed;
};

#endif
