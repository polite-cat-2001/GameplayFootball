#include "career.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>

#include <boost/uuid/detail/sha1.hpp>

namespace blunted {
namespace career {

namespace {

const int kPlayingMinutesPerSeason = 2600;

long long RoundToThousand(long long value) {
  return (value / 1000LL) * 1000LL;
}

} // namespace

Career::Career(uint32_t seed, const CareerConfig &config, CareerDataSource &dataSource)
    : seed_(seed), config_(config), rng_(seed ^ 0xa5a5a5a5u) {
  BuildInitialState(dataSource);
  GenerateSeason(config_.seasonStartYear);
  currentDate_ = seasons_[currentSeasonIndex_].startDate;
  windowOpen_ = IsTransferWindowOpen();
  ResetSeasonStats();
  IssueObjectives();
  AppendTranscript("CAREER seed=" + std::to_string(seed_) + " manager=" + manager_.name +
                   " club=" + ClubRef(manager_.clubId).name + " date=" + currentDate_.ToString());
}

void Career::BuildInitialState(CareerDataSource &dataSource) {
  std::vector<ClubSeed> clubSeeds = dataSource.LoadClubs();
  std::vector<PlayerSeed> freeAgents = dataSource.LoadFreeAgents();

  Date referenceDate(config_.seasonStartYear, config_.seasonStartMonth, config_.seasonStartDay);

  for (size_t i = 0; i < clubSeeds.size(); i++) {
    const ClubSeed &seed = clubSeeds[i];
    Club club;
    club.id = seed.id;
    club.name = seed.name;
    club.shortName = seed.shortName;
    club.division = seed.division;
    club.reputation = seed.reputation;
    club.subscribers = 80000 + seed.reputation * 2200;
    club.fanSatisfaction = 50;
    club.boardTrust = 55;
    club.formation = (i % 3 == 0) ? "4-3-3" : (i % 3 == 1 ? "4-4-2" : "4-2-3-1");
    for (size_t p = 0; p < seed.players.size(); p++) {
      PlayerSeed playerSeed = seed.players[p];
      playerSeed.clubId = seed.id;
      Player player;
      player.id = playerSeed.id;
      player.name = playerSeed.name;
      player.position = playerSeed.position;
      player.birthDate = playerSeed.birthDate;
      player.baseStat = playerSeed.baseStat;
      player.potentialOvr = playerSeed.potentialOvr;
      player.marketValue = playerSeed.marketValue;
      player.clubId = seed.id;
      player.contract.annualSalary = std::max(20000LL, RoundToThousand(player.marketValue / 9LL));
      player.contract.yearsRemaining = rng_.Int(1, 5);
      players_[player.id] = player;
    }
    clubs_[club.id] = club;
  }

  for (size_t i = 0; i < freeAgents.size(); i++) {
    PlayerSeed playerSeed = freeAgents[i];
    Player player;
    player.id = playerSeed.id;
    player.name = playerSeed.name;
    player.position = playerSeed.position;
    player.birthDate = playerSeed.birthDate;
    player.baseStat = playerSeed.baseStat;
    player.potentialOvr = playerSeed.potentialOvr;
    player.marketValue = playerSeed.marketValue;
    player.clubId = -1;
    player.contract.annualSalary = 0;
    player.contract.yearsRemaining = 0;
    players_[player.id] = player;
  }

  if (clubs_.empty()) {
    Club dummy;
    dummy.id = 1;
    dummy.name = "Dummy";
    dummy.division = 1;
    clubs_[1] = dummy;
  }

  // The manager starts in the lower division, at its first club, so promotion
  // and relegation are observable in a headless career.
  int managedClubId = clubs_.begin()->first;
  for (std::map<int, Club>::iterator it = clubs_.begin(); it != clubs_.end(); ++it) {
    if (it->second.division == 2) {
      managedClubId = it->first;
      break;
    }
  }
  manager_.name = "Alex Manager";
  manager_.nationality = "Syntheticland";
  manager_.reputation = 42;
  manager_.clubId = managedClubId;
  clubs_[managedClubId].isManaged = true;

  for (std::map<int, Club>::iterator it = clubs_.begin(); it != clubs_.end(); ++it) {
    long long squadValue = SquadValue(it->first);
    it->second.balance = squadValue * config_.startingBalanceFactor + config_.startingBalanceFloor;
    it->second.seasonStartBalance = it->second.balance;
  }
  nextPlayerId_ = 1;
  for (std::map<int, Player>::const_iterator it = players_.begin(); it != players_.end(); ++it) {
    nextPlayerId_ = std::max(nextPlayerId_, it->first + 1);
  }
  (void)referenceDate;
}

std::vector<Date> Career::WeekendDates(int year, int count) const {
  std::vector<Date> dates;
  Date cursor(year, config_.seasonStartMonth, config_.seasonStartDay);
  while (cursor.Weekday() != 6) cursor = cursor.AddDays(1);
  for (int i = 0; i < count; i++) dates.push_back(cursor.AddDays(i * 7));
  return dates;
}

std::vector<Date> Career::MidweekDates(int year, int count) const {
  std::vector<Date> dates;
  int leagueRounds = std::max(1, 2 * (config_.clubsPerDivision - 1));
  std::vector<Date> weekends = WeekendDates(year, leagueRounds);
  for (int i = 0; i < count; i++) {
    int anchor = std::min(leagueRounds - 1, 3 + i * 4);
    dates.push_back(weekends[anchor].AddDays(3));
  }
  std::sort(dates.begin(), dates.end());
  dates.erase(std::unique(dates.begin(), dates.end(), [](const Date &a, const Date &b) { return a == b; }), dates.end());
  while (static_cast<int>(dates.size()) < count) {
    Date last = dates.empty() ? Date(year, config_.seasonStartMonth, config_.seasonStartDay) : dates.back();
    dates.push_back(last.AddDays(7 * (static_cast<int>(dates.size()) + 1)));
  }
  return dates;
}

void Career::GenerateSeason(int year) {
  Season season;
  season.year = year;
  int n = std::max(2, config_.clubsPerDivision);
  int leagueRounds = 2 * (n - 1);
  int totalClubs = static_cast<int>(clubs_.size());
  int cupRounds = 0;
  int bracketSize = 1;
  while (bracketSize < totalClubs) {
    bracketSize <<= 1;
    cupRounds++;
  }
  if (cupRounds < 1) cupRounds = 1;

  std::vector<Date> weekendDates = WeekendDates(year, leagueRounds);
  std::vector<Date> midweekDates = MidweekDates(year, cupRounds);

  for (size_t i = 0; i < weekendDates.size(); i++) {
    CalendarSlot slot;
    slot.date = weekendDates[i];
    slot.type = e_Slot_Weekend;
    season.slots.push_back(slot);
  }
  for (size_t i = 0; i < midweekDates.size(); i++) {
    CalendarSlot slot;
    slot.date = midweekDates[i];
    slot.type = e_Slot_Midweek;
    season.slots.push_back(slot);
  }
  Date break1 = (midweekDates.empty() ? weekendDates.back() : midweekDates.back()).AddDays(7);
  Date break2 = break1.AddDays(7);
  CalendarSlot breakSlot;
  breakSlot.date = break1;
  breakSlot.type = e_Slot_Break;
  season.slots.push_back(breakSlot);
  breakSlot.date = break2;
  season.slots.push_back(breakSlot);

  std::sort(season.slots.begin(), season.slots.end(),
            [](const CalendarSlot &a, const CalendarSlot &b) { return a.date < b.date; });

  season.startDate = Date(year, config_.seasonStartMonth, config_.seasonStartDay);
  season.endDate = season.slots.back().date;

  for (int division = 1; division <= 2; division++) {
    Competition competition;
    competition.id = year * 10 + division;
    competition.type = e_Competition_League;
    competition.division = division;
    competition.name = division == 1 ? "Synthetic Premier League" : "Synthetic Second Division";
    for (std::map<int, Club>::const_iterator it = clubs_.begin(); it != clubs_.end(); ++it) {
      if (it->second.division == division) competition.clubIds.push_back(it->first);
    }
    season.competitions.push_back(competition);
  }

  Competition cup;
  cup.id = year * 10 + 9;
  cup.type = e_Competition_Cup;
  cup.division = 0;
  cup.name = "Synthetic National Cup";
  for (std::map<int, Club>::const_iterator it = clubs_.begin(); it != clubs_.end(); ++it) {
    cup.clubIds.push_back(it->first);
  }
  season.competitions.push_back(cup);

  season.finished = false;
  seasons_.push_back(season);
  currentSeasonIndex_ = static_cast<int>(seasons_.size()) - 1;

  for (size_t i = 0; i < CurrentSeasonMutable().competitions.size(); i++) {
    Competition &competition = CurrentSeasonMutable().competitions[i];
    if (competition.type == e_Competition_League) {
      BuildLeagueSchedule(competition, weekendDates);
    } else {
      BuildCupFirstRound(competition, midweekDates);
    }
  }
}

void Career::BuildLeagueSchedule(Competition &competition, const std::vector<Date> &weekendDates) {
  std::vector<int> teams = competition.clubIds;
  rng_.Shuffle(teams);
  if (teams.size() < 2) return;

  int n = static_cast<int>(teams.size());
  if (n % 2 == 1) {
    for (int i = 1; i <= n; i++) {
      if (std::find(teams.begin(), teams.end(), -i) == teams.end()) {
        teams.push_back(-i);
        n++;
        break;
      }
    }
  }

  int rounds = n - 1;
  int secondHalf = rounds;
  for (int round = 0; round < rounds; round++) {
    for (int i = 0; i < n / 2; i++) {
      int a = teams[i];
      int b = teams[n - 1 - i];
      if (a <= 0 || b <= 0) continue;
      bool swap = ((round + i) % 2) == 1;
      Fixture fixture;
      fixture.id = nextFixtureId_++;
      fixture.competitionId = competition.id;
      fixture.round = round + 1;
      fixture.date = weekendDates[std::min(round, static_cast<int>(weekendDates.size()) - 1)];
      fixture.homeClubId = swap ? b : a;
      fixture.awayClubId = swap ? a : b;
      CurrentSeasonMutable().fixtures.push_back(fixture);

      Fixture reverse;
      reverse.id = nextFixtureId_++;
      reverse.competitionId = competition.id;
      reverse.round = secondHalf + round + 1;
      reverse.date = weekendDates[std::min(secondHalf + round, static_cast<int>(weekendDates.size()) - 1)];
      reverse.homeClubId = fixture.awayClubId;
      reverse.awayClubId = fixture.homeClubId;
      CurrentSeasonMutable().fixtures.push_back(reverse);
    }
    // Rotate all teams except the first.
    if (n > 2) {
      int last = teams.back();
      teams.pop_back();
      teams.insert(teams.begin() + 1, last);
    }
  }
}

void Career::BuildCupFirstRound(Competition &competition, const std::vector<Date> &midweekDates) {
  std::vector<int> entrants = competition.clubIds;
  rng_.Shuffle(entrants);
  int bracketSize = 1;
  while (bracketSize < static_cast<int>(entrants.size())) bracketSize <<= 1;
  while (static_cast<int>(entrants.size()) < bracketSize) entrants.push_back(-1);

  Date firstDate = midweekDates.empty() ? CurrentSeasonMutable().startDate : midweekDates[0];
  for (size_t i = 0; i < entrants.size(); i += 2) {
    int a = entrants[i];
    int b = entrants[i + 1];
    Fixture fixture;
    fixture.id = nextFixtureId_++;
    fixture.competitionId = competition.id;
    fixture.round = 1;
    fixture.date = firstDate;
    fixture.homeClubId = a;
    if (b == -1) {
      fixture.awayClubId = -1;
      fixture.bye = true;
      fixture.played = true;
      fixture.homeGoals = 0;
      fixture.awayGoals = 0;
    } else if (a == -1) {
      fixture.homeClubId = b;
      fixture.awayClubId = -1;
      fixture.bye = true;
      fixture.played = true;
      fixture.homeGoals = 0;
      fixture.awayGoals = 0;
    } else {
      fixture.awayClubId = b;
    }
    CurrentSeasonMutable().fixtures.push_back(fixture);
  }
}

void Career::MaybeCreateNextCupRound(Competition &competition) {
  int maxRound = 0;
  std::vector<Fixture *> roundFixtures;
  for (size_t i = 0; i < CurrentSeasonMutable().fixtures.size(); i++) {
    Fixture &fixture = CurrentSeasonMutable().fixtures[i];
    if (fixture.competitionId != competition.id) continue;
    maxRound = std::max(maxRound, fixture.round);
  }
  if (maxRound == 0) return;

  for (size_t i = 0; i < CurrentSeasonMutable().fixtures.size(); i++) {
    Fixture &fixture = CurrentSeasonMutable().fixtures[i];
    if (fixture.competitionId != competition.id || fixture.round != maxRound) continue;
    if (!fixture.played) return;
    roundFixtures.push_back(&fixture);
  }
  if (roundFixtures.size() <= 1) return;

  std::vector<Date> midweekDates = MidweekDates(CurrentSeasonMutable().year, maxRound + 1);
  if (maxRound >= static_cast<int>(midweekDates.size())) return;
  Date nextDate = midweekDates[maxRound];

  for (size_t i = 0; i + 1 < roundFixtures.size(); i += 2) {
    int winnerA = -1;
    int winnerB = -1;
    const Fixture &a = *roundFixtures[i];
    const Fixture &b = *roundFixtures[i + 1];
    if (a.bye) winnerA = a.homeClubId;
    else if (a.homeGoals > a.awayGoals) winnerA = a.homeClubId;
    else if (a.awayGoals > a.homeGoals) winnerA = a.awayClubId;
    else if (a.homePenalties > a.awayPenalties) winnerA = a.homeClubId;
    else if (a.awayPenalties > a.homePenalties) winnerA = a.awayClubId;

    if (b.bye) winnerB = b.homeClubId;
    else if (b.homeGoals > b.awayGoals) winnerB = b.homeClubId;
    else if (b.awayGoals > b.homeGoals) winnerB = b.awayClubId;
    else if (b.homePenalties > b.awayPenalties) winnerB = b.homeClubId;
    else if (b.awayPenalties > b.homePenalties) winnerB = b.awayClubId;

    if (winnerA < 0 || winnerB < 0) continue;
    Fixture fixture;
    fixture.id = nextFixtureId_++;
    fixture.competitionId = competition.id;
    fixture.round = maxRound + 1;
    fixture.date = nextDate;
    fixture.homeClubId = winnerA;
    fixture.awayClubId = winnerB;
    CurrentSeasonMutable().fixtures.push_back(fixture);
  }
}

std::vector<int> Career::ClubPlayerIds(int clubId) const {
  std::vector<int> result;
  for (std::map<int, Player>::const_iterator it = players_.begin(); it != players_.end(); ++it) {
    if (it->second.clubId == clubId && !it->second.isYouth) result.push_back(it->first);
  }
  return result;
}

std::vector<int> Career::BuildLineup(int clubId, CompetitionType competition) const {
  std::vector<const Player *> available;
  for (std::map<int, Player>::const_iterator it = players_.begin(); it != players_.end(); ++it) {
    const Player &player = it->second;
    if (player.clubId != clubId || player.isYouth) continue;
    if (!IsAvailable(player.condition, competition)) continue;
    available.push_back(&player);
  }
  std::sort(available.begin(), available.end(), [this](const Player *a, const Player *b) {
    int ovrA = a->OvrOn(currentDate_);
    int ovrB = b->OvrOn(currentDate_);
    if (ovrA != ovrB) return ovrA > ovrB;
    return a->id < b->id;
  });

  std::vector<int> lineup;
  std::vector<bool> used(available.size(), false);

  int gkIndex = -1;
  for (size_t i = 0; i < available.size(); i++) {
    if (IsGoalkeeper(available[i]->position)) {
      gkIndex = static_cast<int>(i);
      break;
    }
  }
  if (gkIndex >= 0) {
    lineup.push_back(available[gkIndex]->id);
    used[gkIndex] = true;
  }

  const int targetDefenders = 4;
  const int targetMidfielders = 4;
  const int targetForwards = 2;
  int defenders = 0, midfielders = 0, forwards = 0;

  for (int pass = 0; pass < 4 && static_cast<int>(lineup.size()) < 11; pass++) {
    for (size_t i = 0; i < available.size() && static_cast<int>(lineup.size()) < 11; i++) {
      if (used[i]) continue;
      const Player &player = *available[i];
      bool want = false;
      if (pass == 0 && IsDefender(player.position) && defenders < targetDefenders) {
        want = true;
        defenders++;
      } else if (pass == 1 && IsMidfielder(player.position) && midfielders < targetMidfielders) {
        want = true;
        midfielders++;
      } else if (pass == 2 && IsForward(player.position) && forwards < targetForwards) {
        want = true;
        forwards++;
      } else if (pass == 3) {
        want = true;
      }
      if (want) {
        lineup.push_back(player.id);
        used[i] = true;
      }
    }
  }

  return lineup;
}

bool Career::CanFieldTeam(int clubId, CompetitionType competition, std::string *why) const {
  std::vector<int> lineup = BuildLineup(clubId, competition);
  bool hasGoalkeeper = false;
  for (size_t i = 0; i < lineup.size(); i++) {
    const Player *player = FindPlayer(lineup[i]);
    if (player && IsGoalkeeper(player->position)) hasGoalkeeper = true;
  }
  if (static_cast<int>(lineup.size()) < 11) {
    if (why) *why = "fewer than 11 available players";
    return false;
  }
  if (!hasGoalkeeper) {
    if (why) *why = "no available goalkeeper";
    return false;
  }
  return true;
}

QuickSimTeam Career::MakeQuickSimTeam(int clubId, CompetitionType competition) const {
  QuickSimTeam team;
  const Club *club = FindClub(clubId);
  if (!club) return team;
  team.clubId = clubId;
  team.formation = club->formation;

  std::vector<int> lineup = BuildLineup(clubId, competition);
  std::set<int> starting;
  for (size_t i = 0; i < lineup.size(); i++) {
    const Player *player = FindPlayer(lineup[i]);
    if (!player) continue;
    QuickSimPlayer simPlayer;
    simPlayer.id = player->id;
    simPlayer.position = player->position;
    simPlayer.ovr = player->OvrOn(currentDate_);
    simPlayer.fatigue = player->condition.fatigue;
    team.startingXi.push_back(simPlayer);
    starting.insert(player->id);
  }

  std::vector<const Player *> bench;
  for (std::map<int, Player>::const_iterator it = players_.begin(); it != players_.end(); ++it) {
    const Player &player = it->second;
    if (player.clubId != clubId || player.isYouth) continue;
    if (!IsAvailable(player.condition, competition)) continue;
    if (starting.find(player.id) != starting.end()) continue;
    bench.push_back(&player);
  }
  std::sort(bench.begin(), bench.end(), [this](const Player *a, const Player *b) {
    int ovrA = a->OvrOn(currentDate_);
    int ovrB = b->OvrOn(currentDate_);
    if (ovrA != ovrB) return ovrA > ovrB;
    return a->id < b->id;
  });
  for (size_t i = 0; i < bench.size() && i < 7; i++) {
    QuickSimPlayer simPlayer;
    simPlayer.id = bench[i]->id;
    simPlayer.position = bench[i]->position;
    simPlayer.ovr = bench[i]->OvrOn(currentDate_);
    simPlayer.fatigue = bench[i]->condition.fatigue;
    team.bench.push_back(simPlayer);
  }
  return team;
}

Fixture *Career::FindFixture(int id) {
  for (size_t s = 0; s < seasons_.size(); s++) {
    for (size_t i = 0; i < seasons_[s].fixtures.size(); i++) {
      if (seasons_[s].fixtures[i].id == id) return &seasons_[s].fixtures[i];
    }
  }
  return nullptr;
}

const Fixture *Career::FindFixture(int id) const {
  for (size_t s = 0; s < seasons_.size(); s++) {
    for (size_t i = 0; i < seasons_[s].fixtures.size(); i++) {
      if (seasons_[s].fixtures[i].id == id) return &seasons_[s].fixtures[i];
    }
  }
  return nullptr;
}

Competition *Career::FindCompetition(int id) {
  for (size_t s = 0; s < seasons_.size(); s++) {
    for (size_t i = 0; i < seasons_[s].competitions.size(); i++) {
      if (seasons_[s].competitions[i].id == id) return &seasons_[s].competitions[i];
    }
  }
  return nullptr;
}

const Competition *Career::FindCompetition(int id) const {
  for (size_t s = 0; s < seasons_.size(); s++) {
    for (size_t i = 0; i < seasons_[s].competitions.size(); i++) {
      if (seasons_[s].competitions[i].id == id) return &seasons_[s].competitions[i];
    }
  }
  return nullptr;
}

bool Career::IsManagedClub(int clubId) const {
  const Club *club = FindClub(clubId);
  return club != nullptr && club->isManaged;
}

bool Career::IsTransferWindowOpen() const {
  int month = currentDate_.month;
  int day = currentDate_.day;
  if (month == 7 || month == 8) return true;
  if (month == 1 && day <= 31) return true;
  return false;
}

long long Career::SquadValue(int clubId) const {
  long long value = 0;
  for (std::map<int, Player>::const_iterator it = players_.begin(); it != players_.end(); ++it) {
    if (it->second.clubId == clubId) value += it->second.marketValue;
  }
  return value;
}

long long Career::ClubStrength(int clubId) const {
  const Club *club = FindClub(clubId);
  if (!club) return 0;
  long long value = SquadValue(clubId);
  int count = 0;
  long long ovrSum = 0;
  for (std::map<int, Player>::const_iterator it = players_.begin(); it != players_.end(); ++it) {
    if (it->second.clubId != clubId) continue;
    ovrSum += it->second.OvrOn(currentDate_);
    count++;
  }
  long long averageOvr = count > 0 ? ovrSum / count : 0;
  return value / 1000000LL + (3 - club->division) * 20LL + averageOvr;
}

bool Career::ApplyAction(const CareerAction &action, std::string *error) {
  if (action.type == e_Action_Advance) {
    Advance();
    return true;
  }
  if (!replaying_) journal_.push_back(action);

  switch (action.type) {
    case e_Action_SimNext: {
      Fixture *fixture = FindFixture(action.fixtureId);
      if (!fixture || fixture->played) {
        if (error) *error = "fixture is not playable";
        return false;
      }
      ResolveFixture(action.fixtureId);
      return true;
    }
    case e_Action_PlayNext: {
      Fixture *fixture = FindFixture(action.fixtureId);
      if (!fixture || fixture->played) {
        if (error) *error = "fixture is not playable";
        return false;
      }
      std::map<int, MatchOutcome>::const_iterator recorded = recordedOutcomes_.find(action.fixtureId);
      if (recorded != recordedOutcomes_.end()) {
        ResolveFixtureWithOutcome(action.fixtureId, recorded->second);
        return true;
      }
      const Competition *competition = FindCompetition(fixture->competitionId);
      CompetitionType type = competition ? competition->type : e_Competition_League;
      bool homeOk = CanFieldTeam(fixture->homeClubId, type);
      bool awayOk = CanFieldTeam(fixture->awayClubId, type);
      if (!homeOk || !awayOk) {
        int losing = !homeOk ? fixture->homeClubId : fixture->awayClubId;
        MatchOutcome outcome = BuildWalkoverOutcome(*fixture, losing);
        recordedOutcomes_[action.fixtureId] = outcome;
        ApplyOutcomeInternal(outcome, false);
        return true;
      }
      QuickSimResult result = QuickSim::Play(fixture->id, type, MakeQuickSimTeam(fixture->homeClubId, type),
                                             MakeQuickSimTeam(fixture->awayClubId, type), config_, rng_);
      recordedOutcomes_[action.fixtureId] = result.outcome;
      ApplyOutcomeInternal(result.outcome, false);
      return true;
    }
    case e_Action_Transfer:
      return TransferPlayer(action.playerId, action.amount, action.intValue, action.years, error);
    case e_Action_Sell:
      return SellPlayer(action.playerId, action.amount, error);
    case e_Action_Renew:
      return RenewPlayer(action.playerId, action.intValue, action.years, error);
    case e_Action_ScoutPlayer:
      return ScoutPlayer(action.playerId, action.scoutId, error);
    case e_Action_HireScout: {
      if (manager_.clubId < 0) {
        if (error) *error = "no club";
        return false;
      }
      int regularScouts = 0;
      for (size_t i = 0; i < scouts_.size(); i++) {
        if (!scouts_[i].youthScout) regularScouts++;
      }
      if (regularScouts >= config_.maxScouts) {
        if (error) *error = "scout limit reached";
        return false;
      }
      HireScoutInternal(action.years, action.intValue, action.amount, false, "");
      return true;
    }
    case e_Action_FireScout: {
      for (size_t i = 0; i < scouts_.size(); i++) {
        if (scouts_[i].id == action.scoutId) {
          AppendTranscript("FIRE SCOUT " + scouts_[i].name);
          scouts_.erase(scouts_.begin() + i);
          return true;
        }
      }
      if (error) *error = "scout not found";
      return false;
    }
    case e_Action_HireYouthScout:
      if (manager_.clubId < 0) {
        if (error) *error = "no club";
        return false;
      }
      for (size_t i = 0; i < scouts_.size(); i++) {
        if (scouts_[i].youthScout) {
          if (error) *error = "a youth scout is already employed";
          return false;
        }
      }
      HireScoutInternal(30, action.intValue, action.amount, true, action.text.empty() ? "Europe" : action.text);
      return true;
    case e_Action_SignYouth:
      return SignYouth(action.clubId, action.playerId, error);
    case e_Action_PromoteYouth:
      return PromoteYouth(action.playerId, action.amount, action.years, error);
    case e_Action_ChooseSponsor:
      return ChooseSponsor(action.intValue, error);
    case e_Action_AcceptJob:
      return AcceptJob(action.clubId, error);
    case e_Action_DeclineJob: {
      for (size_t i = 0; i < jobOffers_.size(); i++) {
        if (jobOffers_[i].clubId == action.clubId) {
          AppendTranscript("DECLINE JOB " + ClubRef(action.clubId).name);
          jobOffers_.erase(jobOffers_.begin() + i);
          return true;
        }
      }
      if (error) *error = "job offer not found";
      return false;
    }
    default:
      break;
  }
  return true;
}

bool Career::TransferPlayer(int playerId, long long fee, long long salary, int years, std::string *error) {
  if (manager_.clubId < 0) {
    if (error) *error = "no club";
    return false;
  }
  if (!windowOpen_) {
    if (error) *error = "transfer window is closed";
    return false;
  }
  std::map<int, Player>::iterator it = players_.find(playerId);
  if (it == players_.end()) {
    if (error) *error = "player not found";
    return false;
  }
  Player &player = it->second;
  if (player.clubId == manager_.clubId) {
    if (error) *error = "player already belongs to the club";
    return false;
  }
  if (player.isYouth) {
    if (error) *error = "youth players must be promoted, not transferred";
    return false;
  }
  Club &buyer = ClubRef(manager_.clubId);
  if (fee < 0 || fee > buyer.balance) {
    if (error) *error = "transfer fee exceeds the current balance";
    return false;
  }
  if (salary <= 0 || years < 1 || years > 5) {
    if (error) *error = "invalid contract";
    return false;
  }

  int sellerId = player.clubId;
  if (sellerId >= 0) {
    Club &seller = ClubRef(sellerId);
    seller.balance += fee;
    AppendTranscript("SELL " + currentDate_.ToString() + " " + player.name + " " + seller.shortName + " -> " +
                     buyer.shortName + " fee=" + std::to_string(fee));
  } else {
    AppendTranscript("SIGN " + currentDate_.ToString() + " " + player.name + " free agent -> " + buyer.shortName +
                     " fee=" + std::to_string(fee));
  }
  buyer.balance -= fee;
  player.clubId = buyer.id;
  player.contract.annualSalary = salary;
  player.contract.yearsRemaining = years;
  AppendNews(player.name + " joined the club for " + std::to_string(fee) + ".");
  return true;
}

bool Career::SellPlayer(int playerId, long long fee, std::string *error) {
  if (manager_.clubId < 0) {
    if (error) *error = "no club";
    return false;
  }
  if (!windowOpen_) {
    if (error) *error = "transfer window is closed";
    return false;
  }
  std::map<int, Player>::iterator it = players_.find(playerId);
  if (it == players_.end() || it->second.clubId != manager_.clubId) {
    if (error) *error = "player is not in the squad";
    return false;
  }
  if (fee <= 0) {
    if (error) *error = "invalid fee";
    return false;
  }
  Club &seller = ClubRef(manager_.clubId);
  std::vector<Club *> buyers;
  for (std::map<int, Club>::iterator c = clubs_.begin(); c != clubs_.end(); ++c) {
    if (!c->second.isManaged && c->second.balance >= fee) buyers.push_back(&c->second);
  }
  if (buyers.empty()) {
    if (error) *error = "no rival can afford the fee";
    return false;
  }
  Club *buyer = buyers[rng_.Int(0, static_cast<int>(buyers.size()) - 1)];
  buyer->balance -= fee;
  seller.balance += fee;
  it->second.clubId = buyer->id;
  it->second.contract.annualSalary = std::max(20000LL, fee / 6LL);
  it->second.contract.yearsRemaining = 2;
  AppendTranscript("SALE " + currentDate_.ToString() + " " + it->second.name + " " + seller.shortName + " -> " +
                   buyer->shortName + " fee=" + std::to_string(fee));
  AppendNews(it->second.name + " was sold for " + std::to_string(fee) + ".");
  return true;
}

bool Career::RenewPlayer(int playerId, long long salary, int years, std::string *error) {
  if (manager_.clubId < 0) {
    if (error) *error = "no club";
    return false;
  }
  std::map<int, Player>::iterator it = players_.find(playerId);
  if (it == players_.end() || it->second.clubId != manager_.clubId) {
    if (error) *error = "player is not in the squad";
    return false;
  }
  if (salary <= 0 || years < 1 || years > 5) {
    if (error) *error = "invalid contract";
    return false;
  }
  if (ClubRef(manager_.clubId).balance < salary / 12) {
    if (error) *error = "cannot afford the salary";
    return false;
  }
  it->second.contract.annualSalary = salary;
  it->second.contract.yearsRemaining = years;
  AppendTranscript("RENEW " + currentDate_.ToString() + " " + it->second.name + " salary=" +
                   std::to_string(salary) + " years=" + std::to_string(years));
  return true;
}

bool Career::ScoutPlayer(int playerId, int scoutId, std::string *error) {
  const Player *player = FindPlayer(playerId);
  if (!player) {
    if (error) *error = "player not found";
    return false;
  }
  Scout *scout = nullptr;
  for (size_t i = 0; i < scouts_.size(); i++) {
    if (scouts_[i].id == scoutId && !scouts_[i].youthScout) scout = &scouts_[i];
  }
  if (!scout) {
    if (error) *error = "scout not found";
    return false;
  }
  for (size_t i = 0; i < scoutReports_.size(); i++) {
    if (scoutReports_[i].playerId == playerId && !scoutReports_[i].ready) {
      if (error) *error = "a report is already in progress";
      return false;
    }
  }
  ScoutReport report;
  report.scoutId = scoutId;
  report.playerId = playerId;
  int days = std::max(3, config_.scoutReportBaseDays - scout->speedDays / 2);
  report.dueDate = currentDate_.AddDays(days);
  scoutReports_.push_back(report);
  AppendTranscript("SCOUT ASSIGN " + currentDate_.ToString() + " player=" + std::to_string(playerId) +
                   " due=" + report.dueDate.ToString());
  return true;
}

void Career::HireScoutInternal(int speedDays, int quality, long long salary, bool youth, const std::string &region) {
  Scout scout;
  scout.id = nextScoutId_++;
  scout.name = youth ? "Youth Scout " + std::to_string(scout.id) : "Scout " + std::to_string(scout.id);
  scout.speedDays = std::max(1, speedDays);
  scout.quality = static_cast<int>(clampValue(quality, 1, 100));
  scout.annualSalary = std::max(1000LL, salary);
  scout.youthScout = youth;
  scout.region = region;
  if (youth) {
    Date intakeDate(currentDate_.year, config_.academyIntakeMonth, config_.academyIntakeDay);
    if (intakeDate <= currentDate_) {
      intakeDate = Date(currentDate_.year + 1, config_.academyIntakeMonth, config_.academyIntakeDay);
    }
    scout.nextReportDate = intakeDate;
  }
  scouts_.push_back(scout);
  AppendTranscript("HIRE SCOUT " + currentDate_.ToString() + " " + scout.name + " quality=" +
                   std::to_string(scout.quality) + " salary=" + std::to_string(scout.annualSalary));
}

bool Career::SignYouth(int reportId, int playerId, std::string *error) {
  if (manager_.clubId < 0) {
    if (error) *error = "no club";
    return false;
  }
  AcademyReport *report = nullptr;
  for (size_t i = 0; i < academyReports_.size(); i++) {
    if (academyReports_[i].id == reportId) report = &academyReports_[i];
  }
  if (!report) {
    if (error) *error = "academy report not found";
    return false;
  }
  std::map<int, Player>::iterator it = players_.find(playerId);
  if (it == players_.end()) {
    if (error) *error = "youth player not found";
    return false;
  }
  if (std::find(report->playerIds.begin(), report->playerIds.end(), playerId) == report->playerIds.end()) {
    if (error) *error = "player is not in this report";
    return false;
  }
  if (std::find(academyPlayerIds_.begin(), academyPlayerIds_.end(), playerId) != academyPlayerIds_.end()) {
    if (error) *error = "player is already in the academy";
    return false;
  }
  Player &player = it->second;
  player.clubId = manager_.clubId;
  player.isYouth = true;
  player.youthClubId = manager_.clubId;
  player.contract.annualSalary = 5000;
  player.contract.yearsRemaining = 1;
  academyPlayerIds_.push_back(playerId);
  report->claimedByAcademy.push_back(playerId);
  AppendTranscript("YOUTH SIGN " + currentDate_.ToString() + " " + player.name + " ovr=" +
                   std::to_string(player.OvrOn(currentDate_)));
  return true;
}

bool Career::PromoteYouth(int playerId, long long salary, int years, std::string *error) {
  std::map<int, Player>::iterator it = players_.find(playerId);
  if (it == players_.end() || !it->second.isYouth || it->second.youthClubId != manager_.clubId) {
    if (error) *error = "player is not in the academy";
    return false;
  }
  int age = it->second.birthDate.AgeOn(currentDate_);
  if (age < 16) {
    if (error) *error = "youth player is younger than 16";
    return false;
  }
  if (salary <= 0 || years < 1 || years > 5) {
    if (error) *error = "invalid contract";
    return false;
  }
  it->second.isYouth = false;
  it->second.youthClubId = -1;
  it->second.clubId = manager_.clubId;
  it->second.contract.annualSalary = salary;
  it->second.contract.yearsRemaining = years;
  academyPlayerIds_.erase(std::remove(academyPlayerIds_.begin(), academyPlayerIds_.end(), playerId),
                          academyPlayerIds_.end());
  academyPromotionsThisSeason_++;
  AppendTranscript("YOUTH PROMOTE " + currentDate_.ToString() + " " + it->second.name + " salary=" +
                   std::to_string(salary));
  return true;
}

bool Career::ChooseSponsor(int index, std::string *error) {
  if (manager_.clubId < 0) {
    if (error) *error = "no club";
    return false;
  }
  if (index < 0 || index > 2) {
    if (error) *error = "invalid sponsor";
    return false;
  }
  if (sponsorChosenSeason_ == currentSeasonIndex_) {
    if (error) *error = "sponsor already chosen this season";
    return false;
  }
  Club &club = ClubRef(manager_.clubId);
  club.sponsorIndex = index;
  long long money = (long long)club.subscribers * (index == 0 ? 8 : (index == 1 ? 5 : 12));
  int fanDelta = index == 0 ? -4 : (index == 1 ? 3 : -8);
  club.fanSatisfaction = static_cast<int>(clampValue(club.fanSatisfaction + fanDelta, 0, 100));
  AddIncomeInternal(money);
  sponsorChosenSeason_ = currentSeasonIndex_;
  AppendTranscript("SPONSOR " + currentDate_.ToString() + " index=" + std::to_string(index) + " money=" +
                   std::to_string(money) + " fan=" + std::to_string(club.fanSatisfaction));
  return true;
}

void Career::GenerateAcademyReport() {
  Scout *youthScout = nullptr;
  for (size_t i = 0; i < scouts_.size(); i++) {
    if (scouts_[i].youthScout) youthScout = &scouts_[i];
  }
  if (!youthScout) return;

  for (size_t r = 0; r < academyReports_.size(); r++) {
    AcademyReport &old = academyReports_[r];
    if (old.created.AddDays(config_.youthReportIntervalDays) >= currentDate_) continue;
    for (size_t p = 0; p < old.playerIds.size(); p++) {
      int id = old.playerIds[p];
      if (std::find(old.claimedByAcademy.begin(), old.claimedByAcademy.end(), id) != old.claimedByAcademy.end()) continue;
      std::map<int, Player>::iterator it = players_.find(id);
      if (it != players_.end() && it->second.isYouth && it->second.youthClubId < 0) {
        AppendTranscript("YOUTH LOST " + currentDate_.ToString() + " " + it->second.name);
        players_.erase(it);
      }
    }
  }

  AcademyReport report;
  report.id = nextAcademyReportId_++;
  report.created = currentDate_;
  int count = rng_.Int(3, 5);
  for (int i = 0; i < count; i++) {
    Player youth;
    youth.id = nextPlayerId_++;
    youth.name = "Youth " + std::to_string(youth.id);
    youth.position = static_cast<Position>(rng_.Int(0, 12));
    int age = rng_.Int(13, 15);
    youth.birthDate = Date(currentDate_.year - age, rng_.Int(1, 12), rng_.Int(1, 28));
    real qualityBase = 0.22f + float(youthScout->quality) / 100.0f * 0.28f;
    youth.baseStat = clampValue(qualityBase + float(rng_.Int(-40, 40)) / 1000.0f, 0.15f, 0.62f);
    int currentOvr = youth.OvrOn(currentDate_);
    int baseOvr = static_cast<int>(std::lround(youth.baseStat * 120.0f));
    youth.potentialOvr = std::min(100, std::max(currentOvr + rng_.Int(8, 28), std::min(100, baseOvr + 4)));
    youth.marketValue = MarketValueForOvr(currentOvr);
    youth.clubId = -1;
    youth.isYouth = true;
    youth.youthClubId = -1;
    youth.contract.annualSalary = 0;
    youth.contract.yearsRemaining = 0;
    players_[youth.id] = youth;
    report.playerIds.push_back(youth.id);
  }
  academyReports_.push_back(report);
  AppendTranscript("YOUTH REPORT " + currentDate_.ToString() + " id=" + std::to_string(report.id) + " juniors=" +
                   std::to_string(report.playerIds.size()));
}

void Career::IssueObjectives() {
  Club *club = manager_.clubId >= 0 ? &ClubRef(manager_.clubId) : nullptr;
  if (!club) return;
  int baseReputation = club->reputation;

  for (int category = 0; category < 4; category++) {
    for (int urgency = 0; urgency < 3; urgency++) {
      Objective objective;
      objective.id = nextObjectiveId_++;
      objective.category = static_cast<ObjectiveCategory>(category);
      objective.urgency = static_cast<ObjectiveUrgency>(urgency);
      objective.issuedSeason = currentSeasonIndex_;
      objective.deadlineSeason = currentSeasonIndex_ + urgency;
      int scale = urgency + 1;

      switch (objective.category) {
        case e_Objective_Financial:
          objective.targetProfit = 100000LL * scale + club->seasonStartBalance / 20LL;
          objective.description = "Profit at least " + std::to_string(objective.targetProfit);
          break;
        case e_Objective_Image:
          objective.targetSubscribers = club->subscribers + 5000 * scale;
          objective.description = "Reach " + std::to_string(objective.targetSubscribers) + " subscribers";
          break;
        case e_Objective_Sporting:
          objective.targetPoints = 12 + 4 * scale + baseReputation / 6 + rng_.Int(-2, 2);
          objective.description = "Collect " + std::to_string(objective.targetPoints) + " league points";
          break;
        case e_Objective_Academy:
          objective.targetAcademyPromotions = scale;
          objective.description = "Promote " + std::to_string(scale) + " academy players";
          break;
      }

      objective.reward.trust = 3 * scale;
      objective.reward.fanSatisfaction = 2 * scale;
      objective.reward.nextBudget = 100000LL * scale;
      objective.reward.reputation = scale;
      objective.penalty.trust = -(3 * scale);
      objective.penalty.fanSatisfaction = -(2 * scale);
      objective.penalty.nextBudget = -(50000LL * scale);
      objective.penalty.reputation = -scale;
      objectives_.push_back(objective);
    }
  }
}

void Career::EvaluateObjectivesForSeason() {
  if (manager_.clubId < 0) return;
  Club &club = ClubRef(manager_.clubId);
  long long profit = club.balance - club.seasonStartBalance;
  std::vector<StandingRow> table = LeagueTable(club.division);
  int points = 0;
  for (size_t i = 0; i < table.size(); i++) {
    if (table[i].clubId == club.id) points = table[i].points;
  }

  for (size_t i = 0; i < objectives_.size(); i++) {
    Objective &objective = objectives_[i];
    if (objective.evaluated || objective.deadlineSeason != currentSeasonIndex_) continue;
    bool success = false;
    switch (objective.category) {
      case e_Objective_Financial:
        objective.progress = static_cast<int>(profit);
        success = profit >= objective.targetProfit;
        break;
      case e_Objective_Image:
        objective.progress = club.subscribers;
        success = club.subscribers >= objective.targetSubscribers;
        break;
      case e_Objective_Sporting:
        objective.progress = points;
        success = points >= objective.targetPoints;
        break;
      case e_Objective_Academy:
        objective.progress = academyPromotionsThisSeason_;
        success = academyPromotionsThisSeason_ >= objective.targetAcademyPromotions;
        break;
    }
    objective.evaluated = true;
    objective.completed = success;
    const ObjectiveDeltas &delta = success ? objective.reward : objective.penalty;
    club.boardTrust = static_cast<int>(clampValue(club.boardTrust + delta.trust, 0, 100));
    club.fanSatisfaction = static_cast<int>(clampValue(club.fanSatisfaction + delta.fanSatisfaction, 0, 100));
    nextSeasonBudget_ += delta.nextBudget;
    manager_.reputation = static_cast<int>(clampValue(manager_.reputation + delta.reputation, 0, 100));
    long long target = objective.targetPoints;
    if (objective.category == e_Objective_Financial) target = objective.targetProfit;
    if (objective.category == e_Objective_Image) target = objective.targetSubscribers;
    if (objective.category == e_Objective_Academy) target = objective.targetAcademyPromotions;
    AppendTranscript(std::string("OBJECTIVE ") + (success ? "MET " : "FAILED ") +
                     ObjectiveCategoryName(objective.category) + "/" + ObjectiveUrgencyName(objective.urgency) +
                     " progress=" + std::to_string(objective.progress) + " target=" + std::to_string(target) +
                     " trust=" + std::to_string(club.boardTrust));
  }
}

void Career::CheckSportingObjectives() {
  if (manager_.clubId < 0) return;
  const Club *club = FindClub(manager_.clubId);
  if (!club) return;
  std::vector<StandingRow> table = LeagueTable(club->division);
  int points = 0;
  for (size_t i = 0; i < table.size(); i++) {
    if (table[i].clubId == club->id) points = table[i].points;
  }
  int remaining = 0;
  const Season &season = CurrentSeason();
  for (size_t i = 0; i < season.fixtures.size(); i++) {
    const Fixture &fixture = season.fixtures[i];
    if (fixture.played) continue;
    if (fixture.homeClubId != club->id && fixture.awayClubId != club->id) continue;
    const Competition *competition = FindCompetition(fixture.competitionId);
    if (competition && competition->type == e_Competition_League) remaining++;
  }
  int maxPoints = points + remaining * 3;

  std::vector<Objective> replacements;
  for (size_t i = 0; i < objectives_.size(); i++) {
    Objective &objective = objectives_[i];
    if (objective.evaluated || objective.category != e_Objective_Sporting) continue;
    if (objective.deadlineSeason != currentSeasonIndex_) continue;
    if (maxPoints >= objective.targetPoints) continue;
    objective.evaluated = true;
    objective.failed = true;
    Club &mutableClub = ClubRef(manager_.clubId);
    mutableClub.boardTrust = static_cast<int>(clampValue(mutableClub.boardTrust + objective.penalty.trust, 0, 100));
    mutableClub.fanSatisfaction =
        static_cast<int>(clampValue(mutableClub.fanSatisfaction + objective.penalty.fanSatisfaction, 0, 100));
    nextSeasonBudget_ += objective.penalty.nextBudget;
    manager_.reputation = static_cast<int>(clampValue(manager_.reputation + objective.penalty.reputation, 0, 100));
    AppendTranscript("OBJECTIVE UNREACHABLE " + std::string(ObjectiveUrgencyName(objective.urgency)) +
                     " maxPoints=" + std::to_string(maxPoints) + " target=" + std::to_string(objective.targetPoints));
    Objective replacement = objective;
    replacement.id = nextObjectiveId_++;
    replacement.evaluated = false;
    replacement.completed = false;
    replacement.failed = false;
    int reissuedTarget = std::max(points + 1, objective.targetPoints - 5);
    if (reissuedTarget > maxPoints) reissuedTarget = maxPoints;
    replacement.targetPoints = std::max(points, reissuedTarget);
    replacement.progress = 0;
    replacements.push_back(replacement);
  }
  for (size_t i = 0; i < replacements.size(); i++) objectives_.push_back(replacements[i]);
}

void Career::DevelopPlayers() {
  const Date &end = CurrentSeason().endDate;
  for (std::map<int, Player>::iterator it = players_.begin(); it != players_.end(); ++it) {
    Player &player = it->second;
    int age = player.birthDate.AgeOn(end);
    int ovr = player.OvrOn(end);
    real base = player.baseStat;

    if (player.isYouth) {
      real potentialBase = std::max(base, float(player.potentialOvr) / 120.0f);
      player.baseStat = clampValue(base + (potentialBase - base) * 0.10f, 0.05f, potentialBase);
    } else if (player.clubId >= 0) {
      int peak = DevelopmentPeakAge(player.position);
      real playingFactor = clampValue(float(player.minutesThisSeason) / float(kPlayingMinutesPerSeason), 0.0f, 1.0f);
      real growthOvr = 0.0f;
      if (age < peak) {
        growthOvr = float(std::max(0, player.potentialOvr - ovr)) * (0.06f + 0.10f * playingFactor) + 0.4f +
                    1.6f * playingFactor;
      } else if (age <= peak + 2) {
        growthOvr = playingFactor * 0.5f - 0.3f;
      } else {
        growthOvr = -0.4f - float(age - peak - 2) * 0.35f;
      }
      real capBase = std::max(base, float(player.potentialOvr) / 120.0f);
      player.baseStat = clampValue(base + growthOvr / 120.0f, 0.05f, capBase);
    }
    player.marketValue = MarketValueForOvr(player.OvrOn(end));
    player.minutesThisSeason = 0;
    player.goalsThisSeason = 0;
    player.assistsThisSeason = 0;
  }
}

void Career::RetireAndIntake() {
  const Date &end = CurrentSeason().endDate;
  std::vector<int> retired;
  for (std::map<int, Player>::iterator it = players_.begin(); it != players_.end(); ++it) {
    Player &player = it->second;
    if (player.isYouth) continue;
    int age = player.birthDate.AgeOn(end);
    if (age >= 36 && rng_.Chance(0.30f)) retired.push_back(player.id);
  }
  for (size_t i = 0; i < retired.size(); i++) {
    std::map<int, Player>::iterator it = players_.find(retired[i]);
    if (it == players_.end()) continue;
    AppendTranscript("RETIRE " + end.ToString() + " " + it->second.name + " age=" +
                     std::to_string(it->second.birthDate.AgeOn(end)));
    players_.erase(it);
  }

  for (std::map<int, Club>::iterator it = clubs_.begin(); it != clubs_.end(); ++it) {
    if (it->second.isManaged) continue;
    int count = static_cast<int>(ClubPlayerIds(it->second.id).size());
    int intake = count < 18 ? 3 : 1;
    for (int i = 0; i < intake; i++) {
      Player youth;
      youth.id = nextPlayerId_++;
      youth.name = "Intake " + std::to_string(youth.id);
      youth.position = static_cast<Position>(rng_.Int(0, 12));
      int age = rng_.Int(16, 19);
      youth.birthDate = Date(end.year - age, rng_.Int(1, 12), rng_.Int(1, 28));
      youth.baseStat = clampValue(0.30f + float(it->second.reputation) / 100.0f * 0.25f +
                                      float(rng_.Int(-40, 40)) / 1000.0f,
                                  0.15f, 0.65f);
      int ovr = youth.OvrOn(end);
      int baseOvr = static_cast<int>(std::lround(youth.baseStat * 120.0f));
      youth.potentialOvr = std::min(100, std::max(ovr + rng_.Int(5, 20), std::min(100, baseOvr + 2)));
      youth.marketValue = MarketValueForOvr(ovr);
      youth.clubId = it->second.id;
      youth.contract.annualSalary = std::max(15000LL, youth.marketValue / 12LL);
      youth.contract.yearsRemaining = rng_.Int(1, 3);
      players_[youth.id] = youth;
    }
  }
}

void Career::SeasonTransition() {
  Season &season = CurrentSeasonMutable();
  if (season.finished) return;

  Club *managed = manager_.clubId >= 0 ? &ClubRef(manager_.clubId) : nullptr;
  if (managed) {
    long long shirtSales = 0;
    for (std::map<int, Player>::const_iterator it = players_.begin(); it != players_.end(); ++it) {
      if (it->second.clubId != managed->id || it->second.isYouth) continue;
      int ovr = it->second.OvrOn(season.endDate);
      shirtSales += (long long)std::max(0, ovr - 40) * managed->subscribers / 100;
    }
    AddIncomeInternal(shirtSales);
    AppendTranscript("SHIRTS " + season.endDate.ToString() + " income=" + std::to_string(shirtSales));
  }

  std::vector<StandingRow> division1 = LeagueTable(1);
  std::vector<StandingRow> division2 = LeagueTable(2);

  auto appendTable = [this, &season](int division, const std::vector<StandingRow> &table) {
    AppendTranscript("TABLE " + season.endDate.ToString() + " division=" + std::to_string(division));
    for (size_t i = 0; i < table.size(); i++) {
      const Club *club = FindClub(table[i].clubId);
      AppendTranscript("  " + std::to_string(i + 1) + " " + (club ? club->shortName : std::string("?")) +
                       " P" + std::to_string(table[i].played) + " W" + std::to_string(table[i].won) +
                       " D" + std::to_string(table[i].drawn) + " L" + std::to_string(table[i].lost) +
                       " GF" + std::to_string(table[i].goalsFor) + " GA" + std::to_string(table[i].goalsAgainst) +
                       " Pts" + std::to_string(table[i].points));
    }
  };
  appendTable(1, division1);
  appendTable(2, division2);
  int cupWinnerId = CupWinner();
  if (cupWinnerId >= 0) {
    const Club *cupClub = FindClub(cupWinnerId);
    if (cupClub) AppendTranscript("CUP WINNER " + season.endDate.ToString() + " " + cupClub->name);
  }

  if (managed) {
    int champion = division1.empty() ? -1 : division1[0].clubId;
    int cupWinner = CupWinner();
    if (champion == managed->id) {
      managed->subscribers += 15000;
      AppendTranscript("TROPHY " + season.endDate.ToString() + " league title subscribers=" +
                       std::to_string(managed->subscribers));
      AppendNews("The club won the league title.");
    }
    if (cupWinner == managed->id) {
      managed->subscribers += 10000;
      AppendTranscript("TROPHY " + season.endDate.ToString() + " cup subscribers=" +
                       std::to_string(managed->subscribers));
      AppendNews("The club won the national cup.");
    }
  }

  if (managed) {
    std::vector<StandingRow> &table = managed->division == 1 ? division1 : division2;
    int position = 0;
    for (size_t i = 0; i < table.size(); i++) {
      if (table[i].clubId == managed->id) position = static_cast<int>(i) + 1;
    }
    if (position > 0) {
      long long prize = (long long)(static_cast<int>(table.size()) - position + 1) * 200000LL;
      AddIncomeInternal(prize);
      AppendTranscript("PRIZE " + season.endDate.ToString() + " position=" + std::to_string(position) +
                       " income=" + std::to_string(prize));
    }
  }

  EvaluateObjectivesForSeason();
  CheckDismissal();

  if (division1.size() >= 2 && division2.size() >= 2) {
    for (int i = 0; i < 2 && i < static_cast<int>(division1.size()); i++) {
      Club &club = ClubRef(division1[division1.size() - 1 - i].clubId);
      club.division = 2;
      if (club.isManaged) AppendTranscript("RELEGATED " + club.name);
    }
    for (int i = 0; i < 2 && i < static_cast<int>(division2.size()); i++) {
      Club &club = ClubRef(division2[i].clubId);
      club.division = 1;
      if (club.isManaged) AppendTranscript("PROMOTED " + club.name);
    }
  }

  int expiredContracts = 0;
  for (std::map<int, Player>::iterator it = players_.begin(); it != players_.end(); ++it) {
    Player &player = it->second;
    if (player.isYouth || player.clubId < 0) continue;
    if (player.contract.yearsRemaining > 0) player.contract.yearsRemaining--;
    if (player.contract.yearsRemaining <= 0) {
      expiredContracts++;
      player.clubId = -1;
      player.contract.annualSalary = 0;
      player.contract.yearsRemaining = 0;
    }
  }
  if (expiredContracts > 0) {
    AppendTranscript("CONTRACT EXPIRED " + season.endDate.ToString() + " players=" +
                     std::to_string(expiredContracts));
  }

  RetireAndIntake();
  season.finished = true;
  DevelopPlayers();

  if (managed && manager_.clubId >= 0) {
    Club &club = ClubRef(manager_.clubId);
    club.balance += nextSeasonBudget_;
    AppendTranscript("BUDGET " + season.endDate.ToString() + " delta=" + std::to_string(nextSeasonBudget_) +
                     " balance=" + std::to_string(club.balance));
  }
  nextSeasonBudget_ = 0;
  academyPromotionsThisSeason_ = 0;

  int nextYear = season.year + 1;
  GenerateSeason(nextYear);
  ResetSeasonStats();
  if (manager_.clubId >= 0) {
    Club &club = ClubRef(manager_.clubId);
    club.seasonStartBalance = club.balance;
    club.subscribers += std::max(0, club.fanSatisfaction - 50) * 500;
    sponsorChosenSeason_ = -1;
  }
  IssueObjectives();
}

void Career::ResetSeasonStats() {
  for (std::map<int, Player>::iterator it = players_.begin(); it != players_.end(); ++it) {
    it->second.minutesThisSeason = 0;
    it->second.goalsThisSeason = 0;
    it->second.assistsThisSeason = 0;
  }
  academyPromotionsThisSeason_ = 0;
}

std::vector<StandingRow> Career::LeagueTable(int division) const {
  std::vector<StandingRow> table;
  const Season &season = CurrentSeason();
  const Competition *competition = nullptr;
  for (size_t i = 0; i < season.competitions.size(); i++) {
    if (season.competitions[i].type == e_Competition_League && season.competitions[i].division == division) {
      competition = &season.competitions[i];
    }
  }
  if (!competition) return table;

  std::map<int, StandingRow> rows;
  for (size_t i = 0; i < competition->clubIds.size(); i++) {
    StandingRow row;
    row.clubId = competition->clubIds[i];
    rows[row.clubId] = row;
  }

  for (size_t i = 0; i < season.fixtures.size(); i++) {
    const Fixture &fixture = season.fixtures[i];
    if (fixture.competitionId != competition->id || !fixture.played) continue;
    if (fixture.bye || fixture.awayClubId < 0) continue;
    StandingRow &home = rows[fixture.homeClubId];
    StandingRow &away = rows[fixture.awayClubId];
    home.played++;
    away.played++;
    home.goalsFor += fixture.homeGoals;
    home.goalsAgainst += fixture.awayGoals;
    away.goalsFor += fixture.awayGoals;
    away.goalsAgainst += fixture.homeGoals;
    if (fixture.homeGoals > fixture.awayGoals) {
      home.won++;
      home.points += 3;
      away.lost++;
    } else if (fixture.awayGoals > fixture.homeGoals) {
      away.won++;
      away.points += 3;
      home.lost++;
    } else {
      home.drawn++;
      away.drawn++;
      home.points++;
      away.points++;
    }
  }

  for (std::map<int, StandingRow>::const_iterator it = rows.begin(); it != rows.end(); ++it) {
    table.push_back(it->second);
  }
  std::sort(table.begin(), table.end(), [](const StandingRow &a, const StandingRow &b) {
    if (a.points != b.points) return a.points > b.points;
    if (a.GoalDifference() != b.GoalDifference()) return a.GoalDifference() > b.GoalDifference();
    if (a.goalsFor != b.goalsFor) return a.goalsFor > b.goalsFor;
    return a.clubId < b.clubId;
  });
  return table;
}

int Career::CupWinner() const {
  const Season &season = CurrentSeason();
  const Competition *cup = nullptr;
  for (size_t i = 0; i < season.competitions.size(); i++) {
    if (season.competitions[i].type == e_Competition_Cup) cup = &season.competitions[i];
  }
  if (!cup) return -1;
  int maxRound = 0;
  const Fixture *final = nullptr;
  for (size_t i = 0; i < season.fixtures.size(); i++) {
    const Fixture &fixture = season.fixtures[i];
    if (fixture.competitionId != cup->id) continue;
    if (fixture.round >= maxRound) {
      maxRound = fixture.round;
      final = &fixture;
    }
  }
  if (!final || !final->played) return -1;
  if (final->bye) return final->homeClubId;
  if (final->homeGoals > final->awayGoals) return final->homeClubId;
  if (final->awayGoals > final->homeGoals) return final->awayClubId;
  if (final->homePenalties > final->awayPenalties) return final->homeClubId;
  if (final->awayPenalties > final->homePenalties) return final->awayClubId;
  return -1;
}

void Career::DismissManager(const std::string &reason) {
  if (manager_.clubId < 0) return;
  int oldClubId = manager_.clubId;
  Club &oldClub = ClubRef(oldClubId);
  oldClub.isManaged = false;
  manager_.clubId = -1;
  pendingJobClubId_ = -1;
  AppendTranscript("DISMISSED " + currentDate_.ToString() + " " + oldClub.name + " reason=" + reason);
  AppendNews("Manager dismissed by " + oldClub.name + ".");
  BuildJobOffers(true);
}

void Career::CheckDismissal() {
  if (manager_.clubId < 0) return;
  const Club *club = FindClub(manager_.clubId);
  if (club && club->boardTrust < config_.trustDismissThreshold) DismissManager("board trust below threshold");
}

void Career::BuildJobOffers(bool includeFormerClub) {
  jobOffers_.clear();
  std::vector<std::pair<long long, int> > candidates;
  for (std::map<int, Club>::const_iterator it = clubs_.begin(); it != clubs_.end(); ++it) {
    if (!includeFormerClub && it->first == manager_.clubId) continue;
    candidates.push_back(std::make_pair(ClubStrength(it->first), it->first));
  }
  std::sort(candidates.begin(), candidates.end());
  Date effective = NextWindowStart(currentDate_);
  for (size_t i = 0; i < candidates.size() && jobOffers_.size() < 3; i++) {
    int clubId = candidates[i].second;
    int required = static_cast<int>(std::max(0LL, candidates[i].first - 20LL));
    if (required > manager_.reputation + 15) continue;
    JobOffer offer;
    offer.clubId = clubId;
    offer.effectiveDate = effective;
    offer.reputationRequired = required;
    offer.reason = "vacancy";
    jobOffers_.push_back(offer);
    AppendTranscript("VACANCY " + currentDate_.ToString() + " " + ClubRef(clubId).name + " required=" +
                     std::to_string(required));
  }
}

bool Career::AcceptJob(int clubId, std::string *error) {
  for (size_t i = 0; i < jobOffers_.size(); i++) {
    if (jobOffers_[i].clubId != clubId) continue;
    pendingJobClubId_ = clubId;
    pendingJobDate_ = jobOffers_[i].effectiveDate;
    if (pendingJobDate_ < currentDate_) pendingJobDate_ = currentDate_;
    AppendTranscript("ACCEPT JOB " + currentDate_.ToString() + " " + ClubRef(clubId).name + " effective=" +
                     pendingJobDate_.ToString());
    jobOffers_.erase(jobOffers_.begin() + i);
    return true;
  }
  if (error) *error = "job offer not found";
  return false;
}

void Career::ProcessPendingJob(const Date &date) {
  if (pendingJobClubId_ < 0 || date < pendingJobDate_) return;
  SwitchClub(pendingJobClubId_);
  pendingJobClubId_ = -1;
}

void Career::SwitchClub(int clubId) {
  int oldClubId = manager_.clubId;
  if (oldClubId >= 0) ClubRef(oldClubId).isManaged = false;
  for (size_t i = 0; i < academyPlayerIds_.size(); i++) {
    std::map<int, Player>::iterator it = players_.find(academyPlayerIds_[i]);
    if (it != players_.end() && it->second.isYouth && it->second.youthClubId == oldClubId) {
      players_.erase(it);
    }
  }
  Club &club = ClubRef(clubId);
  club.isManaged = true;
  manager_.clubId = clubId;
  club.boardTrust =
      static_cast<int>(clampValue(30 + (manager_.reputation - 50) / 2 + (club.reputation - 50) / 3, 10, 90));
  academyPlayerIds_.clear();
  academyReports_.clear();
  objectives_.clear();
  jobOffers_.clear();
  IssueObjectives();
  AppendTranscript("NEW CLUB " + currentDate_.ToString() + " " + club.name + " trust=" +
                   std::to_string(club.boardTrust));
  AppendNews("Manager took charge of " + club.name + ".");
}

Date Career::NextWindowStart(const Date &date) const {
  Date candidates[] = {Date(date.year, 1, 1), Date(date.year, 7, 1), Date(date.year + 1, 1, 1),
                       Date(date.year + 1, 7, 1)};
  for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
    if (candidates[i] >= date) return candidates[i];
  }
  return candidates[0];
}

Career::Candidate Career::FindNextEvent() const {
  Candidate best;
  best.valid = false;

  auto consider = [&best](const Date &date, int priority, CareerEventType type, int fixtureId, int competition) {
    if (!best.valid || date < best.date || (date == best.date && priority < best.priority)) {
      best.date = date;
      best.priority = priority;
      best.type = type;
      best.fixtureId = fixtureId;
      best.fixtureCompetition = competition;
      best.valid = true;
    }
  };

  const Season &season = CurrentSeason();

  for (size_t i = 0; i < season.fixtures.size(); i++) {
    const Fixture &fixture = season.fixtures[i];
    if (fixture.played) continue;
    if (fixture.date < currentDate_) continue;
    const Competition *competition = FindCompetition(fixture.competitionId);
    int type = competition ? competition->type : e_Competition_League;
    consider(fixture.date, 50, e_CareerEvent_Fixture, fixture.id, type);
  }

  auto salaryDate = [this](const Date &reference) {
    int year = reference.year;
    int month = reference.month;
    int day = config_.salaryDay;
    if (month == 2) day = std::min(day, 28);
    if (month == 4 || month == 6 || month == 9 || month == 11) day = std::min(day, 30);
    Date candidate(year, month, day);
    if (candidate <= reference) {
      month++;
      if (month > 12) {
        month = 1;
        year++;
      }
      day = config_.salaryDay;
      if (month == 2) day = std::min(day, 28);
      if (month == 4 || month == 6 || month == 9 || month == 11) day = std::min(day, 30);
      candidate = Date(year, month, day);
    }
    return candidate;
  };
  consider(salaryDate(currentDate_), 10, e_CareerEvent_Monthly, -1, e_Competition_League);

  const Date windowBoundaries[] = {
      Date(currentDate_.year, 1, 1), Date(currentDate_.year, 2, 1),
      Date(currentDate_.year, 7, 1), Date(currentDate_.year, 9, 1),
      Date(currentDate_.year + 1, 1, 1), Date(currentDate_.year + 1, 2, 1),
      Date(currentDate_.year + 1, 7, 1), Date(currentDate_.year + 1, 9, 1)};
  for (size_t i = 0; i < sizeof(windowBoundaries) / sizeof(windowBoundaries[0]); i++) {
    if (windowBoundaries[i] > currentDate_) {
      consider(windowBoundaries[i], 20,
               IsTransferWindowOpenAtDate(windowBoundaries[i]) ? e_CareerEvent_TransferWindowOpen
                                                               : e_CareerEvent_TransferWindowClose,
               -1, e_Competition_League);
    }
  }

  if (pendingJobClubId_ >= 0 && pendingJobDate_ >= currentDate_) {
    consider(pendingJobDate_, 25, e_CareerEvent_JobStart, -1, e_Competition_League);
  }

  for (size_t i = 0; i < scoutReports_.size(); i++) {
    if (!scoutReports_[i].ready && scoutReports_[i].dueDate >= currentDate_) {
      consider(scoutReports_[i].dueDate, 30, e_CareerEvent_ScoutReport, -1, e_Competition_League);
    }
  }

  for (size_t i = 0; i < scouts_.size(); i++) {
    const Scout &scout = scouts_[i];
    if (scout.youthScout && scout.nextReportDate >= currentDate_) {
      consider(scout.nextReportDate, 40, e_CareerEvent_YouthReport, -1, e_Competition_League);
    }
  }

  if (!season.finished) {
    bool allPlayed = true;
    for (size_t i = 0; i < season.fixtures.size(); i++) {
      if (!season.fixtures[i].played) {
        allPlayed = false;
        break;
      }
    }
    if (allPlayed) {
      Date end = season.endDate.AddDays(1);
      if (end < currentDate_) end = currentDate_;
      consider(end, 60, e_CareerEvent_SeasonEnd, -1, e_Competition_League);
    }
  }

  return best;
}

bool Career::IsTransferWindowOpenAtDate(const Date &date) const {
  if (date.month == 7 || date.month == 8) return true;
  if (date.month == 1) return true;
  return false;
}

std::vector<CareerEvent> Career::Advance() {
  CareerAction advance = CareerAction::Advance();
  if (!replaying_) journal_.push_back(advance);

  std::vector<CareerEvent> result;
  for (int guard = 0; guard < 10000; guard++) {
    Candidate candidate = FindNextEvent();
    if (!candidate.valid) break;

    int delta = candidate.date.Ordinal() - currentDate_.Ordinal();
    if (delta > 0) AdvanceDays(delta);
    currentDate_ = candidate.date;

    CareerEvent event;
    event.date = candidate.date;
    event.type = candidate.type;
    event.fixtureId = candidate.fixtureId;
    event.fixtureCompetition = candidate.fixtureCompetition;

    switch (candidate.type) {
      case e_CareerEvent_Fixture: {
        Fixture *fixture = FindFixture(candidate.fixtureId);
        if (!fixture || fixture->played) break;
        bool managed = IsManagedClub(fixture->homeClubId) || IsManagedClub(fixture->awayClubId);
        if (managed) {
          event.halt = true;
          event.text = "FIXTURE " + FixtureLine(*fixture);
          result.push_back(event);
          RecordEvent(event);
          return result;
        }
        ResolveFixture(fixture->id);
        fixture = FindFixture(candidate.fixtureId);
        event.halt = false;
        event.text = "AUTO " + (fixture ? FixtureLine(*fixture) : std::string("fixture"));
        result.push_back(event);
        RecordEvent(event);
        break;
      }
      case e_CareerEvent_Monthly: {
        ProcessMonthly(candidate.date);
        event.halt = true;
        event.text = "MONTHLY " + candidate.date.ToString() + " salaries processed";
        result.push_back(event);
        RecordEvent(event);
        return result;
      }
      case e_CareerEvent_TransferWindowOpen:
      case e_CareerEvent_TransferWindowClose: {
        ProcessWindowBoundary(candidate.date);
        event.halt = true;
        event.text = (windowOpen_ ? "TRANSFER WINDOW OPEN " : "TRANSFER WINDOW CLOSED ") + candidate.date.ToString();
        result.push_back(event);
        RecordEvent(event);
        return result;
      }
      case e_CareerEvent_ScoutReport: {
        ProcessScoutReports(candidate.date);
        event.halt = true;
        event.text = "SCOUT REPORT READY " + candidate.date.ToString();
        result.push_back(event);
        RecordEvent(event);
        return result;
      }
      case e_CareerEvent_YouthReport: {
        ProcessYouthReport(candidate.date);
        event.halt = true;
        event.text = "YOUTH REPORT READY " + candidate.date.ToString();
        result.push_back(event);
        RecordEvent(event);
        return result;
      }
      case e_CareerEvent_JobStart: {
        ProcessPendingJob(candidate.date);
        event.halt = true;
        event.text = "JOB START " + candidate.date.ToString();
        result.push_back(event);
        RecordEvent(event);
        return result;
      }
      case e_CareerEvent_SeasonEnd: {
        SeasonTransition();
        event.halt = true;
        event.text = "SEASON END " + candidate.date.ToString();
        result.push_back(event);
        RecordEvent(event);
        return result;
      }
    }
    if (event.halt) break;
  }
  return result;
}

void Career::ProcessMonthly(const Date &date) {
  PaySalaries(date);

  int bestPlayerId = -1;
  int bestScore = -1;
  for (std::map<int, Player>::const_iterator it = players_.begin(); it != players_.end(); ++it) {
    if (it->second.clubId < 0) continue;
    int score = it->second.goalsThisSeason * 3 + it->second.assistsThisSeason * 2 + it->second.OvrOn(date) / 10;
    if (score > bestScore || (score == bestScore && it->first < bestPlayerId)) {
      bestScore = score;
      bestPlayerId = it->first;
    }
  }
  if (bestPlayerId >= 0) {
    const Player &winner = PlayerRef(bestPlayerId);
    if (IsManagedClub(winner.clubId)) {
      Club &club = ClubRef(winner.clubId);
      club.subscribers += 2500;
      AppendTranscript("AWARD " + date.ToString() + " player of the month " + winner.name + " subscribers=" +
                       std::to_string(club.subscribers));
      AppendNews(winner.name + " won the player of the month award.");
    }
  }
}

void Career::ProcessWindowBoundary(const Date &date) {
  windowOpen_ = IsTransferWindowOpenAtDate(date);
  if (windowOpen_) {
    RunRivalTransfers(date);
    BuildJobOffers(manager_.clubId < 0);
  }
}

void Career::ProcessScoutReports(const Date &date) {
  for (size_t i = 0; i < scoutReports_.size(); i++) {
    ScoutReport &report = scoutReports_[i];
    if (report.ready || report.dueDate > date) continue;
    report.ready = true;
    const Player *player = FindPlayer(report.playerId);
    if (player) report.exactOvr = player->OvrOn(date);
    AppendTranscript("SCOUT " + date.ToString() + " report player=" + std::to_string(report.playerId) +
                     " ovr=" + std::to_string(report.exactOvr));
  }
}

void Career::ProcessYouthReport(const Date &date) {
  GenerateAcademyReport();
  for (size_t i = 0; i < scouts_.size(); i++) {
    if (scouts_[i].youthScout) scouts_[i].nextReportDate = date.AddDays(config_.youthReportIntervalDays);
  }
}

void Career::AdvanceDays(int days) {
  if (days <= 0) return;
  for (std::map<int, Player>::iterator it = players_.begin(); it != players_.end(); ++it) {
    Player &player = it->second;
    player.condition.fatigue = static_cast<int>(clampValue(player.condition.fatigue + days * config_.fatigueRecoveryPerDay,
                                                          0, 100));
    if (player.condition.injury.active) {
      player.condition.injury.daysRemaining -= days;
      if (player.condition.injury.daysRemaining <= 0) {
        player.condition.injury.active = false;
        player.condition.injury.daysRemaining = 0;
        player.condition.injury.critical = false;
        player.condition.injury.type = "none";
      }
    }
  }
}

void Career::AddIncome(long long amount, const std::string &reason) {
  if (amount <= 0 || manager_.clubId < 0) return;
  AddIncomeInternal(amount);
  AppendTranscript("INCOME " + currentDate_.ToString() + " amount=" + std::to_string(amount) + " " + reason +
                   " balance=" + std::to_string(ClubRef(manager_.clubId).balance));
}

void Career::AddExpense(long long amount, const std::string &reason) {
  if (amount <= 0 || manager_.clubId < 0) return;
  AddExpenseInternal(amount);
  AppendTranscript("EXPENSE " + currentDate_.ToString() + " amount=" + std::to_string(amount) + " " + reason +
                   " balance=" + std::to_string(ClubRef(manager_.clubId).balance));
}

void Career::AddIncomeInternal(long long amount) {
  if (amount <= 0) return;
  if (salaryArrears_ > 0) {
    long long repay = std::min(salaryArrears_, amount);
    salaryArrears_ -= repay;
    amount -= repay;
  }
  if (manager_.clubId >= 0) ClubRef(manager_.clubId).balance += amount;
}

void Career::AddExpenseInternal(long long amount) {
  if (amount <= 0) return;
  if (manager_.clubId >= 0) ClubRef(manager_.clubId).balance -= amount;
}

void Career::PaySalaries(const Date &date) {
  if (manager_.clubId < 0) return;
  Club &club = ClubRef(manager_.clubId);
  long long monthly = 0;
  for (std::map<int, Player>::const_iterator it = players_.begin(); it != players_.end(); ++it) {
    const Player &player = it->second;
    if (player.clubId != club.id) continue;
    monthly += player.contract.annualSalary / 12;
  }
  for (size_t i = 0; i < scouts_.size(); i++) monthly += scouts_[i].annualSalary / 12;

  if (club.balance >= monthly) {
    club.balance -= monthly;
  } else {
    long long unpaid = monthly - club.balance;
    club.balance = 0;
    salaryArrears_ += unpaid;
    club.boardTrust = std::max(0, club.boardTrust - 3);
    AppendNews("Salary arrears: " + std::to_string(unpaid) + " unpaid on " + date.ToString() + ".");
    AppendTranscript("ARREARS " + date.ToString() + " unpaid=" + std::to_string(unpaid) + " total=" +
                     std::to_string(salaryArrears_) + " trust=" + std::to_string(club.boardTrust));
  }

  for (std::map<int, Club>::iterator it = clubs_.begin(); it != clubs_.end(); ++it) {
    if (it->second.id == club.id) continue;
    long long rivalMonthly = 0;
    for (std::map<int, Player>::const_iterator p = players_.begin(); p != players_.end(); ++p) {
      if (p->second.clubId == it->second.id) rivalMonthly += p->second.contract.annualSalary / 12;
    }
    it->second.balance = std::max(0LL, it->second.balance - rivalMonthly);
  }
}

void Career::RunRivalTransfers(const Date &date) {
  int moves = rng_.Int(2, 5);
  std::vector<Club *> rivals;
  for (std::map<int, Club>::iterator it = clubs_.begin(); it != clubs_.end(); ++it) {
    if (!it->second.isManaged) rivals.push_back(&it->second);
  }
  if (rivals.size() < 2) return;

  for (int move = 0; move < moves; move++) {
    Club *buyer = rivals[rng_.Int(0, static_cast<int>(rivals.size()) - 1)];
    Club *seller = rivals[rng_.Int(0, static_cast<int>(rivals.size()) - 1)];
    if (buyer == seller) continue;
    std::vector<int> sellerPlayers = ClubPlayerIds(seller->id);
    if (sellerPlayers.empty()) continue;
    int playerId = sellerPlayers[rng_.Int(0, static_cast<int>(sellerPlayers.size()) - 1)];
    Player &player = PlayerRef(playerId);
    long long fee = std::max(10000LL, player.marketValue / 3LL);
    if (buyer->balance < fee || seller->balance + fee < 0) continue;
    buyer->balance -= fee;
    seller->balance += fee;
    player.clubId = buyer->id;
    player.contract.annualSalary = std::max(20000LL, fee / 6LL);
    player.contract.yearsRemaining = rng_.Int(1, 4);
    AppendTranscript("RIVAL " + date.ToString() + " " + player.name + " " + seller->shortName + " -> " +
                     buyer->shortName + " fee=" + std::to_string(fee));
  }
}

void Career::ResolveFixture(int fixtureId) {
  Fixture *fixture = FindFixture(fixtureId);
  if (!fixture || fixture->played) return;
  const Competition *competition = FindCompetition(fixture->competitionId);
  CompetitionType type = competition ? competition->type : e_Competition_League;

  if (fixture->bye || fixture->awayClubId < 0) {
    fixture->played = true;
    return;
  }

  bool homeOk = CanFieldTeam(fixture->homeClubId, type);
  bool awayOk = CanFieldTeam(fixture->awayClubId, type);
  if (!homeOk || !awayOk) {
    int losing = !homeOk ? fixture->homeClubId : fixture->awayClubId;
    MatchOutcome outcome = BuildWalkoverOutcome(*fixture, losing);
    ApplyOutcomeInternal(outcome, false);
    return;
  }

  QuickSimResult result = QuickSim::Play(fixture->id, type, MakeQuickSimTeam(fixture->homeClubId, type),
                                         MakeQuickSimTeam(fixture->awayClubId, type), config_, rng_);
  ApplyOutcomeInternal(result.outcome, false);
}

MatchOutcome Career::BuildWalkoverOutcome(const Fixture &fixture, int losingClubId) const {
  MatchOutcome outcome;
  outcome.fixtureId = fixture.id;
  outcome.homeClubId = fixture.homeClubId;
  outcome.awayClubId = fixture.awayClubId;
  outcome.walkover = true;
  if (losingClubId == fixture.homeClubId) {
    outcome.homeGoals = 0;
    outcome.awayGoals = 3;
  } else {
    outcome.homeGoals = 3;
    outcome.awayGoals = 0;
  }
  return outcome;
}

void Career::ResolveFixtureWithOutcome(int fixtureId, const MatchOutcome &outcome) {
  Fixture *fixture = FindFixture(fixtureId);
  if (!fixture || fixture->played) return;
  ApplyOutcomeInternal(outcome, false);
}

void Career::ApplyOutcome(const MatchOutcome &outcome) {
  const Fixture *fixture = FindFixture(outcome.fixtureId);
  if (!fixture || fixture->played) return;
  if (!replaying_ && (IsManagedClub(fixture->homeClubId) || IsManagedClub(fixture->awayClubId))) {
    bool alreadyRecorded = false;
    for (size_t i = 0; i < journal_.size(); i++) {
      if (journal_[i].type == e_Action_PlayNext && journal_[i].fixtureId == outcome.fixtureId) {
        alreadyRecorded = true;
        break;
      }
    }
    if (!alreadyRecorded) journal_.push_back(CareerAction::Play(outcome.fixtureId));
  }
  ApplyOutcomeInternal(outcome, true);
}

void Career::ApplyOutcomeInternal(const MatchOutcome &outcome, bool record) {
  if (record) recordedOutcomes_[outcome.fixtureId] = outcome;
  Fixture *fixture = FindFixture(outcome.fixtureId);
  if (!fixture) return;
  const Competition *competition = FindCompetition(fixture->competitionId);
  CompetitionType type = competition ? competition->type : e_Competition_League;

  fixture->homeGoals = outcome.homeGoals;
  fixture->awayGoals = outcome.awayGoals;
  fixture->homePenalties = outcome.homePenalties;
  fixture->awayPenalties = outcome.awayPenalties;
  fixture->played = true;

  int clubsOnPitch[2] = {fixture->homeClubId, fixture->awayClubId};
  for (int side = 0; side < 2; side++) {
    int clubId = clubsOnPitch[side];
    if (clubId < 0) continue;
    for (std::map<int, Player>::iterator it = players_.begin(); it != players_.end(); ++it) {
      Player &player = it->second;
      if (player.clubId != clubId) continue;
      if (type == e_Competition_Cup) {
        if (player.condition.cupBan > 0) player.condition.cupBan--;
      } else {
        if (player.condition.leagueBan > 0) player.condition.leagueBan--;
      }
    }
  }

  for (size_t i = 0; i < outcome.lines.size(); i++) {
    const MatchLine &line = outcome.lines[i];
    std::map<int, Player>::iterator it = players_.find(line.playerId);
    if (it == players_.end()) continue;
    Player &player = it->second;
    player.condition.fatigue = static_cast<int>(clampValue(line.endFatigue, 0, 100));
    if (line.injured) {
      player.condition.injury.active = true;
      player.condition.injury.critical = line.critical;
      player.condition.injury.daysRemaining = std::max(1, line.injuryDays);
      player.condition.injury.type = line.injuryType.empty() ? "knock" : line.injuryType;
      AppendNews(player.name + " injured (" + player.condition.injury.type + ", " +
                 std::to_string(player.condition.injury.daysRemaining) + " days).");
    }
    player.minutesThisSeason += line.minutes;
    player.goalsThisSeason += line.goals;
    player.assistsThisSeason += line.assists;
    if (line.redCards > 0) {
      if (type == e_Competition_Cup) player.condition.cupBan = std::max(player.condition.cupBan, 1);
      else player.condition.leagueBan = std::max(player.condition.leagueBan, 1);
    }
  }

  if (IsManagedClub(fixture->homeClubId)) {
    Club &club = ClubRef(fixture->homeClubId);
    long long matchday = (long long)club.subscribers * (20 + club.fanSatisfaction) / 100;
    AddIncomeInternal(matchday);
  }

  if (fixture->homeGoals > fixture->awayGoals) {
    if (IsManagedClub(fixture->homeClubId)) ClubRef(fixture->homeClubId).fanSatisfaction = std::min(100, ClubRef(fixture->homeClubId).fanSatisfaction + 1);
    if (IsManagedClub(fixture->awayClubId)) ClubRef(fixture->awayClubId).fanSatisfaction = std::max(0, ClubRef(fixture->awayClubId).fanSatisfaction - 1);
  } else if (fixture->awayGoals > fixture->homeGoals) {
    if (IsManagedClub(fixture->awayClubId)) ClubRef(fixture->awayClubId).fanSatisfaction = std::min(100, ClubRef(fixture->awayClubId).fanSatisfaction + 1);
    if (IsManagedClub(fixture->homeClubId)) ClubRef(fixture->homeClubId).fanSatisfaction = std::max(0, ClubRef(fixture->homeClubId).fanSatisfaction - 1);
  }

  if (IsManagedClub(fixture->homeClubId) || IsManagedClub(fixture->awayClubId)) {
    AppendTranscript("RESULT " + FixtureLine(*fixture));
  }

  if (type == e_Competition_Cup) {
    Competition *cup = FindCompetition(fixture->competitionId);
    if (cup) MaybeCreateNextCupRound(*cup);
  }

  if (IsManagedClub(fixture->homeClubId) || IsManagedClub(fixture->awayClubId)) {
    CheckSportingObjectives();
  }

  const Club *managed = manager_.clubId >= 0 ? FindClub(manager_.clubId) : nullptr;
  if (managed && managed->boardTrust < config_.trustDismissThreshold) {
    DismissManager("board trust below threshold");
  }
}

std::string Career::FixtureLine(const Fixture &fixture) const {
  const Club *home = FindClub(fixture.homeClubId);
  const Club *away = FindClub(fixture.awayClubId);
  std::string homeName = home ? home->shortName : "BYE";
  std::string awayName = away ? away->shortName : "BYE";
  std::ostringstream text;
  text << fixture.date.ToString() << " " << homeName << " ";
  if (fixture.awayClubId < 0 || fixture.bye) {
    text << "bye";
  } else if (!fixture.played) {
    text << "vs " << awayName;
  } else {
    text << fixture.homeGoals << ":" << fixture.awayGoals << " " << awayName;
    if (fixture.homePenalties >= 0 && fixture.awayPenalties >= 0) {
      text << " pens " << fixture.homePenalties << ":" << fixture.awayPenalties;
    }
  }
  return text.str();
}

void Career::RecordEvent(const CareerEvent &event) {
  AppendTranscript(event.text);
}

void Career::AppendTranscript(const std::string &line) {
  transcript_.push_back(line);
}

void Career::AppendNews(const std::string &text) {
  news_.push_back(currentDate_.ToString() + " " + text);
}

std::string Career::Transcript() const {
  std::ostringstream text;
  for (size_t i = 0; i < transcript_.size(); i++) text << transcript_[i] << "\n";
  return text.str();
}

std::string Career::Hash() const {
  std::string raw = Transcript();
  boost::uuids::detail::sha1 sha;
  sha.process_bytes(raw.data(), raw.size());
  boost::uuids::detail::sha1::digest_type digest;
  sha.get_digest(digest);
  std::ostringstream text;
  for (int i = 0; i < 20; i++) {
    text << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(digest[i]);
  }
  return text.str();
}

} // namespace career
} // namespace blunted
