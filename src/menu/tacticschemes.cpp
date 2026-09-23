// written by bastiaan konings schuiling 2008 - 2015
// this work is public domain. the code is undocumented, scruffy, untested, and should generally not be used for anything important.
// i do not offer support, so don't ask. to be used for inspiration :)

#include "tacticschemes.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <utility>

#include "../misc/hungarian.h"

#include "../data/teamdata.hpp"
#include "../onthepitch/team.hpp"
#include "../onthepitch/player/player.hpp"

namespace {

typedef std::pair<e_PlayerRole, Vector3> SchemeEntryDef;

TacticalScheme MakeScheme(const std::string &name, const std::vector<SchemeEntryDef> &entries) {
  TacticalScheme scheme;
  scheme.name = name;
  for (unsigned int i = 0; i < entries.size(); i++) {
    TacticalSchemeEntry entry;
    entry.role = entries.at(i).first;
    entry.databasePosition = entries.at(i).second;
    scheme.entries.push_back(entry);
  }
  return scheme;
}

FormationEntry MakeFormationEntry(const TacticalSchemeEntry &entry) {
  FormationEntry fe;
  fe.role = entry.role;
  fe.databasePosition = entry.databasePosition;
  // Same role adaptation TeamData applies to database formations.
  fe.position = entry.databasePosition * 0.6f + GetDefaultRolePosition(entry.role) * 0.4f;
  return fe;
}

int RoleLine(e_PlayerRole role) {
  switch (role) {
    case e_PlayerRole_GK: return 0;
    case e_PlayerRole_CB:
    case e_PlayerRole_LB:
    case e_PlayerRole_RB: return 1;
    case e_PlayerRole_DM:
    case e_PlayerRole_CM:
    case e_PlayerRole_LM:
    case e_PlayerRole_RM:
    case e_PlayerRole_AM: return 2;
    case e_PlayerRole_CF:
    case e_PlayerRole_LW:
    case e_PlayerRole_RW:
    case e_PlayerRole_ST: return 3;
    default: return 2;
  }
}

// Cost added on top of the positional distance so a defender is not pushed up
// front while a striker drops into the back line.
int RolePenalty(e_PlayerRole a, e_PlayerRole b) {
  if (a == b) return 0;
  int lineA = RoleLine(a), lineB = RoleLine(b);
  if (lineA == 0 || lineB == 0) return 8;
  if (lineA == lineB) return 2;
  return 6;
}

void BuildSchemes(std::vector<TacticalScheme> &schemes) {
  schemes.push_back(MakeScheme("4-4-2", {
    {e_PlayerRole_GK, Vector3(-1.0f,  0.0f, 0)},
    {e_PlayerRole_LB, Vector3(-0.7f,  0.6f, 0)},
    {e_PlayerRole_CB, Vector3(-0.8f,  0.2f, 0)},
    {e_PlayerRole_CB, Vector3(-0.8f, -0.2f, 0)},
    {e_PlayerRole_RB, Vector3(-0.7f, -0.6f, 0)},
    {e_PlayerRole_LM, Vector3( 0.0f,  0.9f, 0)},
    {e_PlayerRole_DM, Vector3(-0.3f,  0.0f, 0)},
    {e_PlayerRole_RM, Vector3( 0.0f, -0.9f, 0)},
    {e_PlayerRole_AM, Vector3( 0.3f,  0.0f, 0)},
    {e_PlayerRole_CF, Vector3( 0.8f,  0.3f, 0)},
    {e_PlayerRole_CF, Vector3( 0.8f, -0.3f, 0)}
  }));

  schemes.push_back(MakeScheme("4-3-3", {
    {e_PlayerRole_GK, Vector3(-1.0f,  0.0f, 0)},
    {e_PlayerRole_LB, Vector3(-0.7f,  0.75f, 0)},
    {e_PlayerRole_CB, Vector3(-1.0f,  0.25f, 0)},
    {e_PlayerRole_CB, Vector3(-1.0f, -0.25f, 0)},
    {e_PlayerRole_RB, Vector3(-0.7f, -0.75f, 0)},
    {e_PlayerRole_CM, Vector3( 0.0f,  0.5f, 0)},
    {e_PlayerRole_CM, Vector3(-0.2f,  0.0f, 0)},
    {e_PlayerRole_CM, Vector3( 0.0f, -0.5f, 0)},
    {e_PlayerRole_LM, Vector3( 0.6f,  0.75f, 0)},
    {e_PlayerRole_CF, Vector3( 1.0f,  0.0f, 0)},
    {e_PlayerRole_RM, Vector3( 0.6f, -0.75f, 0)}
  }));

  schemes.push_back(MakeScheme("4-2-3-1", {
    {e_PlayerRole_GK, Vector3(-1.0f,  0.0f, 0)},
    {e_PlayerRole_LB, Vector3(-0.7f,  0.75f, 0)},
    {e_PlayerRole_CB, Vector3(-1.0f,  0.25f, 0)},
    {e_PlayerRole_CB, Vector3(-1.0f, -0.25f, 0)},
    {e_PlayerRole_RB, Vector3(-0.7f, -0.75f, 0)},
    {e_PlayerRole_CM, Vector3(-0.2f,  0.3f, 0)},
    {e_PlayerRole_CM, Vector3(-0.2f, -0.3f, 0)},
    {e_PlayerRole_LM, Vector3( 0.7f,  0.9f, 0)},
    {e_PlayerRole_AM, Vector3( 0.2f,  0.0f, 0)},
    {e_PlayerRole_RM, Vector3( 0.7f, -0.9f, 0)},
    {e_PlayerRole_CF, Vector3( 1.0f,  0.0f, 0)}
  }));

  schemes.push_back(MakeScheme("3-5-2", {
    {e_PlayerRole_GK, Vector3(-1.0f,  0.0f, 0)},
    {e_PlayerRole_CB, Vector3(-1.0f,  0.35f, 0)},
    {e_PlayerRole_CB, Vector3(-1.0f,  0.0f, 0)},
    {e_PlayerRole_CB, Vector3(-1.0f, -0.35f, 0)},
    {e_PlayerRole_LM, Vector3( 0.0f,  0.9f, 0)},
    {e_PlayerRole_CM, Vector3(-0.1f,  0.3f, 0)},
    {e_PlayerRole_DM, Vector3(-0.3f,  0.0f, 0)},
    {e_PlayerRole_CM, Vector3(-0.1f, -0.3f, 0)},
    {e_PlayerRole_RM, Vector3( 0.0f, -0.9f, 0)},
    {e_PlayerRole_CF, Vector3( 0.8f,  0.3f, 0)},
    {e_PlayerRole_CF, Vector3( 0.8f, -0.3f, 0)}
  }));

  schemes.push_back(MakeScheme("5-3-2", {
    {e_PlayerRole_GK, Vector3(-1.0f,  0.0f, 0)},
    {e_PlayerRole_LB, Vector3(-0.6f,  0.75f, 0)},
    {e_PlayerRole_CB, Vector3(-1.0f,  0.35f, 0)},
    {e_PlayerRole_CB, Vector3(-1.0f,  0.0f, 0)},
    {e_PlayerRole_CB, Vector3(-1.0f, -0.35f, 0)},
    {e_PlayerRole_RB, Vector3(-0.6f, -0.75f, 0)},
    {e_PlayerRole_CM, Vector3( 0.0f,  0.5f, 0)},
    {e_PlayerRole_CM, Vector3(-0.2f,  0.0f, 0)},
    {e_PlayerRole_CM, Vector3( 0.0f, -0.5f, 0)},
    {e_PlayerRole_CF, Vector3( 0.8f,  0.3f, 0)},
    {e_PlayerRole_CF, Vector3( 0.8f, -0.3f, 0)}
  }));

  schemes.push_back(MakeScheme("4-5-1", {
    {e_PlayerRole_GK, Vector3(-1.0f,  0.0f, 0)},
    {e_PlayerRole_LB, Vector3(-0.7f,  0.75f, 0)},
    {e_PlayerRole_CB, Vector3(-1.0f,  0.25f, 0)},
    {e_PlayerRole_CB, Vector3(-1.0f, -0.25f, 0)},
    {e_PlayerRole_RB, Vector3(-0.7f, -0.75f, 0)},
    {e_PlayerRole_LM, Vector3( 0.1f,  0.9f, 0)},
    {e_PlayerRole_CM, Vector3(-0.2f,  0.3f, 0)},
    {e_PlayerRole_DM, Vector3(-0.4f,  0.0f, 0)},
    {e_PlayerRole_CM, Vector3(-0.2f, -0.3f, 0)},
    {e_PlayerRole_RM, Vector3( 0.1f, -0.9f, 0)},
    {e_PlayerRole_CF, Vector3( 0.9f,  0.0f, 0)}
  }));
}

}

const std::vector<TacticalScheme> &GetTacticalSchemes() {
  static std::vector<TacticalScheme> schemes;
  if (schemes.empty()) BuildSchemes(schemes);
  return schemes;
}

std::vector<FormationEntry> AssignScheme(const std::vector<SchemeCandidate> &candidates, const TacticalScheme &scheme) {
  std::vector<FormationEntry> result(candidates.size());
  std::vector<bool> assigned(candidates.size(), false);

  if (candidates.empty() || scheme.entries.empty()) return result;

  // The goalkeeper never changes role.
  int gkCandidate = -1;
  for (unsigned int i = 0; i < candidates.size(); i++) {
    if (candidates.at(i).role == e_PlayerRole_GK) { gkCandidate = (int)i; break; }
  }
  int gkEntry = -1;
  for (unsigned int j = 0; j < scheme.entries.size(); j++) {
    if (scheme.entries.at(j).role == e_PlayerRole_GK) { gkEntry = (int)j; break; }
  }
  if (gkCandidate >= 0 && gkEntry >= 0) {
    result.at(gkCandidate) = MakeFormationEntry(scheme.entries.at(gkEntry));
    assigned.at(gkCandidate) = true;
  }

  std::vector<int> candidateIndices, entryIndices;
  for (unsigned int i = 0; i < candidates.size(); i++) if ((int)i != gkCandidate) candidateIndices.push_back((int)i);
  for (unsigned int j = 0; j < scheme.entries.size(); j++) if ((int)j != gkEntry) entryIndices.push_back((int)j);

  int n = (int)candidateIndices.size();
  int m = (int)entryIndices.size();
  if (n > 0 && m > 0) {
    // Square matrix: rows = scheme entries, columns = candidates, padded with a
    // high cost so real pairings are always preferred over dummy ones.
    int size = std::max(n, m);
    const int padCost = 100000;
    std::vector<int> costs(size * size, padCost);
    for (int ci = 0; ci < n; ci++) {
      for (int ej = 0; ej < m; ej++) {
        const SchemeCandidate &candidate = candidates.at(candidateIndices.at(ci));
        const TacticalSchemeEntry &entry = scheme.entries.at(entryIndices.at(ej));
        float distance = (candidate.position - entry.databasePosition).GetLength();
        costs[ej * size + ci] = int(std::round(distance * 10.0f)) + RolePenalty(candidate.role, entry.role);
      }
    }

    int **matrix = array_to_matrix(&costs[0], size, size);
    hungarian_problem_t problem;
    hungarian_init(&problem, matrix, size, size, HUNGARIAN_MODE_MINIMIZE_COST);
    hungarian_solve(&problem);
    for (int ci = 0; ci < n; ci++) {
      for (int ej = 0; ej < m; ej++) {
        if (problem.assignment[ej][ci] == 1) {
          result.at(candidateIndices.at(ci)) = MakeFormationEntry(scheme.entries.at(entryIndices.at(ej)));
          assigned.at(candidateIndices.at(ci)) = true;
        }
      }
    }
    hungarian_free(&problem);
    for (int i = 0; i < size; i++) free(matrix[i]);
    free(matrix);
  }

  // Fallback for a candidate left over (more players than scheme slots): keep
  // its current role at its current position.
  for (unsigned int i = 0; i < candidates.size(); i++) {
    if (assigned.at(i)) continue;
    result.at(i).role = candidates.at(i).role;
    result.at(i).databasePosition = candidates.at(i).position;
    result.at(i).position = candidates.at(i).position;
  }
  return result;
}

int MatchScheme(const std::vector<FormationEntry> &formation) {
  const std::vector<TacticalScheme> &schemes = GetTacticalSchemes();
  for (unsigned int s = 0; s < schemes.size(); s++) {
    const TacticalScheme &scheme = schemes.at(s);
    if (formation.size() != scheme.entries.size()) continue;
    std::vector<bool> used(scheme.entries.size(), false);
    bool matches = true;
    for (unsigned int f = 0; f < formation.size() && matches; f++) {
      int found = -1;
      for (unsigned int j = 0; j < scheme.entries.size(); j++) {
        if (used.at(j)) continue;
        if (scheme.entries.at(j).role != formation.at(f).role) continue;
        if ((scheme.entries.at(j).databasePosition - formation.at(f).databasePosition).GetLength() >= 0.02f) continue;
        found = (int)j;
        break;
      }
      if (found < 0) matches = false;
      else used.at(found) = true;
    }
    if (matches) return (int)s;
  }
  return -1;
}

void ApplySchemeToTeamData(TeamData *teamData, int schemeIndex) {
  const std::vector<TacticalScheme> &schemes = GetTacticalSchemes();
  if (!teamData || schemeIndex < 0 || schemeIndex >= (int)schemes.size()) return;

  std::vector<SchemeCandidate> candidates;
  for (int i = 0; i < playerNum; i++) {
    FormationEntry fe = teamData->GetFormationEntry(i);
    SchemeCandidate candidate;
    candidate.slot = i;
    candidate.role = fe.role;
    candidate.position = fe.databasePosition;
    candidates.push_back(candidate);
  }

  std::vector<FormationEntry> assignment = AssignScheme(candidates, schemes.at(schemeIndex));
  for (unsigned int i = 0; i < assignment.size(); i++) {
    teamData->SetFormationEntry(candidates.at(i).slot, assignment.at(i));
  }
}

void ApplySchemeToTeam(Team *team, int schemeIndex) {
  const std::vector<TacticalScheme> &schemes = GetTacticalSchemes();
  if (!team || schemeIndex < 0 || schemeIndex >= (int)schemes.size()) return;

  const std::vector<Player*> &all = team->GetAllPlayers();
  std::vector<SchemeCandidate> candidates;
  for (unsigned int i = 0; i < all.size(); i++) {
    if (!all.at(i)->IsActive()) continue;
    FormationEntry fe = team->GetFormationEntry(all.at(i)->GetID());
    SchemeCandidate candidate;
    candidate.slot = (int)i;
    candidate.role = fe.role;
    candidate.position = fe.databasePosition;
    candidates.push_back(candidate);
  }

  std::vector<FormationEntry> assignment = AssignScheme(candidates, schemes.at(schemeIndex));
  for (unsigned int i = 0; i < assignment.size(); i++) {
    team->SetRuntimeFormationEntry(all.at(candidates.at(i).slot)->GetID(), assignment.at(i));
  }
}
