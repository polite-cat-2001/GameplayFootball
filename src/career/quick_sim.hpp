// Headless zone-based quick simulation (wave 6 decisions). It reuses only the
// career condition contract and OVR values: no Match, no ElizaController, no
// physics. Every random decision is draw from the career RNG.

#ifndef _HPP_CAREER_QUICK_SIM
#define _HPP_CAREER_QUICK_SIM

#include "career_types.hpp"
#include "career_rng.hpp"

namespace blunted {
namespace career {

struct QuickSimPlayer {
  int id = 0;
  Position position = e_Position_CM;
  int ovr = 50;
  int fatigue = 100;
};

struct QuickSimTeam {
  int clubId = -1;
  std::string formation = "4-4-2";
  std::vector<QuickSimPlayer> startingXi;
  std::vector<QuickSimPlayer> bench;
};

struct QuickSimResult {
  MatchOutcome outcome;
  int homeChances = 0;
  int awayChances = 0;
};

class QuickSim {
  public:
    // Simulates a normal fixture. The caller handles the walkover case.
    static QuickSimResult Play(int fixtureId, CompetitionType competition, const QuickSimTeam &home,
                               const QuickSimTeam &away, const CareerConfig &config, CareerRng &rng);

    // Deterministic shootout only (used by the cup path and by tests).
    static void Shootout(const QuickSimTeam &home, const QuickSimTeam &away, CareerRng &rng,
                         int &homePenalties, int &awayPenalties);
};

} // namespace career
} // namespace blunted

#endif
