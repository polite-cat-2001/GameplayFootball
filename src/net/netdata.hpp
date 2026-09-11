#ifndef _HPP_NETDATA
#define _HPP_NETDATA

#include "netbuffer.hpp"

#include "data/playerdata.hpp"
#include "data/teamdata.hpp"

// Serializes the raw (database-shaped) representation of match participants so a
// client can rebuild TeamData/PlayerData without access to the host's database.
// Kept in the net layer and limited to the raw structs: it does not depend on
// the data classes' implementation, only on their declarations.

void SerializePlayerDataRaw(NetBuffer &buffer, const PlayerDataRaw &player);
PlayerDataRaw DeserializePlayerDataRaw(NetBuffer &buffer);

void SerializeTeamDataRaw(NetBuffer &buffer, const TeamDataRaw &team);
TeamDataRaw DeserializeTeamDataRaw(NetBuffer &buffer);

#endif
