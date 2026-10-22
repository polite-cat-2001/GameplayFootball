// Headless career runner. Prints a deterministic line transcript of a career,
// or its SHA-1 hash for the golden regression check.
//
//   career_runner <seed> [--seasons N] [--actions file] [--replay]
//   career_runner <seed> --hash [--seasons N]
//   career_runner <seed> --check <hash> [--seasons N]

#include "career/career.hpp"
#include "career/career_data_source.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace blunted;
using namespace blunted::career;

namespace {

std::vector<int> FreeAgentsByOvr(const Career &career) {
  std::vector<std::pair<int, int> > values;
  for (std::map<int, Player>::const_iterator it = career.Players().begin(); it != career.Players().end(); ++it) {
    if (it->second.clubId >= 0 || it->second.isYouth) continue;
    values.push_back(std::make_pair(-it->second.OvrOn(career.CurrentDate()), it->first));
  }
  std::sort(values.begin(), values.end());
  std::vector<int> result;
  for (size_t i = 0; i < values.size(); i++) result.push_back(values[i].second);
  return result;
}

void InitialAutoActions(Career &career) {
  std::string error;
  career.ApplyAction(CareerAction::HireScout(18, 72, 30000), &error);
  career.ApplyAction(CareerAction::HireYouthScout(66, 22000, "Europe"), &error);
  career.ApplyAction(CareerAction::ChooseSponsor(1), &error);

  std::vector<int> freeAgents = FreeAgentsByOvr(career);
  if (!freeAgents.empty()) {
    const Player *target = nullptr;
    std::map<int, Player>::const_iterator it = career.Players().find(freeAgents[0]);
    if (it != career.Players().end()) target = &it->second;
    if (target) {
      long long salary = std::max(40000LL, target->marketValue / 8LL);
      career.ApplyAction(CareerAction::Transfer(target->id, 0, salary, 2), &error);
      if (!career.Scouts().empty()) career.ApplyAction(CareerAction::Scout(target->id, career.Scouts()[0].id), &error);
    }
  }
}

void OnYouthReport(Career &career) {
  if (career.AcademyReports().empty()) return;
  const AcademyReport &report = career.AcademyReports().back();
  if (report.playerIds.empty()) return;
  std::string error;
  career.ApplyAction(CareerAction::SignYouth(report.id, report.playerIds[0]), &error);

  for (size_t i = 0; i < career.AcademyReports().size(); i++) {
    const AcademyReport &candidate = career.AcademyReports()[i];
    for (size_t p = 0; p < candidate.playerIds.size(); p++) {
      std::map<int, Player>::const_iterator it = career.Players().find(candidate.playerIds[p]);
      if (it == career.Players().end()) continue;
      if (!it->second.isYouth || it->second.youthClubId < 0) continue;
      if (it->second.birthDate.AgeOn(career.CurrentDate()) >= 16) {
        long long salary = std::max(30000LL, it->second.marketValue / 8LL);
        career.ApplyAction(CareerAction::PromoteYouth(it->first, salary, 3), &error);
      }
    }
  }
}

void OnWindowOpen(Career &career) {
  // A deterministic, budget-respecting squad top-up: buy the cheapest free
  // agent that the balance can carry. This exercises the transfer path without
  // making the transcript depend on the transfer market's mood.
  if (career.ManagerClubId() < 0) return;
  std::vector<int> freeAgents = FreeAgentsByOvr(career);
  if (freeAgents.empty()) return;
  int playerId = freeAgents.back();
  std::map<int, Player>::const_iterator it = career.Players().find(playerId);
  if (it == career.Players().end()) return;
  long long salary = std::max(30000LL, it->second.marketValue / 10LL);
  std::string error;
  career.ApplyAction(CareerAction::Transfer(playerId, 0, salary, 2), &error);
}

bool RunAuto(Career &career, int seasons) {
  InitialAutoActions(career);
  int startSeason = career.CurrentSeasonIndex();
  int targetSeason = startSeason + seasons;
  for (int guard = 0; guard < 200000; guard++) {
    if (career.CurrentSeasonIndex() >= targetSeason) break;
    std::vector<CareerEvent> events = career.Advance();
    if (events.empty()) break;
    for (size_t i = 0; i < events.size(); i++) {
      const CareerEvent &event = events[i];
      if (event.type == e_CareerEvent_Fixture && event.halt) {
        std::string error;
        career.ApplyAction(CareerAction::Sim(event.fixtureId), &error);
      } else if (event.type == e_CareerEvent_YouthReport) {
        OnYouthReport(career);
      } else if (event.type == e_CareerEvent_TransferWindowOpen) {
        OnWindowOpen(career);
      } else if (event.type == e_CareerEvent_SeasonEnd) {
        std::string error;
        career.ApplyAction(CareerAction::ChooseSponsor(1), &error);
      }
    }
  }
  return true;
}

void RunScript(Career &career, const std::string &path) {
  std::ifstream file(path.c_str());
  if (!file) {
    std::cerr << "could not open actions file " << path << "\n";
    return;
  }
  std::string line;
  while (std::getline(file, line)) {
    std::istringstream stream(line);
    std::string command;
    stream >> command;
    if (command.empty() || command[0] == '#') continue;
    std::string error;
    if (command == "advance") {
      career.Advance();
    } else if (command == "sim") {
      for (int guard = 0; guard < 200000; guard++) {
        std::vector<CareerEvent> events = career.Advance();
        if (events.empty()) break;
        const CareerEvent &last = events.back();
        if (last.type == e_CareerEvent_Fixture && last.halt) {
          career.ApplyAction(CareerAction::Sim(last.fixtureId), &error);
          break;
        }
      }
    } else if (command == "season") {
      int start = career.CurrentSeasonIndex();
      for (int guard = 0; guard < 200000 && career.CurrentSeasonIndex() == start; guard++) {
        std::vector<CareerEvent> events = career.Advance();
        if (events.empty()) break;
        const CareerEvent &last = events.back();
        if (last.type == e_CareerEvent_Fixture && last.halt) {
          career.ApplyAction(CareerAction::Sim(last.fixtureId), &error);
        }
      }
    } else if (command == "transfer") {
      int playerId = 0, years = 0;
      long long fee = 0, salary = 0;
      stream >> playerId >> fee >> salary >> years;
      career.ApplyAction(CareerAction::Transfer(playerId, fee, salary, years), &error);
    } else if (command == "sell") {
      int playerId = 0;
      long long fee = 0;
      stream >> playerId >> fee;
      career.ApplyAction(CareerAction::Sell(playerId, fee), &error);
    } else if (command == "scout") {
      int playerId = 0, scoutId = 0;
      stream >> playerId >> scoutId;
      career.ApplyAction(CareerAction::Scout(playerId, scoutId), &error);
    } else if (command == "hire_scout") {
      int speed = 0, quality = 0;
      long long salary = 0;
      stream >> speed >> quality >> salary;
      career.ApplyAction(CareerAction::HireScout(speed, quality, salary), &error);
    } else if (command == "sponsor") {
      int index = 0;
      stream >> index;
      career.ApplyAction(CareerAction::ChooseSponsor(index), &error);
    }
    if (!error.empty()) std::cerr << "action '" << command << "' failed: " << error << "\n";
  }
}

} // namespace

int main(int argc, char **argv) {
  uint32_t seed = 12345;
  int seasons = 2;
  bool hashOnly = false;
  bool replayCheck = false;
  std::string expectedHash;
  std::string actionsPath;

  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == "--hash") {
      hashOnly = true;
    } else if (arg == "--replay") {
      replayCheck = true;
    } else if (arg == "--check" && i + 1 < argc) {
      hashOnly = true;
      expectedHash = argv[++i];
    } else if (arg == "--seasons" && i + 1 < argc) {
      seasons = std::atoi(argv[++i]);
    } else if (arg == "--actions" && i + 1 < argc) {
      actionsPath = argv[++i];
    } else if (arg.size() > 0 && arg[0] != '-') {
      seed = static_cast<uint32_t>(std::strtoul(arg.c_str(), nullptr, 10));
    }
  }
  if (seasons < 1) seasons = 1;

  CareerConfig config;
  Date reference(config.seasonStartYear, config.seasonStartMonth, config.seasonStartDay);
  SyntheticCareerDataSource source(seed, config.clubsPerDivision, reference, config.freeAgentCount);
  Career career(seed, config, source);

  if (!actionsPath.empty()) RunScript(career, actionsPath);
  else RunAuto(career, seasons);

  std::string hash = career.Hash();
  if (hashOnly) {
    if (!expectedHash.empty()) {
      if (hash == expectedHash) {
        std::cout << "OK " << hash << "\n";
        return 0;
      }
      std::cout << "FAIL got=" << hash << " expected=" << expectedHash << "\n";
      return 1;
    }
    std::cout << hash << "\n";
    return 0;
  }

  if (replayCheck) {
    SyntheticCareerDataSource replaySource(seed, config.clubsPerDivision, reference, config.freeAgentCount);
    Career replayed = Career::Replay(seed, config, replaySource, career.Journal(), career.RecordedOutcomes());
    if (replayed.Hash() == hash && replayed.Transcript() == career.Transcript()) {
      std::cout << "REPLAY OK " << hash << "\n";
    } else {
      std::cout << "INVARIANT FAILED: replay transcript/hash mismatch\n";
      std::cout << "original=" << hash << " replayed=" << replayed.Hash() << "\n";
      return 1;
    }
  }

  std::cout << career.Transcript();
  std::cout << "HASH " << hash << "\n";
  return 0;
}

