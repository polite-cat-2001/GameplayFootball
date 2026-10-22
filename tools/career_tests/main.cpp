// Career v1 invariants from issue #54 Testing Decisions. Any failure prints
// "INVARIANT FAILED: ..." and the executable exits non-zero (ctest).

#include "career/career.hpp"
#include "career/career_data_source.hpp"
#include "career/quick_sim.hpp"

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

using namespace blunted;
using namespace blunted::career;

namespace blunted {
namespace career {

struct CareerTestProbe {
  static Club &ClubAt(Career &career, int clubId) { return career.ClubRef(clubId); }
  static Player &PlayerAt(Career &career, int playerId) { return career.PlayerRef(playerId); }
  static void SetBalance(Career &career, long long balance) { career.ClubRef(career.manager_.clubId).balance = balance; }
  static void SetBoardTrust(Career &career, int trust) { career.ClubRef(career.manager_.clubId).boardTrust = trust; }
  static void SetWindowOpen(Career &career, bool open) { career.windowOpen_ = open; }
  static void SetCurrentDate(Career &career, const Date &date) { career.currentDate_ = date; }
  static void SetArrears(Career &career, long long arrears) { career.salaryArrears_ = arrears; }
  static void CheckDismissal(Career &career) { career.CheckDismissal(); }
  static void ProcessScoutReports(Career &career, const Date &date) { career.ProcessScoutReports(date); }
  static void GenerateAcademyReport(Career &career) { career.GenerateAcademyReport(); }
  static void DevelopPlayers(Career &career) { career.DevelopPlayers(); }
  static void PaySalaries(Career &career, const Date &date) { career.PaySalaries(date); }
  static void SeasonTransition(Career &career) { career.SeasonTransition(); }
  static int AcademyPromotions(Career &career) { return career.academyPromotionsThisSeason_; }
  static const std::vector<int> &AcademyPlayers(Career &career) { return career.academyPlayerIds_; }
  static void SetPlayerInjury(Career &career, int playerId, bool active, int days) {
    career.PlayerRef(playerId).condition.injury.active = active;
    career.PlayerRef(playerId).condition.injury.daysRemaining = days;
  }
  static void SetPlayerBirthDate(Career &career, int playerId, const Date &date) {
    career.PlayerRef(playerId).birthDate = date;
  }
};

} // namespace career
} // namespace blunted

namespace {

int failures = 0;

void Check(bool condition, const std::string &message) {
  if (!condition) {
    std::cout << "INVARIANT FAILED: " << message << "\n";
    failures++;
  }
}

Career MakeCareer(uint32_t seed, CareerConfig config = CareerConfig()) {
  Date reference(config.seasonStartYear, config.seasonStartMonth, config.seasonStartDay);
  SyntheticCareerDataSource source(seed, config.clubsPerDivision, reference, config.freeAgentCount);
  return Career(seed, config, source);
}

void ResolveAllFixtures(Career &career) {
  std::vector<int> ids;
  const Season &season = career.CurrentSeason();
  for (size_t i = 0; i < season.fixtures.size(); i++) {
    if (!season.fixtures[i].played) ids.push_back(season.fixtures[i].id);
  }
  for (size_t i = 0; i < ids.size(); i++) {
    std::string error;
    career.ApplyAction(CareerAction::Sim(ids[i]), &error);
  }
}

void FinishSeason(Career &career) {
  int start = career.CurrentSeasonIndex();
  for (int guard = 0; guard < 20000; guard++) {
    std::vector<CareerEvent> events = career.Advance();
    if (career.CurrentSeasonIndex() != start) break;
    if (events.empty()) break;
    const CareerEvent &last = events.back();
    if (last.type == e_CareerEvent_Fixture && last.halt) {
      std::string error;
      career.ApplyAction(CareerAction::Sim(last.fixtureId), &error);
    }
  }
}

void TestScheduleAndSkeleton() {
  Career career = MakeCareer(101);
  const Season &season = career.CurrentSeason();
  Check(!season.fixtures.empty(), "schedule is not empty");

  for (size_t c = 0; c < season.competitions.size(); c++) {
    const Competition &competition = season.competitions[c];
    if (competition.type != e_Competition_League) continue;
    std::map<std::pair<int, int>, std::pair<int, int> > meetings;
    std::map<int, int> games;
    for (size_t i = 0; i < season.fixtures.size(); i++) {
      const Fixture &fixture = season.fixtures[i];
      if (fixture.competitionId != competition.id || fixture.bye) continue;
      meetings[std::make_pair(fixture.homeClubId, fixture.awayClubId)].first++;
      meetings[std::make_pair(fixture.awayClubId, fixture.homeClubId)].second++;
      games[fixture.homeClubId]++;
      games[fixture.awayClubId]++;
    }
    for (size_t a = 0; a < competition.clubIds.size(); a++) {
      for (size_t b = a + 1; b < competition.clubIds.size(); b++) {
        int homeMeetings = meetings[std::make_pair(competition.clubIds[a], competition.clubIds[b])].first;
        int awayMeetings = meetings[std::make_pair(competition.clubIds[a], competition.clubIds[b])].second;
        Check(homeMeetings == 1 && awayMeetings == 1, "each pairing is played once per venue");
      }
    }
    int expectedGames = 2 * (static_cast<int>(competition.clubIds.size()) - 1);
    for (std::map<int, int>::const_iterator it = games.begin(); it != games.end(); ++it) {
      Check(it->second == expectedGames, "each club plays every other club home and away");
    }
  }

  int weekends = 0, midweeks = 0, breaks = 0;
  for (size_t i = 0; i < season.slots.size(); i++) {
    if (season.slots[i].type == e_Slot_Weekend) weekends++;
    if (season.slots[i].type == e_Slot_Midweek) midweeks++;
    if (season.slots[i].type == e_Slot_Break) breaks++;
  }
  Check(weekends > 0 && midweeks > 0 && breaks > 0, "slot skeleton has weekend, midweek and break slots");
  for (size_t i = 0; i < season.fixtures.size(); i++) {
    const Fixture &fixture = season.fixtures[i];
    const Competition *competition = nullptr;
    for (size_t c = 0; c < season.competitions.size(); c++) {
      if (season.competitions[c].id == fixture.competitionId) competition = &season.competitions[c];
    }
    if (!competition) continue;
    bool onWeekend = false;
    bool onMidweek = false;
    for (size_t s = 0; s < season.slots.size(); s++) {
      if (season.slots[s].date == fixture.date) {
        onWeekend = season.slots[s].type == e_Slot_Weekend;
        onMidweek = season.slots[s].type == e_Slot_Midweek;
      }
    }
    if (competition->type == e_Competition_League) Check(onWeekend, "league fixtures sit on weekend slots");
    else Check(onMidweek, "cup fixtures sit on midweek slots");
  }
}

void TestPromotionRelegation() {
  Career career = MakeCareer(202);
  ResolveAllFixtures(career);
  std::vector<StandingRow> division1 = career.LeagueTable(1);
  std::vector<StandingRow> division2 = career.LeagueTable(2);
  Check(division1.size() >= 2 && division2.size() >= 2, "both divisions have tables");
  std::vector<int> relegated;
  std::vector<int> promoted;
  for (int i = 0; i < 2; i++) relegated.push_back(division1[division1.size() - 1 - i].clubId);
  for (int i = 0; i < 2; i++) promoted.push_back(division2[i].clubId);

  FinishSeason(career);
  for (size_t i = 0; i < relegated.size(); i++) {
    std::map<int, Club>::const_iterator it = career.Clubs().find(relegated[i]);
    Check(it != career.Clubs().end() && it->second.division == 2, "bottom two of division 1 are relegated");
  }
  for (size_t i = 0; i < promoted.size(); i++) {
    std::map<int, Club>::const_iterator it = career.Clubs().find(promoted[i]);
    Check(it != career.Clubs().end() && it->second.division == 1, "top two of division 2 are promoted");
  }
}

void TestReplay() {
  Career career = MakeCareer(303);
  career.ApplyAction(CareerAction::HireScout(15, 70, 25000));
  career.ApplyAction(CareerAction::HireYouthScout(60, 20000, "Europe"));
  career.ApplyAction(CareerAction::ChooseSponsor(0));
  ResolveAllFixtures(career);
  FinishSeason(career);

  CareerConfig config;
  Date reference(config.seasonStartYear, config.seasonStartMonth, config.seasonStartDay);
  SyntheticCareerDataSource source(303, config.clubsPerDivision, reference, config.freeAgentCount);
  Career replayed = Career::Replay(303, config, source, career.Journal(), career.RecordedOutcomes());
  Check(replayed.Hash() == career.Hash(), "same seed + journal reproduces the same transcript");
  Check(replayed.Transcript() == career.Transcript(), "same seed + journal reproduces the same career");
}

void TestPlayedOutcomeReplay() {
  Career career = MakeCareer(313);
  const Season &season = career.CurrentSeason();
  const Fixture *target = nullptr;
  for (size_t i = 0; i < season.fixtures.size(); i++) {
    const Fixture &fixture = season.fixtures[i];
    if (fixture.played) continue;
    if (!career.IsManagedClub(fixture.homeClubId) && !career.IsManagedClub(fixture.awayClubId)) continue;
    target = &fixture;
    break;
  }
  Check(target != nullptr, "there is a managed fixture for a played outcome");
  if (target) {
    MatchOutcome outcome;
    outcome.fixtureId = target->id;
    outcome.homeClubId = target->homeClubId;
    outcome.awayClubId = target->awayClubId;
    outcome.homeGoals = 2;
    outcome.awayGoals = 1;
    career.ApplyOutcome(outcome);
  }
  ResolveAllFixtures(career);
  FinishSeason(career);

  CareerConfig config;
  Date reference(config.seasonStartYear, config.seasonStartMonth, config.seasonStartDay);
  SyntheticCareerDataSource source(313, config.clubsPerDivision, reference, config.freeAgentCount);
  Career replayed = Career::Replay(313, config, source, career.Journal(), career.RecordedOutcomes());
  Check(replayed.Hash() == career.Hash(), "played 3D outcome snapshot is injected on replay");
}

void RunToFirstWindow(Career &career) {
  for (int guard = 0; guard < 20000; guard++) {
    std::vector<CareerEvent> events = career.Advance();
    if (events.empty()) break;
    const CareerEvent &last = events.back();
    if (last.type == e_CareerEvent_TransferWindowOpen) break;
    if (last.type == e_CareerEvent_Fixture && last.halt) {
      std::string error;
      career.ApplyAction(CareerAction::Sim(last.fixtureId), &error);
    }
  }
}

void TestRivalTransfers() {
  Career first = MakeCareer(1111);
  Career second = MakeCareer(1111);
  RunToFirstWindow(first);
  RunToFirstWindow(second);
  Check(first.IsTransferWindowOpen(), "the first transfer window opens");
  Check(first.Transcript().find("RIVAL ") != std::string::npos, "rivals conduct autonomous transfers");
  bool same = true;
  for (std::map<int, Player>::const_iterator it = first.Players().begin(); it != first.Players().end(); ++it) {
    std::map<int, Player>::const_iterator other = second.Players().find(it->first);
    if (other == second.Players().end() || other->second.clubId != it->second.clubId) {
      same = false;
      break;
    }
  }
  Check(same, "rival transfers are deterministic from the seed");
  Check(first.Hash() == second.Hash(), "rival transfer transcript is deterministic");
}

void TestConditionContract() {
  Career career = MakeCareer(404);
  int playerId = career.Players().begin()->first;
  CareerTestProbe::SetPlayerInjury(career, playerId, true, 5);
  Check(!IsAvailable(CareerTestProbe::PlayerAt(career, playerId).condition, e_Competition_League),
        "injured player is unavailable");
  career.AdvanceDays(3);
  Check(!IsAvailable(CareerTestProbe::PlayerAt(career, playerId).condition, e_Competition_League),
        "injury has not healed before recoveryDays");
  career.AdvanceDays(3);
  Check(IsAvailable(CareerTestProbe::PlayerAt(career, playerId).condition, e_Competition_League),
        "injury heals after recoveryDays");

  int managed = career.ManagerClubId();
  int fixtureId = -1;
  const Season &season = career.CurrentSeason();
  for (size_t i = 0; i < season.fixtures.size(); i++) {
    const Fixture &fixture = season.fixtures[i];
    if (fixture.homeClubId == managed || fixture.awayClubId == managed) {
      fixtureId = fixture.id;
      break;
    }
  }
  Check(fixtureId >= 0, "managed club has a fixture");
  MatchOutcome outcome;
  outcome.fixtureId = fixtureId;
  const Fixture *fixture = nullptr;
  for (size_t i = 0; i < season.fixtures.size(); i++) {
    if (season.fixtures[i].id == fixtureId) fixture = &season.fixtures[i];
  }
  outcome.homeClubId = fixture->homeClubId;
  outcome.awayClubId = fixture->awayClubId;
  outcome.homeGoals = 0;
  outcome.awayGoals = 0;
  MatchLine line;
  line.playerId = playerId;
  line.clubId = managed;
  line.minutes = 90;
  line.endFatigue = 17;
  line.redCards = 1;
  outcome.lines.push_back(line);
  career.ApplyOutcome(outcome);
  Check(CareerTestProbe::PlayerAt(career, playerId).condition.fatigue == 17,
        "endFatigue is written into the condition contract");
  Check(CareerTestProbe::PlayerAt(career, playerId).condition.leagueBan == 1,
        "straight red gives a one-match league ban");
  Check(CareerTestProbe::PlayerAt(career, playerId).condition.cupBan == 0,
        "league and cup suspensions are independent");
}

void TestTransfersAndFinance() {
  Career career = MakeCareer(505);
  CareerTestProbe::SetWindowOpen(career, true);
  int managed = career.ManagerClubId();
  long long balance = CareerTestProbe::ClubAt(career, managed).balance;

  int targetId = -1;
  for (std::map<int, Player>::const_iterator it = career.Players().begin(); it != career.Players().end(); ++it) {
    if (it->second.clubId >= 0 && it->second.clubId != managed && !it->second.isYouth) {
      targetId = it->first;
      break;
    }
  }
  Check(targetId >= 0, "there is a transfer target");

  std::string error;
  Check(!career.ApplyAction(CareerAction::Transfer(targetId, balance + 1, 100000, 2), &error),
        "transfer above the current balance is rejected");
  Check(CareerTestProbe::ClubAt(career, managed).balance == balance, "rejected transfer does not change the balance");
  Check(CareerTestProbe::PlayerAt(career, targetId).clubId != managed, "rejected transfer does not move the player");

  long long fee = std::min(balance, 500000LL);
  Check(career.ApplyAction(CareerAction::Transfer(targetId, fee, 100000, 2), &error),
        "transfer within the balance succeeds");
  Check(CareerTestProbe::PlayerAt(career, targetId).clubId == managed, "accepted transfer moves the player");
  Check(CareerTestProbe::ClubAt(career, managed).balance == balance - fee, "fee is deducted from the balance");

  long long afterBuy = CareerTestProbe::ClubAt(career, managed).balance;
  Check(career.ApplyAction(CareerAction::Sell(targetId, 100000), &error), "sale succeeds");
  Check(CareerTestProbe::ClubAt(career, managed).balance == afterBuy + 100000, "sale adds to the balance");

  // Salary arrears: force the balance to zero, then pay the monthly salaries.
  CareerTestProbe::SetBalance(career, 0);
  int trustBefore = CareerTestProbe::ClubAt(career, managed).boardTrust;
  Date salaryDate(career.CurrentDate().year, career.CurrentDate().month, 30);
  if (salaryDate <= career.CurrentDate()) salaryDate = salaryDate.AddDays(30);
  CareerTestProbe::PaySalaries(career, salaryDate);
  Check(career.SalaryArrears() > 0, "unpaid salaries become salary arrears");
  Check(CareerTestProbe::ClubAt(career, managed).boardTrust < trustBefore, "salary arrears lower board trust");
  long long arrearsBefore = career.SalaryArrears();
  career.AddIncome(10000000, "test income");
  Check(career.SalaryArrears() == 0 && arrearsBefore > 0, "next income repays salary arrears first");
}

void TestScoutingAndAcademy() {
  Career career = MakeCareer(606);
  std::string error;
  Check(career.ApplyAction(CareerAction::HireScout(12, 80, 30000), &error), "scout can be hired");
  Check(career.ApplyAction(CareerAction::HireYouthScout(75, 20000, "Europe"), &error),
        "youth scout can be hired");
  Check(!career.Scouts().empty(), "scout list is populated");

  int targetId = -1;
  for (std::map<int, Player>::const_iterator it = career.Players().begin(); it != career.Players().end(); ++it) {
    if (it->second.clubId < 0 && !it->second.isYouth) {
      targetId = it->first;
      break;
    }
  }
  Check(targetId >= 0, "there is a scouting target");
  Check(career.ApplyAction(CareerAction::Scout(targetId, career.Scouts()[0].id), &error), "scout assignment succeeds");
  Check(!career.ScoutReports().empty(), "scout report is scheduled");
  const ScoutReport &report = career.ScoutReports().back();
  CareerTestProbe::ProcessScoutReports(career, report.dueDate);
  bool ready = false;
  int exact = 0;
  for (size_t i = 0; i < career.ScoutReports().size(); i++) {
    if (career.ScoutReports()[i].playerId == targetId) {
      ready = career.ScoutReports()[i].ready;
      exact = career.ScoutReports()[i].exactOvr;
    }
  }
  Check(ready, "scout report is ready after N career days");
  Check(exact == CareerTestProbe::PlayerAt(career, targetId).OvrOn(career.CurrentDate()),
        "scout report contains the exact rating");

  CareerTestProbe::GenerateAcademyReport(career);
  Check(!career.AcademyReports().empty(), "youth scout produces an academy report");
  const AcademyReport &academyReport = career.AcademyReports().back();
  Check(!academyReport.playerIds.empty(), "academy report lists juniors");
  int youthId = academyReport.playerIds[0];
  Check(career.ApplyAction(CareerAction::SignYouth(academyReport.id, youthId), &error),
        "junior can be signed to the academy");
  Check(CareerTestProbe::PlayerAt(career, youthId).isYouth, "signed junior is an academy player");
  Check(!CareerTestProbe::AcademyPlayers(career).empty(), "academy player list is populated");

  real baseBefore = CareerTestProbe::PlayerAt(career, youthId).baseStat;
  CareerTestProbe::DevelopPlayers(career);
  Check(CareerTestProbe::PlayerAt(career, youthId).baseStat >= baseBefore, "academy player grows toward potential");

  CareerTestProbe::SetPlayerBirthDate(career, youthId,
                                      Date(career.CurrentDate().year - 16, career.CurrentDate().month,
                                           career.CurrentDate().day));
  Check(career.ApplyAction(CareerAction::PromoteYouth(youthId, 40000, 3), &error),
        "16-year-old academy player can be promoted");
  Check(!CareerTestProbe::PlayerAt(career, youthId).isYouth, "promoted player leaves the academy");
  Check(CareerTestProbe::AcademyPromotions(career) == 1, "academy promotion is counted for objectives");
}

void TestObjectivesAndDismissal() {
  Career career = MakeCareer(707);
  Check(career.Objectives().size() == 12, "12 objectives are issued each season");
  int categories[4] = {0, 0, 0, 0};
  int urgencies[3] = {0, 0, 0};
  for (size_t i = 0; i < career.Objectives().size(); i++) {
    categories[career.Objectives()[i].category]++;
    urgencies[career.Objectives()[i].urgency]++;
  }
  for (int i = 0; i < 4; i++) Check(categories[i] == 3, "each objective category has three urgencies");
  for (int i = 0; i < 3; i++) Check(urgencies[i] == 4, "each urgency has four categories");

  int managed = career.ManagerClubId();
  Check(managed >= 0, "manager starts employed");
  CareerTestProbe::SetBoardTrust(career, 1);
  CareerTestProbe::CheckDismissal(career);
  Check(career.ManagerClubId() < 0, "trust below the threshold dismisses the manager");
  Check(!career.JobOffers().empty(), "dismissal produces a vacancy list");
  for (size_t i = 0; i < career.JobOffers().size(); i++) {
    Check(career.JobOffers()[i].reputationRequired <= career.GetManager().reputation + 15,
          "vacancies are gated by manager reputation");
  }
}

void TestCupPenaltiesAndWalkover() {
  QuickSimTeam home;
  QuickSimTeam away;
  home.clubId = 1;
  away.clubId = 2;
  for (int i = 0; i < 11; i++) {
    QuickSimPlayer player;
    player.id = i + 1;
    player.position = i == 0 ? e_Position_GK : e_Position_CM;
    player.ovr = 60;
    player.fatigue = 100;
    home.startingXi.push_back(player);
    player.id = i + 100;
    away.startingXi.push_back(player);
  }
  CareerRng rngA(9);
  CareerRng rngB(9);
  int homeA = 0, awayA = 0, homeB = 0, awayB = 0;
  QuickSim::Shootout(home, away, rngA, homeA, awayA);
  QuickSim::Shootout(home, away, rngB, homeB, awayB);
  Check(homeA == homeB && awayA == awayB, "penalty shootout is deterministic from the seed");
  Check(homeA != awayA, "penalty shootout always produces a winner");

  Career career = MakeCareer(808);
  int managed = career.ManagerClubId();
  for (std::map<int, Player>::const_iterator it = career.Players().begin(); it != career.Players().end(); ++it) {
    if (it->second.clubId == managed && !it->second.isYouth) {
      CareerTestProbe::SetPlayerInjury(career, it->first, true, 10);
    }
  }
  Check(!career.CanFieldTeam(managed, e_Competition_League), "team with no available players cannot field");
  int fixtureId = -1;
  bool managedIsHome = false;
  const Season &season = career.CurrentSeason();
  for (size_t i = 0; i < season.fixtures.size(); i++) {
    const Fixture &fixture = season.fixtures[i];
    const Competition *competition = nullptr;
    for (size_t c = 0; c < season.competitions.size(); c++) {
      if (season.competitions[c].id == fixture.competitionId) competition = &season.competitions[c];
    }
    if (!competition || competition->type != e_Competition_League) continue;
    if (fixture.homeClubId == managed) {
      fixtureId = fixture.id;
      managedIsHome = true;
      break;
    }
    if (fixture.awayClubId == managed) {
      fixtureId = fixture.id;
      managedIsHome = false;
      break;
    }
  }
  Check(fixtureId >= 0, "managed club has a league fixture");
  std::string error;
  career.ApplyAction(CareerAction::Sim(fixtureId), &error);
  const Fixture *played = nullptr;
  for (size_t i = 0; i < career.CurrentSeason().fixtures.size(); i++) {
    if (career.CurrentSeason().fixtures[i].id == fixtureId) played = &career.CurrentSeason().fixtures[i];
  }
  Check(played && played->played, "walkover fixture is played");
  if (played) {
    if (managedIsHome) Check(played->homeGoals == 0 && played->awayGoals == 3, "walkover is 0:3 against the unfielded home side");
    else Check(played->homeGoals == 3 && played->awayGoals == 0, "walkover is 0:3 against the unfielded away side");
  }
}

void TestSaveLoad() {
  Career career = MakeCareer(909);
  career.ApplyAction(CareerAction::ChooseSponsor(2));
  ResolveAllFixtures(career);
  FinishSeason(career);
  std::string path = "career_test_save.json";
  std::string error;
  Check(career.SaveJson(path, &error), "career saves to JSON");
  CareerConfig config;
  Date reference(config.seasonStartYear, config.seasonStartMonth, config.seasonStartDay);
  SyntheticCareerDataSource source(909, config.clubsPerDivision, reference, config.freeAgentCount);
  Career loaded = MakeCareer(909);
  Check(Career::LoadJson(path, source, loaded, &error), "career loads from JSON");
  Check(loaded.Hash() == career.Hash(), "loaded career reproduces the same transcript");
  Check(loaded.CurrentDate() == career.CurrentDate(), "loaded career keeps the current date");
  Check(loaded.ManagerClubId() == career.ManagerClubId(), "loaded career keeps the manager club");
  Check(loaded.Clubs().size() == career.Clubs().size(), "loaded career keeps the club world");
  std::remove(path.c_str());
}

} // namespace

int main() {
  TestScheduleAndSkeleton();
  TestPromotionRelegation();
  TestReplay();
  TestPlayedOutcomeReplay();
  TestRivalTransfers();
  TestConditionContract();
  TestTransfersAndFinance();
  TestScoutingAndAcademy();
  TestObjectivesAndDismissal();
  TestCupPenaltiesAndWalkover();
  TestSaveLoad();

  if (failures != 0) {
    std::cout << failures << " invariant(s) failed\n";
    return 1;
  }
  std::cout << "ALL CAREER INVARIANTS PASSED\n";
  return 0;
}

