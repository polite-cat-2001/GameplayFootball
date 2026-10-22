// Manager career v1: pure data types shared by the headless career module.
// This module deliberately has no dependency on GetDB(), the global random()
// stream or time_ms; every random decision comes from CareerRng seeded by the
// career seed.

#ifndef _HPP_CAREER_TYPES
#define _HPP_CAREER_TYPES

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

namespace blunted {
namespace career {

typedef float real;

const real kPi = 3.14159265358979323846f;

inline real clampValue(real value, real minValue, real maxValue) {
  if (value < minValue) return minValue;
  if (value > maxValue) return maxValue;
  return value;
}

inline real normalizedClamp(real value, real minValue, real maxValue) {
  if (maxValue <= minValue) return 0.0f;
  return (clampValue(value, minValue, maxValue) - minValue) / (maxValue - minValue);
}

inline real curve(real source, real bias = 1.0f) {
  return (std::sin((source - 0.5f) * kPi) * 0.5f + 0.5f) * bias +
         source * (1.0f - bias);
}

// A calendar date with deterministic civil-day arithmetic (no locale, no DST).
struct Date {
  int year = 1970;
  int month = 1;
  int day = 1;

  Date() {}
  Date(int y, int m, int d) : year(y), month(m), day(d) {}

  static int DaysFromCivil(int y, unsigned m, unsigned d) {
    y -= m <= 2;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<int>(doe) - 719468;
  }

  static Date CivilFromDays(int z) {
    z += 719468;
    const int era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = static_cast<unsigned>(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const int y = static_cast<int>(yoe) + era * 400;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    const unsigned d = doy - (153 * mp + 2) / 5 + 1;
    const unsigned m = mp + (mp < 10 ? 3 : -9);
    return Date(y + (m <= 2), static_cast<int>(m), static_cast<int>(d));
  }

  int Ordinal() const { return DaysFromCivil(year, static_cast<unsigned>(month), static_cast<unsigned>(day)); }

  Date AddDays(int days) const { return CivilFromDays(Ordinal() + days); }

  // 0 = Sunday, 1 = Monday, ... 6 = Saturday.
  int Weekday() const {
    int value = (Ordinal() + 4) % 7;
    if (value < 0) value += 7;
    return value;
  }

  bool operator<(const Date &other) const { return Ordinal() < other.Ordinal(); }
  bool operator<=(const Date &other) const { return Ordinal() <= other.Ordinal(); }
  bool operator>(const Date &other) const { return Ordinal() > other.Ordinal(); }
  bool operator>=(const Date &other) const { return Ordinal() >= other.Ordinal(); }
  bool operator==(const Date &other) const {
    return year == other.year && month == other.month && day == other.day;
  }
  bool operator!=(const Date &other) const { return !(*this == other); }

  std::string ToString() const {
    char buffer[24];
    std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", year, month, day);
    return std::string(buffer);
  }

  static Date Parse(const std::string &text) {
    Date result;
    if (text.size() >= 10) {
      result.year = std::atoi(text.substr(0, 4).c_str());
      result.month = std::atoi(text.substr(5, 2).c_str());
      result.day = std::atoi(text.substr(8, 2).c_str());
    }
    return result;
  }

  int AgeOn(const Date &today) const {
    int value = today.year - year;
    if (today.month < month || (today.month == month && today.day < day)) value--;
    return value;
  }
};

enum Position {
  e_Position_GK,
  e_Position_CB,
  e_Position_LB,
  e_Position_RB,
  e_Position_DM,
  e_Position_CM,
  e_Position_LM,
  e_Position_RM,
  e_Position_AM,
  e_Position_LW,
  e_Position_RW,
  e_Position_CF,
  e_Position_ST
};

inline const char *PositionName(Position position) {
  switch (position) {
    case e_Position_GK: return "GK";
    case e_Position_CB: return "CB";
    case e_Position_LB: return "LB";
    case e_Position_RB: return "RB";
    case e_Position_DM: return "DM";
    case e_Position_CM: return "CM";
    case e_Position_LM: return "LM";
    case e_Position_RM: return "RM";
    case e_Position_AM: return "AM";
    case e_Position_LW: return "LW";
    case e_Position_RW: return "RW";
    case e_Position_CF: return "CF";
    case e_Position_ST: return "ST";
  }
  return "CM";
}

inline Position ParsePosition(const std::string &name) {
  if (name == "GK") return e_Position_GK;
  if (name == "CB") return e_Position_CB;
  if (name == "LB") return e_Position_LB;
  if (name == "RB") return e_Position_RB;
  if (name == "DM") return e_Position_DM;
  if (name == "LM") return e_Position_LM;
  if (name == "RM") return e_Position_RM;
  if (name == "AM") return e_Position_AM;
  if (name == "LW") return e_Position_LW;
  if (name == "RW") return e_Position_RW;
  if (name == "CF") return e_Position_CF;
  if (name == "ST") return e_Position_ST;
  return e_Position_CM;
}

inline bool IsGoalkeeper(Position position) { return position == e_Position_GK; }
inline bool IsDefender(Position position) {
  return position == e_Position_CB || position == e_Position_LB || position == e_Position_RB;
}
inline bool IsMidfielder(Position position) {
  return position == e_Position_DM || position == e_Position_CM || position == e_Position_LM ||
         position == e_Position_RM || position == e_Position_AM;
}
inline bool IsForward(Position position) {
  return position == e_Position_LW || position == e_Position_RW || position == e_Position_CF ||
         position == e_Position_ST;
}

// Peak age of the positional development curve (wave 3 decisions).
inline int DevelopmentPeakAge(Position position) {
  if (position == e_Position_GK) return 33;
  if (position == e_Position_CB) return 31;
  if (position == e_Position_LB || position == e_Position_RB) return 29;
  if (position == e_Position_DM || position == e_Position_CM) return 29;
  if (position == e_Position_LM || position == e_Position_RM) return 28;
  if (position == e_Position_AM) return 27;
  if (position == e_Position_LW || position == e_Position_RW) return 26;
  return 26;
}

// Displayed rating: the same age-adjusted maths as CalculateStat(base, 0.5, age)
// in src/utils.cpp, expressed in 0-100 OVR units. The engine formula is the canon
// for the number shown to the player.
inline int CalculateOvr(real baseStat, int age) {
  real idealAge = 27.0f;
  real ageFactor = curve(1.0f - normalizedClamp(std::fabs(float(age) - idealAge), 0.0f, 13.0f) * 0.5f, 1.0f) * 2.0f - 1.0f;
  real agedBaseStat = baseStat * (ageFactor * 0.5f + 0.5f) * 1.2f;
  real agedProfileStat = clampValue(0.5f * 2.0f * agedBaseStat, 0.01f, 1.0f);
  return static_cast<int>(std::lround(agedProfileStat * 100.0f));
}

inline long long MarketValueForOvr(int ovr) {
  real value = 100000.0f * std::exp((float(ovr) - 50.0f) / 12.0f);
  long long rounded = static_cast<long long>(value / 10000.0f + 0.5f) * 10000LL;
  if (rounded < 10000LL) rounded = 10000LL;
  return rounded;
}

struct Injury {
  bool active = false;
  bool critical = false;
  int daysRemaining = 0;
  std::string type = "none";
};

struct Condition {
  // 0 = exhausted, 100 = fully fresh.
  int fatigue = 100;
  Injury injury;
  int leagueBan = 0;
  int cupBan = 0;
};

enum CompetitionType {
  e_Competition_League,
  e_Competition_Cup
};

inline bool IsAvailable(const Condition &condition, CompetitionType competition) {
  if (condition.injury.active) return false;
  if (competition == e_Competition_Cup) return condition.cupBan <= 0;
  return condition.leagueBan <= 0;
}

struct Contract {
  long long annualSalary = 0;
  int yearsRemaining = 1;
};

struct PlayerSeed {
  int id = 0;
  std::string name;
  Position position = e_Position_CM;
  Date birthDate;
  real baseStat = 0.5f;
  int potentialOvr = 60;
  long long marketValue = 0;
  int clubId = -1;
};

struct Player {
  int id = 0;
  std::string name;
  Position position = e_Position_CM;
  Date birthDate;
  real baseStat = 0.5f;
  int potentialOvr = 60;
  long long marketValue = 0;
  int clubId = -1;
  Condition condition;
  Contract contract;
  bool isYouth = false;
  int youthClubId = -1;
  int minutesThisSeason = 0;
  int goalsThisSeason = 0;
  int assistsThisSeason = 0;

  int OvrOn(const Date &date) const { return CalculateOvr(baseStat, birthDate.AgeOn(date)); }
};

struct ClubSeed {
  int id = 0;
  std::string name;
  std::string shortName;
  int division = 1;
  int reputation = 50;
  std::vector<PlayerSeed> players;
};

struct Club {
  int id = 0;
  std::string name;
  std::string shortName;
  int division = 1;
  int reputation = 50;
  bool isManaged = false;
  long long balance = 0;
  long long seasonStartBalance = 0;
  int subscribers = 100000;
  int fanSatisfaction = 50;
  int boardTrust = 55;
  int sponsorIndex = 0;
  std::string formation = "4-4-2";
};

struct Manager {
  std::string name = "Manager";
  std::string nationality = "Syntheticland";
  int reputation = 45;
  int clubId = -1;
};

struct MatchLine {
  int playerId = 0;
  int clubId = 0;
  int minutes = 0;
  int goals = 0;
  int assists = 0;
  int yellowCards = 0;
  int redCards = 0;
  int endFatigue = 100;
  bool injured = false;
  bool critical = false;
  std::string injuryType = "none";
  int injuryDays = 0;
};

struct MatchOutcome {
  int fixtureId = -1;
  int homeClubId = -1;
  int awayClubId = -1;
  int homeGoals = 0;
  int awayGoals = 0;
  int homePenalties = -1;
  int awayPenalties = -1;
  bool walkover = false;
  std::vector<MatchLine> lines;
};

enum SlotType {
  e_Slot_Weekend,
  e_Slot_Midweek,
  e_Slot_Break
};

inline const char *SlotTypeName(SlotType type) {
  if (type == e_Slot_Weekend) return "weekend";
  if (type == e_Slot_Midweek) return "midweek";
  return "break";
}

struct CalendarSlot {
  Date date;
  SlotType type = e_Slot_Weekend;
};

struct StandingRow {
  int clubId = 0;
  int played = 0;
  int won = 0;
  int drawn = 0;
  int lost = 0;
  int goalsFor = 0;
  int goalsAgainst = 0;
  int points = 0;

  int GoalDifference() const { return goalsFor - goalsAgainst; }
};

struct Fixture {
  int id = 0;
  int competitionId = 0;
  int round = 0;
  Date date;
  int homeClubId = -1;
  int awayClubId = -1;
  int homeGoals = -1;
  int awayGoals = -1;
  int homePenalties = -1;
  int awayPenalties = -1;
  bool played = false;
  bool bye = false;
};

struct Competition {
  int id = 0;
  CompetitionType type = e_Competition_League;
  int division = 0;
  std::string name;
  std::vector<int> clubIds;
};

struct Season {
  int year = 2026;
  Date startDate;
  Date endDate;
  std::vector<CalendarSlot> slots;
  std::vector<Competition> competitions;
  std::vector<Fixture> fixtures;
  bool finished = false;
};

enum ObjectiveCategory {
  e_Objective_Financial,
  e_Objective_Image,
  e_Objective_Sporting,
  e_Objective_Academy
};

enum ObjectiveUrgency {
  e_Urgency_Short,
  e_Urgency_Medium,
  e_Urgency_Long
};

inline const char *ObjectiveCategoryName(ObjectiveCategory category) {
  switch (category) {
    case e_Objective_Financial: return "financial";
    case e_Objective_Image: return "image";
    case e_Objective_Sporting: return "sporting";
    case e_Objective_Academy: return "academy";
  }
  return "unknown";
}

inline const char *ObjectiveUrgencyName(ObjectiveUrgency urgency) {
  switch (urgency) {
    case e_Urgency_Short: return "short";
    case e_Urgency_Medium: return "medium";
    case e_Urgency_Long: return "long";
  }
  return "unknown";
}

struct ObjectiveDeltas {
  int trust = 0;
  int fanSatisfaction = 0;
  long long nextBudget = 0;
  int reputation = 0;
};

struct Objective {
  int id = 0;
  ObjectiveCategory category = e_Objective_Sporting;
  ObjectiveUrgency urgency = e_Urgency_Short;
  std::string description;
  int targetPoints = 0;
  int targetPosition = 0;
  long long targetProfit = 0;
  int targetSubscribers = 0;
  int targetAcademyPromotions = 0;
  int deadlineSeason = 0;
  int issuedSeason = 0;
  int progress = 0;
  bool evaluated = false;
  bool completed = false;
  bool failed = false;
  ObjectiveDeltas reward;
  ObjectiveDeltas penalty;
};

struct Scout {
  int id = 0;
  std::string name;
  int speedDays = 20;
  int quality = 50;
  long long annualSalary = 20000;
  bool youthScout = false;
  std::string region;
  Date nextReportDate;
};

struct ScoutReport {
  int scoutId = 0;
  int playerId = -1;
  Date dueDate;
  bool ready = false;
  int exactOvr = 0;
};

struct AcademyReport {
  int id = 0;
  Date created;
  std::vector<int> playerIds;
  std::vector<int> claimedByAcademy;
};

struct JobOffer {
  int clubId = -1;
  Date effectiveDate;
  int reputationRequired = 0;
  std::string reason;
};

struct CareerConfig {
  int clubsPerDivision = 8;
  int seasonStartYear = 2026;
  int seasonStartMonth = 8;
  int seasonStartDay = 8;
  int quickSimMoments = 18;
  int homeAdvantage = 4;
  int salaryDay = 30;
  int trustDismissThreshold = 20;
  int fatigueRecoveryPerDay = 4;
  int startingBalanceFactor = 1; // multiplied by the squad value
  long long startingBalanceFloor = 1500000;
  int scoutReportBaseDays = 30;
  int academyIntakeDay = 1;
  int academyIntakeMonth = 10;
  int youthReportIntervalDays = 90;
  int maxScouts = 3;
  int freeAgentCount = 16;
};

inline int PositionWeightAttack(Position position) {
  if (position == e_Position_ST || position == e_Position_CF) return 5;
  if (position == e_Position_LW || position == e_Position_RW) return 4;
  if (position == e_Position_AM) return 3;
  if (position == e_Position_LM || position == e_Position_RM) return 2;
  if (position == e_Position_CM) return 1;
  return 0;
}

inline int PositionWeightDefense(Position position) {
  if (position == e_Position_CB) return 5;
  if (position == e_Position_LB || position == e_Position_RB) return 4;
  if (position == e_Position_DM) return 4;
  if (position == e_Position_CM || position == e_Position_LM || position == e_Position_RM) return 2;
  if (position == e_Position_AM) return 1;
  return 0;
}

inline int PositionWeightGoal(Position position) {
  if (position == e_Position_ST) return 6;
  if (position == e_Position_CF) return 5;
  if (position == e_Position_LW || position == e_Position_RW) return 4;
  if (position == e_Position_AM) return 3;
  if (position == e_Position_CM || position == e_Position_LM || position == e_Position_RM) return 2;
  if (position == e_Position_DM) return 1;
  return 0;
}

inline int PositionWeightAssist(Position position) {
  if (position == e_Position_AM) return 5;
  if (position == e_Position_LW || position == e_Position_RW) return 5;
  if (position == e_Position_CM || position == e_Position_LM || position == e_Position_RM) return 4;
  if (position == e_Position_CF || position == e_Position_ST) return 3;
  if (position == e_Position_DM) return 2;
  return 0;
}

inline int PositionWeightCard(Position position) {
  if (position == e_Position_CB || position == e_Position_DM) return 5;
  if (position == e_Position_LB || position == e_Position_RB) return 4;
  if (position == e_Position_CM || position == e_Position_LM || position == e_Position_RM) return 3;
  if (position == e_Position_AM) return 1;
  return 1;
}

} // namespace career
} // namespace blunted

#endif
