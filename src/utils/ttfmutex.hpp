// This work is public domain. The code is undocumented, scruffy, untested, and
// should generally not be used for anything important.

#ifndef _HPP_UTILS_TTFMUTEX
#define _HPP_UTILS_TTFMUTEX

#include <boost/thread/mutex.hpp>

namespace blunted {

  // SDL_ttf is not thread-safe per font. Menus render text in the game thread
  // while the scoreboard renders in the graphics thread (Match::Put ->
  // Gui2Caption::SetCaption), both with the same TTF_Font. Every call that can
  // touch a font's glyph cache must be serialized through this mutex.
  inline boost::mutex &GetTTFMutex() {
    static boost::mutex mutex;
    return mutex;
  }

}

#endif
