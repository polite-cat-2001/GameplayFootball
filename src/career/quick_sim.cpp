#include "quick_sim.hpp"

#include <algorithm>
#include <map>

namespace blunted {
namespace career {

namespace {

real FatigueFactor(int fatigue) {
  return 0.55f + 0.45f * (float(clampValue(float(fatigue), 0.0f, 100.0f)) / 100.0f);
}

real FormationAttack(const std::string &formation) {
  if (formation == "4-3-3") return 1.05f;
  if (formation == "3-5-2") return 1.06f;
  if (formation == "4-2-3-1") return 1.03f;
  if (formation == "5-3-2") return 0.95f;
  return 1.0f;
}

real FormationDefense(const std::string &formation) {
  if (formation == "5-3-2") return 1.06f;
  if (formation == "4-2-3-1") return 1.04f;
  if (formation == "4-3-3") return 0.96f;
  if (formation == "3-5-2") return 0.97f;
  return 1.0f;
}

struct Strength {
  real attack = 0.0f;
  real midfield = 0.0f;
  real defense = 0.0f;
};

Strength ComputeStrength(const QuickSimTeam &team) {
  Strength result;
  if (team.startingXi.empty()) return result;

  real attackWeight = 0.0f;
  real attackSum = 0.0f;
  real defenseWeight = 0.0f;
  real defenseSum = 0.0f;
  real midfieldWeight = 0.0f;
  real midfieldSum = 0.0f;

  for (size_t i = 0; i < team.startingXi.size(); i++) {
    const QuickSimPlayer &player = team.startingXi[i];
    real rating = float(player.ovr) * FatigueFactor(player.fatigue);
    int a = PositionWeightAttack(player.position);
    int d = PositionWeightDefense(player.position);
    int m = IsMidfielder(player.position) ? 4 : 2;
    if (IsGoalkeeper(player.position)) d = 6;
    attackSum += rating * float(a);
    attackWeight += float(a);
    defenseSum += rating * float(d);
    defenseWeight += float(d);
    midfieldSum += rating * float(m);
    midfieldWeight += float(m);
  }

  result.attack = attackWeight > 0.0f ? attackSum / attackWeight : 0.0f;
  result.defense = defenseWeight > 0.0f ? defenseSum / defenseWeight : 0.0f;
  result.midfield = midfieldWeight > 0.0f ? midfieldSum / midfieldWeight : 0.0f;
  result.attack *= FormationAttack(team.formation);
  result.defense *= FormationDefense(team.formation);
  return result;
}

int WeightedPick(const std::vector<int> &weights, CareerRng &rng) {
  int total = 0;
  for (size_t i = 0; i < weights.size(); i++) total += std::max(0, weights[i]);
  if (total <= 0 || weights.empty()) return weights.empty() ? -1 : rng.Int(0, int(weights.size()) - 1);

  int roll = rng.Int(1, total);
  for (size_t i = 0; i < weights.size(); i++) {
    roll -= std::max(0, weights[i]);
    if (roll <= 0) return static_cast<int>(i);
  }
  return static_cast<int>(weights.size()) - 1;
}

int PickIndex(const std::vector<QuickSimPlayer> &players, int (*weightFn)(Position), CareerRng &rng) {
  std::vector<int> weights;
  weights.reserve(players.size());
  for (size_t i = 0; i < players.size(); i++) {
    int weight = weightFn(players[i].position) + 1;
    weights.push_back(weight);
  }
  return WeightedPick(weights, rng);
}

bool IsStartingGoalkeeper(const QuickSimTeam &team, int playerId) {
  if (team.startingXi.empty()) return false;
  return team.startingXi[0].id == playerId && IsGoalkeeper(team.startingXi[0].position);
}

int StartFatigue(const QuickSimTeam &team, int playerId) {
  for (size_t i = 0; i < team.startingXi.size(); i++) {
    if (team.startingXi[i].id == playerId) return team.startingXi[i].fatigue;
  }
  for (size_t i = 0; i < team.bench.size(); i++) {
    if (team.bench[i].id == playerId) return team.bench[i].fatigue;
  }
  return 100;
}

MatchLine *FindLine(std::vector<MatchLine> &lines, int playerId) {
  for (size_t i = 0; i < lines.size(); i++) {
    if (lines[i].playerId == playerId) return &lines[i];
  }
  return nullptr;
}

const QuickSimPlayer *FindPlayer(const QuickSimTeam &team, int playerId) {
  for (size_t i = 0; i < team.startingXi.size(); i++) {
    if (team.startingXi[i].id == playerId) return &team.startingXi[i];
  }
  for (size_t i = 0; i < team.bench.size(); i++) {
    if (team.bench[i].id == playerId) return &team.bench[i];
  }
  return nullptr;
}

void AddLines(std::vector<MatchLine> &lines, const QuickSimTeam &team, int clubId) {
  for (size_t i = 0; i < team.startingXi.size(); i++) {
    MatchLine line;
    line.playerId = team.startingXi[i].id;
    line.clubId = clubId;
    line.endFatigue = team.startingXi[i].fatigue;
    lines.push_back(line);
  }
}

void ApplySubstitutions(std::vector<MatchLine> &lines, const QuickSimTeam &team, int clubId, CareerRng &rng) {
  if (team.startingXi.empty()) return;

  int subs = 0;
  if (!team.bench.empty()) subs = rng.Int(1, std::min(3, static_cast<int>(team.bench.size())));

  for (int s = 0; s < subs && s < static_cast<int>(team.bench.size()); s++) {
    int minute = rng.Int(58, 74);
    int slot = rng.Int(1, static_cast<int>(team.startingXi.size()) - 1);
    if (FindLine(lines, team.startingXi[slot].id) == nullptr) continue;

    MatchLine *starter = FindLine(lines, team.startingXi[slot].id);
    if (starter == nullptr || starter->minutes > 0) continue;
    starter->minutes = minute;

    const QuickSimPlayer &sub = team.bench[s];
    MatchLine line;
    line.playerId = sub.id;
    line.clubId = clubId;
    line.minutes = 90 - minute;
    line.endFatigue = sub.fatigue;
    lines.push_back(line);
  }
}

void FinalizeMinutes(std::vector<MatchLine> &lines, const QuickSimTeam &team) {
  for (size_t i = 0; i < lines.size(); i++) {
    MatchLine &line = lines[i];
    if (line.minutes <= 0) line.minutes = 90;
    if (line.minutes > 90) line.minutes = 90;
    real cost = IsStartingGoalkeeper(team, line.playerId) ? 0.14f : 0.36f;
    real startFatigue = float(StartFatigue(team, line.playerId));
    line.endFatigue = static_cast<int>(std::lround(clampValue(startFatigue - float(line.minutes) * cost, 0.0f, 100.0f)));
  }
}

void ApplyCardsAndInjuries(std::vector<MatchLine> &lines, const QuickSimTeam &team, CareerRng &rng) {
  if (team.startingXi.empty()) return;

  if (rng.Chance(0.72f)) {
    int index = PickIndex(team.startingXi, PositionWeightCard, rng);
    if (index >= 0 && index < static_cast<int>(team.startingXi.size())) {
      MatchLine *line = FindLine(lines, team.startingXi[index].id);
      if (line) line->yellowCards++;
    }
  }
  if (rng.Chance(0.10f)) {
    int index = PickIndex(team.startingXi, PositionWeightCard, rng);
    if (index >= 0 && index < static_cast<int>(team.startingXi.size())) {
      MatchLine *line = FindLine(lines, team.startingXi[index].id);
      if (line) line->redCards = 1;
    }
  }

  if (rng.Chance(0.42f)) {
    int index = PickIndex(team.startingXi, PositionWeightCard, rng);
    if (index >= 0 && index < static_cast<int>(team.startingXi.size())) {
      const QuickSimPlayer &player = team.startingXi[index];
      MatchLine *line = FindLine(lines, player.id);
      if (line) {
        line->injured = true;
        line->critical = rng.Chance(0.35f);
        line->injuryType = line->critical ? "muscle" : "knock";
        line->injuryDays = line->critical ? rng.Int(14, 56) : rng.Int(3, 14);
      }
    }
  }
}

void PickScorer(const QuickSimTeam &team, std::vector<QuickSimPlayer> &onPitch, CareerRng &rng, int &scorerId,
                int &assistId) {
  scorerId = -1;
  assistId = -1;
  if (onPitch.empty()) return;

  std::vector<int> scorerWeights;
  for (size_t i = 0; i < onPitch.size(); i++) {
    int weight = (PositionWeightGoal(onPitch[i].position) + 1) * std::max(1, onPitch[i].ovr);
    scorerWeights.push_back(weight);
  }
  int scorerIndex = WeightedPick(scorerWeights, rng);
  if (scorerIndex >= 0 && scorerIndex < static_cast<int>(onPitch.size())) {
    scorerId = onPitch[scorerIndex].id;
    if (rng.Chance(0.68f)) {
      std::vector<int> assistWeights;
      for (size_t i = 0; i < onPitch.size(); i++) {
        if (onPitch[i].id == scorerId) {
          assistWeights.push_back(0);
          continue;
        }
        assistWeights.push_back((PositionWeightAssist(onPitch[i].position) + 1) * std::max(1, onPitch[i].ovr));
      }
      int assistIndex = WeightedPick(assistWeights, rng);
      if (assistIndex >= 0 && assistIndex < static_cast<int>(onPitch.size())) assistId = onPitch[assistIndex].id;
    }
  }
}

} // namespace

void QuickSim::Shootout(const QuickSimTeam &home, const QuickSimTeam &away, CareerRng &rng, int &homePenalties,
                        int &awayPenalties) {
  homePenalties = 0;
  awayPenalties = 0;

  real homeSkill = 0.0f;
  real awaySkill = 0.0f;
  for (size_t i = 0; i < home.startingXi.size(); i++) homeSkill += float(home.startingXi[i].ovr);
  for (size_t i = 0; i < away.startingXi.size(); i++) awaySkill += float(away.startingXi[i].ovr);
  homeSkill = home.startingXi.empty() ? 0.6f : homeSkill / float(home.startingXi.size()) / 100.0f;
  awaySkill = away.startingXi.empty() ? 0.6f : awaySkill / float(away.startingXi.size()) / 100.0f;

  real homeGoalChance = clampValue(0.58f + (homeSkill - 0.6f) * 0.5f, 0.45f, 0.85f);
  real awayGoalChance = clampValue(0.58f + (awaySkill - 0.6f) * 0.5f, 0.45f, 0.85f);

  for (int round = 0; round < 5; round++) {
    if (rng.Chance(homeGoalChance)) homePenalties++;
    if (rng.Chance(awayGoalChance)) awayPenalties++;
  }
  int guard = 0;
  while (homePenalties == awayPenalties && guard < 40) {
    int homeKick = rng.Chance(homeGoalChance) ? 1 : 0;
    int awayKick = rng.Chance(awayGoalChance) ? 1 : 0;
    homePenalties += homeKick;
    awayPenalties += awayKick;
    guard++;
  }
  if (homePenalties == awayPenalties) {
    // The guard is unreachable in practice; keep the contract total anyway.
    homePenalties++;
  }
}

QuickSimResult QuickSim::Play(int fixtureId, CompetitionType competition, const QuickSimTeam &home,
                              const QuickSimTeam &away, const CareerConfig &config, CareerRng &rng) {
  QuickSimResult result;
  result.outcome.fixtureId = fixtureId;
  result.outcome.homeClubId = home.clubId;
  result.outcome.awayClubId = away.clubId;

  AddLines(result.outcome.lines, home, home.clubId);
  AddLines(result.outcome.lines, away, away.clubId);
  ApplySubstitutions(result.outcome.lines, home, home.clubId, rng);
  ApplySubstitutions(result.outcome.lines, away, away.clubId, rng);
  FinalizeMinutes(result.outcome.lines, home);
  FinalizeMinutes(result.outcome.lines, away);

  Strength homeStrength = ComputeStrength(home);
  Strength awayStrength = ComputeStrength(away);

  std::vector<QuickSimPlayer> homeOnPitch = home.startingXi;
  std::vector<QuickSimPlayer> awayOnPitch = away.startingXi;
  for (size_t i = 0; i < home.bench.size() && i < 2; i++) homeOnPitch.push_back(home.bench[i]);
  for (size_t i = 0; i < away.bench.size() && i < 2; i++) awayOnPitch.push_back(away.bench[i]);

  int homeGoals = 0;
  int awayGoals = 0;
  int moments = std::max(1, config.quickSimMoments);

  for (int moment = 0; moment < moments; moment++) {
    real homeMid = homeStrength.midfield + float(config.homeAdvantage);
    real awayMid = awayStrength.midfield;
    real possession = homeMid / (homeMid + awayMid);
    bool homeAttacks = rng.Real() < possession;

    Strength &attacking = homeAttacks ? homeStrength : awayStrength;
    Strength &defending = homeAttacks ? awayStrength : homeStrength;
    if (homeAttacks) result.homeChances++;
    else result.awayChances++;

    real attackFactor = attacking.attack / std::max(1.0f, defending.defense);
    real chanceProbability = clampValue(0.44f + (attackFactor - 1.0f) * 0.35f, 0.22f, 0.72f);
    if (!rng.Chance(chanceProbability)) continue;

    real goalProbability = clampValue(0.34f + (attackFactor - 1.0f) * 0.22f, 0.16f, 0.58f);
    bool goal = rng.Chance(goalProbability);
    if (!goal) continue;

    if (homeAttacks) {
      homeGoals++;
      int scorerId = -1, assistId = -1;
      PickScorer(home, homeOnPitch, rng, scorerId, assistId);
      MatchLine *line = FindLine(result.outcome.lines, scorerId);
      if (line) line->goals++;
      line = FindLine(result.outcome.lines, assistId);
      if (line) line->assists++;
    } else {
      awayGoals++;
      int scorerId = -1, assistId = -1;
      PickScorer(away, awayOnPitch, rng, scorerId, assistId);
      MatchLine *line = FindLine(result.outcome.lines, scorerId);
      if (line) line->goals++;
      line = FindLine(result.outcome.lines, assistId);
      if (line) line->assists++;
    }
  }

  ApplyCardsAndInjuries(result.outcome.lines, home, rng);
  ApplyCardsAndInjuries(result.outcome.lines, away, rng);

  result.outcome.homeGoals = homeGoals;
  result.outcome.awayGoals = awayGoals;

  if (competition == e_Competition_Cup && homeGoals == awayGoals) {
    int homePenalties = 0, awayPenalties = 0;
    Shootout(home, away, rng, homePenalties, awayPenalties);
    result.outcome.homePenalties = homePenalties;
    result.outcome.awayPenalties = awayPenalties;
  }

  return result;
}

} // namespace career
} // namespace blunted
