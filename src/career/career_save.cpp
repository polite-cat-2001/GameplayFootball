// Career JSON save/load. The file is human-inspectable and contains the full
// mutable state, but loading reconstructs from the same seed + command journal
// (plus recorded 3D outcomes). That keeps one source of truth for the world and
// makes replay and load exercise the same deterministic path.

#include "career.hpp"
#include "career_json.hpp"

#include <fstream>
#include <sstream>

namespace blunted {
namespace career {

namespace {

JsonValue ConfigToJson(const CareerConfig &config) {
  JsonValue value = JsonValue::Object();
  value.Set("clubsPerDivision", JsonValue(config.clubsPerDivision));
  value.Set("seasonStartYear", JsonValue(config.seasonStartYear));
  value.Set("seasonStartMonth", JsonValue(config.seasonStartMonth));
  value.Set("seasonStartDay", JsonValue(config.seasonStartDay));
  value.Set("quickSimMoments", JsonValue(config.quickSimMoments));
  value.Set("homeAdvantage", JsonValue(config.homeAdvantage));
  value.Set("salaryDay", JsonValue(config.salaryDay));
  value.Set("trustDismissThreshold", JsonValue(config.trustDismissThreshold));
  value.Set("fatigueRecoveryPerDay", JsonValue(config.fatigueRecoveryPerDay));
  value.Set("startingBalanceFactor", JsonValue(config.startingBalanceFactor));
  value.Set("startingBalanceFloor", JsonValue(config.startingBalanceFloor));
  value.Set("scoutReportBaseDays", JsonValue(config.scoutReportBaseDays));
  value.Set("academyIntakeDay", JsonValue(config.academyIntakeDay));
  value.Set("academyIntakeMonth", JsonValue(config.academyIntakeMonth));
  value.Set("youthReportIntervalDays", JsonValue(config.youthReportIntervalDays));
  value.Set("maxScouts", JsonValue(config.maxScouts));
  value.Set("freeAgentCount", JsonValue(config.freeAgentCount));
  return value;
}

CareerConfig ConfigFromJson(const JsonValue &value) {
  CareerConfig config;
  if (!value.IsObject()) return config;
  const JsonValue *field = nullptr;
  if ((field = value.Find("clubsPerDivision"))) config.clubsPerDivision = field->AsIntValue(config.clubsPerDivision);
  if ((field = value.Find("seasonStartYear"))) config.seasonStartYear = field->AsIntValue(config.seasonStartYear);
  if ((field = value.Find("seasonStartMonth"))) config.seasonStartMonth = field->AsIntValue(config.seasonStartMonth);
  if ((field = value.Find("seasonStartDay"))) config.seasonStartDay = field->AsIntValue(config.seasonStartDay);
  if ((field = value.Find("quickSimMoments"))) config.quickSimMoments = field->AsIntValue(config.quickSimMoments);
  if ((field = value.Find("homeAdvantage"))) config.homeAdvantage = field->AsIntValue(config.homeAdvantage);
  if ((field = value.Find("salaryDay"))) config.salaryDay = field->AsIntValue(config.salaryDay);
  if ((field = value.Find("trustDismissThreshold")))
    config.trustDismissThreshold = field->AsIntValue(config.trustDismissThreshold);
  if ((field = value.Find("fatigueRecoveryPerDay")))
    config.fatigueRecoveryPerDay = field->AsIntValue(config.fatigueRecoveryPerDay);
  if ((field = value.Find("startingBalanceFactor")))
    config.startingBalanceFactor = field->AsIntValue(config.startingBalanceFactor);
  if ((field = value.Find("startingBalanceFloor")))
    config.startingBalanceFloor = field->AsInt(config.startingBalanceFloor);
  if ((field = value.Find("scoutReportBaseDays")))
    config.scoutReportBaseDays = field->AsIntValue(config.scoutReportBaseDays);
  if ((field = value.Find("academyIntakeDay"))) config.academyIntakeDay = field->AsIntValue(config.academyIntakeDay);
  if ((field = value.Find("academyIntakeMonth")))
    config.academyIntakeMonth = field->AsIntValue(config.academyIntakeMonth);
  if ((field = value.Find("youthReportIntervalDays")))
    config.youthReportIntervalDays = field->AsIntValue(config.youthReportIntervalDays);
  if ((field = value.Find("maxScouts"))) config.maxScouts = field->AsIntValue(config.maxScouts);
  if ((field = value.Find("freeAgentCount"))) config.freeAgentCount = field->AsIntValue(config.freeAgentCount);
  return config;
}

std::string ActionTypeName(CareerActionType type) {
  switch (type) {
    case e_Action_Advance: return "advance";
    case e_Action_SimNext: return "sim";
    case e_Action_PlayNext: return "play";
    case e_Action_Transfer: return "transfer";
    case e_Action_Sell: return "sell";
    case e_Action_Renew: return "renew";
    case e_Action_ScoutPlayer: return "scout";
    case e_Action_HireScout: return "hire_scout";
    case e_Action_FireScout: return "fire_scout";
    case e_Action_HireYouthScout: return "hire_youth_scout";
    case e_Action_SignYouth: return "sign_youth";
    case e_Action_PromoteYouth: return "promote_youth";
    case e_Action_ChooseSponsor: return "choose_sponsor";
    case e_Action_AcceptJob: return "accept_job";
    case e_Action_DeclineJob: return "decline_job";
  }
  return "advance";
}

CareerActionType ActionTypeFromName(const std::string &name) {
  if (name == "sim") return e_Action_SimNext;
  if (name == "play") return e_Action_PlayNext;
  if (name == "transfer") return e_Action_Transfer;
  if (name == "sell") return e_Action_Sell;
  if (name == "renew") return e_Action_Renew;
  if (name == "scout") return e_Action_ScoutPlayer;
  if (name == "hire_scout") return e_Action_HireScout;
  if (name == "fire_scout") return e_Action_FireScout;
  if (name == "hire_youth_scout") return e_Action_HireYouthScout;
  if (name == "sign_youth") return e_Action_SignYouth;
  if (name == "promote_youth") return e_Action_PromoteYouth;
  if (name == "choose_sponsor") return e_Action_ChooseSponsor;
  if (name == "accept_job") return e_Action_AcceptJob;
  if (name == "decline_job") return e_Action_DeclineJob;
  return e_Action_Advance;
}

JsonValue ActionToJson(const CareerAction &action) {
  JsonValue value = JsonValue::Object();
  value.Set("type", JsonValue(ActionTypeName(action.type)));
  value.Set("fixtureId", JsonValue(action.fixtureId));
  value.Set("playerId", JsonValue(action.playerId));
  value.Set("clubId", JsonValue(action.clubId));
  value.Set("scoutId", JsonValue(action.scoutId));
  value.Set("intValue", JsonValue(action.intValue));
  value.Set("amount", JsonValue(action.amount));
  value.Set("years", JsonValue(action.years));
  value.Set("text", JsonValue(action.text));
  return value;
}

CareerAction ActionFromJson(const JsonValue &value) {
  CareerAction action;
  action.type = ActionTypeFromName(value.Find("type") ? value.Find("type")->AsString("advance") : "advance");
  if (value.Find("fixtureId")) action.fixtureId = value.Find("fixtureId")->AsIntValue(-1);
  if (value.Find("playerId")) action.playerId = value.Find("playerId")->AsIntValue(-1);
  if (value.Find("clubId")) action.clubId = value.Find("clubId")->AsIntValue(-1);
  if (value.Find("scoutId")) action.scoutId = value.Find("scoutId")->AsIntValue(-1);
  if (value.Find("intValue")) action.intValue = value.Find("intValue")->AsIntValue(0);
  if (value.Find("amount")) action.amount = value.Find("amount")->AsInt(0);
  if (value.Find("years")) action.years = value.Find("years")->AsIntValue(1);
  if (value.Find("text")) action.text = value.Find("text")->AsString("");
  return action;
}

JsonValue OutcomeToJson(const MatchOutcome &outcome) {
  JsonValue value = JsonValue::Object();
  value.Set("fixtureId", JsonValue(outcome.fixtureId));
  value.Set("homeClubId", JsonValue(outcome.homeClubId));
  value.Set("awayClubId", JsonValue(outcome.awayClubId));
  value.Set("homeGoals", JsonValue(outcome.homeGoals));
  value.Set("awayGoals", JsonValue(outcome.awayGoals));
  value.Set("homePenalties", JsonValue(outcome.homePenalties));
  value.Set("awayPenalties", JsonValue(outcome.awayPenalties));
  value.Set("walkover", JsonValue(outcome.walkover));
  JsonValue lines = JsonValue::Array();
  for (size_t i = 0; i < outcome.lines.size(); i++) {
    const MatchLine &line = outcome.lines[i];
    JsonValue entry = JsonValue::Object();
    entry.Set("playerId", JsonValue(line.playerId));
    entry.Set("clubId", JsonValue(line.clubId));
    entry.Set("minutes", JsonValue(line.minutes));
    entry.Set("goals", JsonValue(line.goals));
    entry.Set("assists", JsonValue(line.assists));
    entry.Set("yellowCards", JsonValue(line.yellowCards));
    entry.Set("redCards", JsonValue(line.redCards));
    entry.Set("endFatigue", JsonValue(line.endFatigue));
    entry.Set("injured", JsonValue(line.injured));
    entry.Set("critical", JsonValue(line.critical));
    entry.Set("injuryType", JsonValue(line.injuryType));
    entry.Set("injuryDays", JsonValue(line.injuryDays));
    lines.arrayValue.push_back(entry);
  }
  value.Set("lines", lines);
  return value;
}

MatchOutcome OutcomeFromJson(const JsonValue &value) {
  MatchOutcome outcome;
  if (value.Find("fixtureId")) outcome.fixtureId = value.Find("fixtureId")->AsIntValue(-1);
  if (value.Find("homeClubId")) outcome.homeClubId = value.Find("homeClubId")->AsIntValue(-1);
  if (value.Find("awayClubId")) outcome.awayClubId = value.Find("awayClubId")->AsIntValue(-1);
  if (value.Find("homeGoals")) outcome.homeGoals = value.Find("homeGoals")->AsIntValue(0);
  if (value.Find("awayGoals")) outcome.awayGoals = value.Find("awayGoals")->AsIntValue(0);
  if (value.Find("homePenalties")) outcome.homePenalties = value.Find("homePenalties")->AsIntValue(-1);
  if (value.Find("awayPenalties")) outcome.awayPenalties = value.Find("awayPenalties")->AsIntValue(-1);
  if (value.Find("walkover")) outcome.walkover = value.Find("walkover")->AsBool(false);
  if (value.Find("lines")) {
    const std::vector<JsonValue> &lines = value.Find("lines")->AsArray();
    for (size_t i = 0; i < lines.size(); i++) {
      const JsonValue &entry = lines[i];
      MatchLine line;
      if (entry.Find("playerId")) line.playerId = entry.Find("playerId")->AsIntValue(0);
      if (entry.Find("clubId")) line.clubId = entry.Find("clubId")->AsIntValue(0);
      if (entry.Find("minutes")) line.minutes = entry.Find("minutes")->AsIntValue(0);
      if (entry.Find("goals")) line.goals = entry.Find("goals")->AsIntValue(0);
      if (entry.Find("assists")) line.assists = entry.Find("assists")->AsIntValue(0);
      if (entry.Find("yellowCards")) line.yellowCards = entry.Find("yellowCards")->AsIntValue(0);
      if (entry.Find("redCards")) line.redCards = entry.Find("redCards")->AsIntValue(0);
      if (entry.Find("endFatigue")) line.endFatigue = entry.Find("endFatigue")->AsIntValue(100);
      if (entry.Find("injured")) line.injured = entry.Find("injured")->AsBool(false);
      if (entry.Find("critical")) line.critical = entry.Find("critical")->AsBool(false);
      if (entry.Find("injuryType")) line.injuryType = entry.Find("injuryType")->AsString("none");
      if (entry.Find("injuryDays")) line.injuryDays = entry.Find("injuryDays")->AsIntValue(0);
      outcome.lines.push_back(line);
    }
  }
  return outcome;
}

} // namespace

struct CareerJson {
  static std::string Serialize(const Career &career) {
    JsonValue root = JsonValue::Object();
    root.Set("version", JsonValue(1));
    root.Set("seed", JsonValue((long long)career.seed_));
    root.Set("config", ConfigToJson(career.config_));
    root.Set("date", JsonValue(career.currentDate_.ToString()));
    root.Set("windowOpen", JsonValue(career.windowOpen_));

    JsonValue manager = JsonValue::Object();
    manager.Set("name", JsonValue(career.manager_.name));
    manager.Set("nationality", JsonValue(career.manager_.nationality));
    manager.Set("reputation", JsonValue(career.manager_.reputation));
    manager.Set("clubId", JsonValue(career.manager_.clubId));
    root.Set("manager", manager);

    JsonValue metrics = JsonValue::Object();
    if (career.manager_.clubId >= 0) {
      const Club &club = career.clubs_.find(career.manager_.clubId)->second;
      metrics.Set("boardTrust", JsonValue(club.boardTrust));
      metrics.Set("subscribers", JsonValue(club.subscribers));
      metrics.Set("fanSatisfaction", JsonValue(club.fanSatisfaction));
      metrics.Set("reputation", JsonValue(career.manager_.reputation));
    }
    root.Set("metrics", metrics);

    JsonValue finance = JsonValue::Object();
    if (career.manager_.clubId >= 0) {
      const Club &club = career.clubs_.find(career.manager_.clubId)->second;
      finance.Set("balance", JsonValue(club.balance));
    }
    finance.Set("arrears", JsonValue(career.salaryArrears_));
    finance.Set("nextSeasonBudget", JsonValue(career.nextSeasonBudget_));
    root.Set("finance", finance);

    JsonValue clubs = JsonValue::Array();
    for (std::map<int, Club>::const_iterator it = career.clubs_.begin(); it != career.clubs_.end(); ++it) {
      const Club &club = it->second;
      JsonValue entry = JsonValue::Object();
      entry.Set("id", JsonValue(club.id));
      entry.Set("name", JsonValue(club.name));
      entry.Set("shortName", JsonValue(club.shortName));
      entry.Set("division", JsonValue(club.division));
      entry.Set("reputation", JsonValue(club.reputation));
      entry.Set("managed", JsonValue(club.isManaged));
      entry.Set("balance", JsonValue(club.balance));
      entry.Set("seasonStartBalance", JsonValue(club.seasonStartBalance));
      entry.Set("subscribers", JsonValue(club.subscribers));
      entry.Set("fanSatisfaction", JsonValue(club.fanSatisfaction));
      entry.Set("boardTrust", JsonValue(club.boardTrust));
      entry.Set("sponsorIndex", JsonValue(club.sponsorIndex));
      entry.Set("formation", JsonValue(club.formation));
      clubs.arrayValue.push_back(entry);
    }
    root.Set("clubs", clubs);

    JsonValue players = JsonValue::Array();
    for (std::map<int, Player>::const_iterator it = career.players_.begin(); it != career.players_.end(); ++it) {
      const Player &player = it->second;
      JsonValue entry = JsonValue::Object();
      entry.Set("id", JsonValue(player.id));
      entry.Set("name", JsonValue(player.name));
      entry.Set("position", JsonValue(PositionName(player.position)));
      entry.Set("birthDate", JsonValue(player.birthDate.ToString()));
      entry.Set("baseStat", JsonValue((double)player.baseStat));
      entry.Set("potentialOvr", JsonValue(player.potentialOvr));
      entry.Set("marketValue", JsonValue(player.marketValue));
      entry.Set("clubId", JsonValue(player.clubId));
      entry.Set("fatigue", JsonValue(player.condition.fatigue));
      entry.Set("injuryActive", JsonValue(player.condition.injury.active));
      entry.Set("injuryDays", JsonValue(player.condition.injury.daysRemaining));
      entry.Set("injuryCritical", JsonValue(player.condition.injury.critical));
      entry.Set("leagueBan", JsonValue(player.condition.leagueBan));
      entry.Set("cupBan", JsonValue(player.condition.cupBan));
      entry.Set("salary", JsonValue(player.contract.annualSalary));
      entry.Set("years", JsonValue(player.contract.yearsRemaining));
      entry.Set("isYouth", JsonValue(player.isYouth));
      entry.Set("youthClubId", JsonValue(player.youthClubId));
      players.arrayValue.push_back(entry);
    }
    root.Set("players", players);

    JsonValue seasons = JsonValue::Array();
    for (size_t i = 0; i < career.seasons_.size(); i++) {
      const Season &season = career.seasons_[i];
      JsonValue entry = JsonValue::Object();
      entry.Set("year", JsonValue(season.year));
      entry.Set("startDate", JsonValue(season.startDate.ToString()));
      entry.Set("endDate", JsonValue(season.endDate.ToString()));
      entry.Set("finished", JsonValue(season.finished));
      entry.Set("current", JsonValue((long long)i == (long long)career.currentSeasonIndex_));
      seasons.arrayValue.push_back(entry);
    }
    root.Set("seasons", seasons);

    JsonValue objectives = JsonValue::Array();
    for (size_t i = 0; i < career.objectives_.size(); i++) {
      const Objective &objective = career.objectives_[i];
      JsonValue entry = JsonValue::Object();
      entry.Set("id", JsonValue(objective.id));
      entry.Set("category", JsonValue(ObjectiveCategoryName(objective.category)));
      entry.Set("urgency", JsonValue(ObjectiveUrgencyName(objective.urgency)));
      entry.Set("description", JsonValue(objective.description));
      entry.Set("deadlineSeason", JsonValue(objective.deadlineSeason));
      entry.Set("evaluated", JsonValue(objective.evaluated));
      entry.Set("completed", JsonValue(objective.completed));
      objectives.arrayValue.push_back(entry);
    }
    root.Set("objectives", objectives);

    JsonValue scouting = JsonValue::Object();
    JsonValue scouts = JsonValue::Array();
    for (size_t i = 0; i < career.scouts_.size(); i++) {
      const Scout &scout = career.scouts_[i];
      JsonValue entry = JsonValue::Object();
      entry.Set("id", JsonValue(scout.id));
      entry.Set("name", JsonValue(scout.name));
      entry.Set("speedDays", JsonValue(scout.speedDays));
      entry.Set("quality", JsonValue(scout.quality));
      entry.Set("salary", JsonValue(scout.annualSalary));
      entry.Set("youthScout", JsonValue(scout.youthScout));
      entry.Set("region", JsonValue(scout.region));
      scouts.arrayValue.push_back(entry);
    }
    scouting.Set("scouts", scouts);
    JsonValue reports = JsonValue::Array();
    for (size_t i = 0; i < career.scoutReports_.size(); i++) {
      const ScoutReport &report = career.scoutReports_[i];
      JsonValue entry = JsonValue::Object();
      entry.Set("scoutId", JsonValue(report.scoutId));
      entry.Set("playerId", JsonValue(report.playerId));
      entry.Set("dueDate", JsonValue(report.dueDate.ToString()));
      entry.Set("ready", JsonValue(report.ready));
      entry.Set("exactOvr", JsonValue(report.exactOvr));
      reports.arrayValue.push_back(entry);
    }
    scouting.Set("reports", reports);
    root.Set("scouting", scouting);

    JsonValue academy = JsonValue::Object();
    JsonValue academyPlayers = JsonValue::Array();
    for (size_t i = 0; i < career.academyPlayerIds_.size(); i++) {
      academyPlayers.arrayValue.push_back(JsonValue(career.academyPlayerIds_[i]));
    }
    academy.Set("players", academyPlayers);
    root.Set("academy", academy);

    JsonValue news = JsonValue::Array();
    for (size_t i = 0; i < career.news_.size(); i++) news.arrayValue.push_back(JsonValue(career.news_[i]));
    root.Set("news", news);

    JsonValue journal = JsonValue::Array();
    for (size_t i = 0; i < career.journal_.size(); i++) journal.arrayValue.push_back(ActionToJson(career.journal_[i]));
    root.Set("journal", journal);

    JsonValue outcomes = JsonValue::Array();
    for (std::map<int, MatchOutcome>::const_iterator it = career.recordedOutcomes_.begin();
         it != career.recordedOutcomes_.end(); ++it) {
      outcomes.arrayValue.push_back(OutcomeToJson(it->second));
    }
    root.Set("outcomes", outcomes);

    return root.Dump();
  }

  static bool Load(const std::string &path, CareerDataSource &dataSource, Career &out, std::string *error) {
    std::ifstream file(path.c_str(), std::ios::binary);
    if (!file) {
      if (error) *error = "could not open " + path;
      return false;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    std::string parseError;
    JsonValue root = JsonValue::Parse(buffer.str(), &parseError);
    if (!root.IsObject()) {
      if (error) *error = "invalid save: " + parseError;
      return false;
    }
    if (root.Find("version") == nullptr || root.Find("version")->AsIntValue(0) != 1) {
      if (error) *error = "unsupported save version";
      return false;
    }
    uint32_t seed = static_cast<uint32_t>(root.Find("seed") ? root.Find("seed")->AsInt(0) : 0);
    CareerConfig config = root.Find("config") ? ConfigFromJson(*root.Find("config")) : CareerConfig();

    Career loaded(seed, config, dataSource);
    loaded.replaying_ = true;
    if (root.Find("outcomes")) {
      const std::vector<JsonValue> &outcomes = root.Find("outcomes")->AsArray();
      for (size_t i = 0; i < outcomes.size(); i++) {
        MatchOutcome outcome = OutcomeFromJson(outcomes[i]);
        loaded.recordedOutcomes_[outcome.fixtureId] = outcome;
      }
    }
    if (root.Find("journal")) {
      const std::vector<JsonValue> &journal = root.Find("journal")->AsArray();
      for (size_t i = 0; i < journal.size(); i++) {
        CareerAction action = ActionFromJson(journal[i]);
        if (action.type == e_Action_Advance) loaded.Advance();
        else loaded.ApplyAction(action);
      }
    }
    out = loaded;
    return true;
  }
};

std::string Career::SerializeJson() const { return CareerJson::Serialize(*this); }

bool Career::SaveJson(const std::string &path, std::string *error) const {
  std::ofstream file(path.c_str(), std::ios::binary);
  if (!file) {
    if (error) *error = "could not open " + path;
    return false;
  }
  file << SerializeJson();
  return true;
}

bool Career::LoadJson(const std::string &path, CareerDataSource &dataSource, Career &out, std::string *error) {
  return CareerJson::Load(path, dataSource, out, error);
}

Career Career::Replay(uint32_t seed, const CareerConfig &config, CareerDataSource &dataSource,
                      const std::vector<CareerAction> &journal, const std::map<int, MatchOutcome> &recordedOutcomes) {
  Career replay(seed, config, dataSource);
  replay.replaying_ = true;
  replay.recordedOutcomes_ = recordedOutcomes;
  for (size_t i = 0; i < journal.size(); i++) {
    const CareerAction &action = journal[i];
    if (action.type == e_Action_Advance) replay.Advance();
    else replay.ApplyAction(action);
  }
  return replay;
}

} // namespace career
} // namespace blunted
