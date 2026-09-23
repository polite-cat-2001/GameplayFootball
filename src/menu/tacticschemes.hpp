// written by bastiaan konings schuiling 2008 - 2015
// this work is public domain. the code is undocumented, scruffy, untested, and should generally not be used for anything important.
// i do not offer support, so don't ask. to be used for inspiration :)

#ifndef _HPP_TACTICSSCHEMES
#define _HPP_TACTICSSCHEMES

#include <string>
#include <vector>

#include "../gamedefines.hpp"

class Team;
class TeamData;

// One role/position of a hardcoded tactical scheme. Positions are in database
// space (x: -1 own goal .. 1 opponent goal, y: -1 .. 1).
struct TacticalSchemeEntry {
  e_PlayerRole role;
  Vector3 databasePosition;
};

// A named formation (4-4-2, 4-3-3, ...): playerNum entries, goalkeeper first.
struct TacticalScheme {
  std::string name;
  std::vector<TacticalSchemeEntry> entries;
};

// The built-in scheme list shown in the game plan's Tactics section.
const std::vector<TacticalScheme> &GetTacticalSchemes();

// One on-pitch player considered for a scheme: slot is the index into the
// team's player list; position is the current formation position.
struct SchemeCandidate {
  int slot;
  e_PlayerRole role;
  Vector3 position;
};

// Assigns the scheme's positions to the candidates, pinning the goalkeeper and
// otherwise minimising total distance + role mismatch (Hungarian method).
// Returns, per candidate, the assigned FormationEntry (databasePosition and the
// role-adapted position).
std::vector<FormationEntry> AssignScheme(const std::vector<SchemeCandidate> &candidates, const TacticalScheme &scheme);

// Index of the scheme matching the given formation set (role + position), or -1.
int MatchScheme(const std::vector<FormationEntry> &formation);

// Applies a scheme to a pre-match team (writes the assigned entries to TeamData)
// or to a live team (sets runtime formation overrides for the active players).
void ApplySchemeToTeamData(TeamData *teamData, int schemeIndex);
void ApplySchemeToTeam(Team *team, int schemeIndex);

#endif
