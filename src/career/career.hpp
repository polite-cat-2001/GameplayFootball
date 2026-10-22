// Manager career v1 headless aggregate. Everything the career knows enters
// through CareerDataSource once; time is event-driven (Career::currentDate),
// never the global time_ms. Player decisions are recorded in a command journal
// so replaying seed + journal (plus recorded played-match outcomes) rebuilds the
// same career.

#ifndef _HPP_CAREER
#define _HPP_CAREER

#include <map>
#include <set>
#include <string>
#include <vector>

#include "career_data_source.hpp"
#include "career_rng.hpp"
#include "career_types.hpp"
#include "quick_sim.hpp"

namespace blunted {
namespace career {

struct CareerTestProbe;

enum CareerEventType {
  e_CareerEvent_Fixture,
  e_CareerEvent_Monthly,
  e_CareerEvent_TransferWindowOpen,
  e_CareerEvent_TransferWindowClose,
  e_CareerEvent_ScoutReport,
  e_CareerEvent_YouthReport,
  e_CareerEvent_SeasonEnd,
  e_CareerEvent_JobStart
};

struct CareerEvent {
  Date date;
  CareerEventType type = e_CareerEvent_Monthly;
  bool halt = false;
  std::string text;
  int fixtureId = -1;
  int fixtureCompetition = e_Competition_League;
};

enum CareerActionType {
  e_Action_Advance,
  e_Action_SimNext,
  e_Action_PlayNext,
  e_Action_Transfer,
  e_Action_Sell,
  e_Action_Renew,
  e_Action_ScoutPlayer,
  e_Action_HireScout,
  e_Action_FireScout,
  e_Action_HireYouthScout,
  e_Action_SignYouth,
  e_Action_PromoteYouth,
  e_Action_ChooseSponsor,
  e_Action_AcceptJob,
  e_Action_DeclineJob
};

struct CareerAction {
  CareerActionType type = e_Action_Advance;
  int fixtureId = -1;
  int playerId = -1;
  int clubId = -1;
  int scoutId = -1;
  int intValue = 0;
  long long amount = 0;
  int years = 1;
  std::string text;

  static CareerAction Advance() {
    CareerAction action;
    action.type = e_Action_Advance;
    return action;
  }
  static CareerAction Sim(int fixtureId) {
    CareerAction action;
    action.type = e_Action_SimNext;
    action.fixtureId = fixtureId;
    return action;
  }
  static CareerAction Play(int fixtureId) {
    CareerAction action;
    action.type = e_Action_PlayNext;
    action.fixtureId = fixtureId;
    return action;
  }
  static CareerAction Transfer(int playerId, long long fee, long long salary, int years) {
    CareerAction action;
    action.type = e_Action_Transfer;
    action.playerId = playerId;
    action.amount = fee;
    action.intValue = static_cast<int>(salary);
    action.years = years;
    return action;
  }
  static CareerAction Sell(int playerId, long long fee) {
    CareerAction action;
    action.type = e_Action_Sell;
    action.playerId = playerId;
    action.amount = fee;
    return action;
  }
  static CareerAction Renew(int playerId, long long salary, int years) {
    CareerAction action;
    action.type = e_Action_Renew;
    action.playerId = playerId;
    action.intValue = static_cast<int>(salary);
    action.years = years;
    return action;
  }
  static CareerAction Scout(int playerId, int scoutId) {
    CareerAction action;
    action.type = e_Action_ScoutPlayer;
    action.playerId = playerId;
    action.scoutId = scoutId;
    return action;
  }
  static CareerAction HireScout(int speedDays, int quality, long long salary) {
    CareerAction action;
    action.type = e_Action_HireScout;
    action.intValue = quality;
    action.years = speedDays;
    action.amount = salary;
    return action;
  }
  static CareerAction FireScout(int scoutId) {
    CareerAction action;
    action.type = e_Action_FireScout;
    action.scoutId = scoutId;
    return action;
  }
  static CareerAction HireYouthScout(int quality, long long salary, const std::string &region) {
    CareerAction action;
    action.type = e_Action_HireYouthScout;
    action.intValue = quality;
    action.amount = salary;
    action.text = region;
    return action;
  }
  static CareerAction SignYouth(int reportId, int playerId) {
    CareerAction action;
    action.type = e_Action_SignYouth;
    action.clubId = reportId;
    action.playerId = playerId;
    return action;
  }
  static CareerAction PromoteYouth(int playerId, long long salary, int years) {
    CareerAction action;
    action.type = e_Action_PromoteYouth;
    action.playerId = playerId;
    action.amount = salary;
    action.years = years;
    return action;
  }
  static CareerAction ChooseSponsor(int index) {
    CareerAction action;
    action.type = e_Action_ChooseSponsor;
    action.intValue = index;
    return action;
  }
  static CareerAction AcceptJob(int clubId) {
    CareerAction action;
    action.type = e_Action_AcceptJob;
    action.clubId = clubId;
    return action;
  }
  static CareerAction DeclineJob(int clubId) {
    CareerAction action;
    action.type = e_Action_DeclineJob;
    action.clubId = clubId;
    return action;
  }
};

class Career {
  public:
    Career(uint32_t seed, const CareerConfig &config, CareerDataSource &dataSource);

    uint32_t Seed() const { return seed_; }
    const CareerConfig &Config() const { return config_; }
    const Date &CurrentDate() const { return currentDate_; }
    const Manager &GetManager() const { return manager_; }
    int ManagerClubId() const { return manager_.clubId; }

    const std::map<int, Club> &Clubs() const { return clubs_; }
    const std::map<int, Player> &Players() const { return players_; }
    const std::vector<Season> &Seasons() const { return seasons_; }
    const Season &CurrentSeason() const { return seasons_[currentSeasonIndex_]; }
    Season &CurrentSeasonMutable() { return seasons_[currentSeasonIndex_]; }
    int CurrentSeasonIndex() const { return currentSeasonIndex_; }
    bool SeasonFinished() const { return seasons_[currentSeasonIndex_].finished; }

    const std::vector<Objective> &Objectives() const { return objectives_; }
    const std::vector<Scout> &Scouts() const { return scouts_; }
    const std::vector<ScoutReport> &ScoutReports() const { return scoutReports_; }
    const std::vector<AcademyReport> &AcademyReports() const { return academyReports_; }
    const std::vector<JobOffer> &JobOffers() const { return jobOffers_; }
    const std::vector<std::string> &News() const { return news_; }
    const std::vector<CareerAction> &Journal() const { return journal_; }
    const std::map<int, MatchOutcome> &RecordedOutcomes() const { return recordedOutcomes_; }

    long long SalaryArrears() const { return salaryArrears_; }
    bool IsTransferWindowOpen() const;
    bool IsTransferWindowOpenAtDate(const Date &date) const;
    bool IsManagedClub(int clubId) const;

    // Event-driven time: resolves every non-halt event until the closest halt,
    // returning all events crossed (the last one is the halt).
    std::vector<CareerEvent> Advance();

    // Applies and records one player decision. Returns false with a message on
    // an invalid decision (no state is mutated in that case).
    bool ApplyAction(const CareerAction &action, std::string *error = nullptr);

    // Applies a played 3D match outcome (external path). Replay injects the same
    // recorded snapshot instead of recomputing.
    void ApplyOutcome(const MatchOutcome &outcome);

    void AdvanceDays(int days);
    void AddIncome(long long amount, const std::string &reason);
    void AddExpense(long long amount, const std::string &reason);

    std::vector<int> BuildLineup(int clubId, CompetitionType competition) const;
    bool CanFieldTeam(int clubId, CompetitionType competition, std::string *why = nullptr) const;

    // Save / replay.
    std::string SerializeJson() const;
    bool SaveJson(const std::string &path, std::string *error = nullptr) const;
    static bool LoadJson(const std::string &path, CareerDataSource &dataSource, Career &out, std::string *error = nullptr);
    static Career Replay(uint32_t seed, const CareerConfig &config, CareerDataSource &dataSource,
                         const std::vector<CareerAction> &journal, const std::map<int, MatchOutcome> &recordedOutcomes);

    std::string Transcript() const;
    std::string Hash() const;

    // Convenience: current league table, cup winner, club strength.
    std::vector<StandingRow> LeagueTable(int division) const;
    int CupWinner() const;
    long long ClubStrength(int clubId) const;
    long long SquadValue(int clubId) const;

  private:
    friend struct CareerTestProbe;
    friend struct CareerJson;

    void BuildInitialState(CareerDataSource &dataSource);
    void GenerateSeason(int year);
    void BuildLeagueSchedule(Competition &competition, const std::vector<Date> &weekendDates);
    void BuildCupFirstRound(Competition &competition, const std::vector<Date> &midweekDates);
    void MaybeCreateNextCupRound(Competition &competition);
    std::vector<Date> WeekendDates(int year, int count) const;
    std::vector<Date> MidweekDates(int year, int count) const;

    struct Candidate {
      Date date;
      int priority = 99;
      CareerEventType type = e_CareerEvent_Monthly;
      int fixtureId = -1;
      int fixtureCompetition = e_Competition_League;
      bool valid = false;
    };
    Candidate FindNextEvent() const;
    void ProcessMonthly(const Date &date);
    void ProcessWindowBoundary(const Date &date);
    void ProcessScoutReports(const Date &date);
    void ProcessYouthReport(const Date &date);
    void ResolveFixture(int fixtureId);
    void ResolveFixtureWithOutcome(int fixtureId, const MatchOutcome &outcome);
    void ApplyOutcomeInternal(const MatchOutcome &outcome, bool record);
    MatchOutcome BuildWalkoverOutcome(const Fixture &fixture, int losingClubId) const;
    QuickSimTeam MakeQuickSimTeam(int clubId, CompetitionType competition) const;

    void PaySalaries(const Date &date);
    void RunRivalTransfers(const Date &date);
    void SeasonTransition();
    void EvaluateObjectivesForSeason();
    void CheckSportingObjectives();
    void IssueObjectives();
    void DevelopPlayers();
    void RetireAndIntake();
    void GenerateAcademyReport();
    void HireScoutInternal(int speedDays, int quality, long long salary, bool youth, const std::string &region);
    bool SignYouth(int reportId, int playerId, std::string *error);
    bool PromoteYouth(int playerId, long long salary, int years, std::string *error);
    bool ScoutPlayer(int playerId, int scoutId, std::string *error);
    bool TransferPlayer(int playerId, long long fee, long long salary, int years, std::string *error);
    bool SellPlayer(int playerId, long long fee, std::string *error);
    bool RenewPlayer(int playerId, long long salary, int years, std::string *error);
    bool ChooseSponsor(int index, std::string *error);
    void DismissManager(const std::string &reason);
    void CheckDismissal();
    void BuildJobOffers(bool includeFormerClub);
    bool AcceptJob(int clubId, std::string *error);
    void ProcessPendingJob(const Date &date);
    void SwitchClub(int clubId);
    Date NextWindowStart(const Date &date) const;

    Player &PlayerRef(int id) { return players_.find(id)->second; }
    const Player *FindPlayer(int id) const {
      std::map<int, Player>::const_iterator it = players_.find(id);
      return it == players_.end() ? nullptr : &it->second;
    }
    Club &ClubRef(int id) { return clubs_.find(id)->second; }
    const Club *FindClub(int id) const {
      std::map<int, Club>::const_iterator it = clubs_.find(id);
      return it == clubs_.end() ? nullptr : &it->second;
    }
    Fixture *FindFixture(int id);
    const Fixture *FindFixture(int id) const;
    Competition *FindCompetition(int id);
    const Competition *FindCompetition(int id) const;
    std::vector<int> ClubPlayerIds(int clubId) const;
    void AppendTranscript(const std::string &line);
    void AppendNews(const std::string &text);
    void AddIncomeInternal(long long amount);
    void AddExpenseInternal(long long amount);
    void RemovePlayerFromWorld(int playerId);
    void AddPlayerToWorld(const Player &player);
    std::string FixtureLine(const Fixture &fixture) const;
    void RecordEvent(const CareerEvent &event);
    void ResetSeasonStats();

    uint32_t seed_;
    CareerConfig config_;
    Manager manager_;
    Date currentDate_;

    std::map<int, Club> clubs_;
    std::map<int, Player> players_;
    std::vector<Season> seasons_;
    int currentSeasonIndex_ = 0;

    std::vector<Objective> objectives_;
    std::vector<Scout> scouts_;
    std::vector<ScoutReport> scoutReports_;
    std::vector<AcademyReport> academyReports_;
    std::vector<int> academyPlayerIds_;
    std::vector<JobOffer> jobOffers_;
    std::vector<std::string> news_;
    std::vector<std::string> transcript_;
    std::vector<CareerAction> journal_;
    std::map<int, MatchOutcome> recordedOutcomes_;

    long long salaryArrears_ = 0;
    long long nextSeasonBudget_ = 0;
    int academyPromotionsThisSeason_ = 0;
    int nextScoutId_ = 1;
    int nextObjectiveId_ = 1;
    int nextFixtureId_ = 1;
    int nextAcademyReportId_ = 1;
    int nextPlayerId_ = 1;
    int sponsorChosenSeason_ = -1;
    bool replaying_ = false;
    bool windowOpen_ = true;

    int pendingJobClubId_ = -1;
    Date pendingJobDate_;

    CareerRng rng_;
};

} // namespace career
} // namespace blunted

#endif
