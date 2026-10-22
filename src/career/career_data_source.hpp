// Career data adapter. The game wraps GetDB(); the headless runner uses the
// synthetic in-memory source below. The career reads the source once at
// construction and never touches the foundation database.

#ifndef _HPP_CAREER_DATA_SOURCE
#define _HPP_CAREER_DATA_SOURCE

#include "career_types.hpp"
#include "career_rng.hpp"

namespace blunted {
namespace career {

class CareerDataSource {
  public:
    virtual ~CareerDataSource() {}
    virtual std::vector<ClubSeed> LoadClubs() = 0;
    virtual std::vector<PlayerSeed> LoadFreeAgents() = 0;
};

// Two synthetic divisions with generated squads. No data/ directory, no sqlite,
// no GetDB(): the whole career world comes from this seed-derived snapshot.
class SyntheticCareerDataSource : public CareerDataSource {
  public:
    SyntheticCareerDataSource(uint32_t seed, int clubsPerDivision, const Date &referenceDate,
                              int freeAgentCount = 16)
        : rng(seed ^ 0x9e3779b9u),
          clubsPerDivision(clubsPerDivision),
          freeAgentCount(freeAgentCount),
          referenceDate(referenceDate) {
      Build();
    }

    std::vector<ClubSeed> LoadClubs() override { return clubs; }
    std::vector<PlayerSeed> LoadFreeAgents() override { return freeAgents; }

  private:
    static std::vector<Position> TemplatePositions() {
      std::vector<Position> positions;
      positions.push_back(e_Position_GK);
      positions.push_back(e_Position_GK);
      positions.push_back(e_Position_CB);
      positions.push_back(e_Position_CB);
      positions.push_back(e_Position_CB);
      positions.push_back(e_Position_LB);
      positions.push_back(e_Position_RB);
      positions.push_back(e_Position_DM);
      positions.push_back(e_Position_CM);
      positions.push_back(e_Position_CM);
      positions.push_back(e_Position_LM);
      positions.push_back(e_Position_RM);
      positions.push_back(e_Position_AM);
      positions.push_back(e_Position_LW);
      positions.push_back(e_Position_RW);
      positions.push_back(e_Position_CF);
      positions.push_back(e_Position_CF);
      positions.push_back(e_Position_ST);
      positions.push_back(e_Position_ST);
      positions.push_back(e_Position_CM);
      positions.push_back(e_Position_CB);
      positions.push_back(e_Position_DM);
      return positions;
    }

    void Build() {
      const std::vector<Position> templatePositions = TemplatePositions();
      int nextPlayerId = 1;

      for (int division = 1; division <= 2; division++) {
        for (int index = 0; index < clubsPerDivision; index++) {
          ClubSeed club;
          club.id = static_cast<int>(clubs.size()) + 1;
          club.division = division;
          char name[64];
          std::snprintf(name, sizeof(name), "%s Club %02d", division == 1 ? "Premier" : "Second", index + 1);
          club.name = name;
          std::snprintf(name, sizeof(name), "%s%02d", division == 1 ? "P" : "S", index + 1);
          club.shortName = name;
          club.reputation = division == 1 ? 55 + (clubsPerDivision - index) * 3 + rng.Int(0, 4)
                                          : 32 + (clubsPerDivision - index) * 2 + rng.Int(0, 4);
          if (club.reputation > 88) club.reputation = 88;

          for (size_t p = 0; p < templatePositions.size(); p++) {
            PlayerSeed player = MakePlayer(nextPlayerId++, templatePositions[p], club.reputation,
                                           division == 1 ? 0.0f : -0.06f);
            player.clubId = club.id;
            club.players.push_back(player);
          }
          clubs.push_back(club);
        }
      }

      for (int i = 0; i < freeAgentCount; i++) {
        Position position = templatePositions[(i * 7) % templatePositions.size()];
        PlayerSeed player = MakePlayer(nextPlayerId++, position, 40 + rng.Int(0, 20), -0.04f);
        player.clubId = -1;
        freeAgents.push_back(player);
      }
    }

    PlayerSeed MakePlayer(int id, Position position, int clubReputation, real strengthOffset) {
      PlayerSeed player;
      player.id = id;
      char name[48];
      std::snprintf(name, sizeof(name), "Player %04d", id);
      player.name = name;
      player.position = position;

      int age = 17 + rng.Int(0, 17);
      if (rng.Chance(0.25f)) age = 17 + rng.Int(0, 4);
      if (rng.Chance(0.18f)) age = 30 + rng.Int(0, 6);
      if (age < 16) age = 16;
      if (age > 37) age = 37;
      player.birthDate = Date(referenceDate.year - age, rng.Int(1, 12), rng.Int(1, 28));

      real reputationBase = 0.34f + float(clubReputation) / 100.0f * 0.36f;
      real variance = (float(rng.Int(-60, 60)) / 1000.0f);
      real baseStat = reputationBase + strengthOffset + variance;
      if (position == e_Position_GK) baseStat -= 0.015f;
      player.baseStat = clampValue(baseStat, 0.22f, 0.86f);

      int ageAtReference = player.birthDate.AgeOn(referenceDate);
      int currentOvr = CalculateOvr(player.baseStat, ageAtReference);
      int potentialBonus = ageAtReference <= 21 ? rng.Int(4, 16) : (ageAtReference <= 26 ? rng.Int(1, 8) : rng.Int(0, 3));
      int baseOvr = static_cast<int>(std::lround(player.baseStat * 120.0f));
      player.potentialOvr = std::min(100, std::max(currentOvr + potentialBonus, std::min(100, baseOvr + 2)));
      player.marketValue = MarketValueForOvr(currentOvr);
      return player;
    }

    CareerRng rng;
    int clubsPerDivision;
    int freeAgentCount;
    Date referenceDate;
    std::vector<ClubSeed> clubs;
    std::vector<PlayerSeed> freeAgents;
};

} // namespace career
} // namespace blunted

#endif
