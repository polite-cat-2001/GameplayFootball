// Private deterministic RNG for the career module. All career decisions pull
// from this stream (never from blunted::random() or the global time), so the
// same seed reproduces the same career.

#ifndef _HPP_CAREER_RNG
#define _HPP_CAREER_RNG

#include <cstdint>
#include <vector>

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include <boost/random/uniform_real_distribution.hpp>

namespace blunted {
namespace career {

class CareerRng {
  public:
    explicit CareerRng(uint32_t seed = 0) : engine(seed) {}

    void Seed(uint32_t seed) { engine.seed(seed); }

    uint32_t Next() { return engine(); }

    int Int(int minValue, int maxValue) {
      if (maxValue <= minValue) return minValue;
      boost::random::uniform_int_distribution<int> distribution(minValue, maxValue);
      return distribution(engine);
    }

    // Returns [0.0, 1.0).
    double Real() {
      boost::random::uniform_real_distribution<double> distribution(0.0, 1.0);
      return distribution(engine);
    }

    bool Chance(float probability) {
      if (probability <= 0.0f) return false;
      if (probability >= 1.0f) return true;
      return Real() < static_cast<double>(probability);
    }

    template <typename T>
    void Shuffle(std::vector<T> &values) {
      for (int i = static_cast<int>(values.size()) - 1; i > 0; i--) {
        int j = Int(0, i);
        std::swap(values[i], values[j]);
      }
    }

  private:
    boost::random::mt19937 engine;
};

} // namespace career
} // namespace blunted

#endif
