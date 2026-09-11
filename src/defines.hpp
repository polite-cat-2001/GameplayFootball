// written by bastiaan konings schuiling 2008 - 2014
// this work is public domain. the code is undocumented, scruffy, untested, and should generally not be used for anything important.
// i do not offer support, so don't ask. to be used for inspiration :)

#ifndef _HPP_DEFINES
#define _HPP_DEFINES

#ifdef WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
// winsock2.h must precede windows.h, otherwise windows.h pulls the legacy
// winsock.h and boost::asio (or anything else using winsock2) fails with
// "WinSock.h has already been included".
#include <winsock2.h>
#include <windows.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <cstring>

#include <fstream>
#include <cmath>

#include <algorithm>
#include <string>
#include <list>
#include <vector>
#include <map>
#include <deque>

#include <boost/shared_ptr.hpp>
#include <boost/intrusive_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition.hpp>
#include <boost/signals2.hpp>
#include <boost/signals2/slot.hpp>
#include <boost/bind.hpp>

#include "base/log.hpp"

namespace blunted {

  using namespace boost;
  typedef float real;

  void randomize(unsigned int seed);

  // EnvState fatal path. Declared here (not in log.hpp) so the EnvState templates
  // below never depend on include order: log.hpp -> defines.hpp and defines.hpp ->
  // log.hpp form a cycle that hides blunted::Log when log.hpp is included first.
  void EnvStateFatal();

  class Vector3;
  class Quaternion;

}
// Raw-memory state serializer used for determinism snapshots (ported from GRF,
// matching-fields only). Writes objects into a string via memcpy.
// Vector3/Quaternion have virtual destructors (vptr), so their memcpy would
// capture the vtable address, which is stable within one binary but changes
// between rebuilds. They get dedicated overloads that serialize only the data.
class EnvState {
 public:
  EnvState(const std::string &state, bool load) : state(state), load(load) { }

  void process(std::string &value) {
    int s = value.size();
    process(s);
    value.resize(s);
    for (char &c : value) process(c);
  }

  void process(blunted::Vector3 &value);
  void process(blunted::Quaternion &value);

  template<typename T> void process(std::vector<T>& collection) {
    int size = collection.size();
    process(size);
    collection.resize(size);
    for (auto& el : collection) process(el);
  }

  template<typename T> void process(std::list<T>& collection) {
    int size = collection.size();
    process(size);
    collection.resize(size);
    for (auto& el : collection) process(el);
  }

  template<typename T> void process(T& obj) {
    if (load) {
      if (pos + sizeof(T) > state.size()) {
        blunted::EnvStateFatal();
      }
      memcpy(&obj, &state[pos], sizeof(T));
      pos += sizeof(T);
    } else {
      state.resize(pos + sizeof(T));
      memcpy(&state[pos], &obj, sizeof(T));
      pos += sizeof(T);
    }
  }

  std::string GetState() { return state; }
  bool Load() { return load; }

 private:
  std::string state;
  size_t pos = 0;
  bool load;
};

#endif
