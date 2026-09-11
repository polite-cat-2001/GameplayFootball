#ifndef _HPP_NETUDP
#define _HPP_NETUDP

#include <cstddef>
#include <cstdint>
#include <vector>

// Realtime datagrams (input client->host, snapshots host->client) travel over
// UDP, separate from the reliable TCP control channel. A fixed little-endian
// header lets a receiver identify the session and drop stale/reordered packets:
//
//   u16 magic | u8 type | u32 sessionId | u32 seq | payload
//
// The server learns a client's UDP endpoint from these datagrams (Hello / the
// first input), then sends snapshots back to that endpoint. Snapshot 'seq' is
// only for diagnostics / future delta compression.
const uint16_t net_udpMagic = 0x5546; // "FU"
const size_t net_udpHeaderSize = 11;

enum e_NetUdpType {
  e_NetUdpType_Hello = 1,
  e_NetUdpType_Input,
  e_NetUdpType_Snapshot
};

inline void NetWriteUdpHeader(std::vector<uint8_t> &out, uint8_t type, uint32_t sessionId, uint32_t seq) {
  out.resize(net_udpHeaderSize);
  out[0] = (uint8_t)(net_udpMagic & 0xFF);
  out[1] = (uint8_t)((net_udpMagic >> 8) & 0xFF);
  out[2] = type;
  out[3] = (uint8_t)(sessionId & 0xFF);
  out[4] = (uint8_t)((sessionId >> 8) & 0xFF);
  out[5] = (uint8_t)((sessionId >> 16) & 0xFF);
  out[6] = (uint8_t)((sessionId >> 24) & 0xFF);
  out[7] = (uint8_t)(seq & 0xFF);
  out[8] = (uint8_t)((seq >> 8) & 0xFF);
  out[9] = (uint8_t)((seq >> 16) & 0xFF);
  out[10] = (uint8_t)((seq >> 24) & 0xFF);
}

inline bool NetReadUdpHeader(const uint8_t *data, size_t size, uint8_t &type, uint32_t &sessionId, uint32_t &seq) {
  if (size < net_udpHeaderSize) return false;
  if ((uint16_t)(data[0] | (data[1] << 8)) != net_udpMagic) return false;
  type = data[2];
  sessionId = (uint32_t)data[3] | ((uint32_t)data[4] << 8) | ((uint32_t)data[5] << 16) | ((uint32_t)data[6] << 24);
  seq = (uint32_t)data[7] | ((uint32_t)data[8] << 8) | ((uint32_t)data[9] << 16) | ((uint32_t)data[10] << 24);
  return true;
}

#endif
