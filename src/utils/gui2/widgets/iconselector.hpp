// written by bastiaan konings schuiling 2008 - 2014
// this work is public domain. the code is undocumented, scruffy, untested, and should generally not be used for anything important.
// i do not offer support, so don't ask. to be used for inspiration :)

#ifndef _HPP_GUI2_VIEW_ICONSELECTOR
#define _HPP_GUI2_VIEW_ICONSELECTOR

#include "SDL3_ttf/SDL_ttf.h"

#include "../view.hpp"

#include "scene/objects/image2d.hpp"

#include "image.hpp"
#include "caption.hpp"

namespace blunted {

  struct Gui2IconSelectorEntry {
    std::string caption;
    std::string id;
    std::string imageFile;
  };

  class Gui2IconSelector : public Gui2View {

    public:
      Gui2IconSelector(Gui2WindowManager *windowManager, const std::string &name, float x_percent, float y_percent, float width_percent, float height_percent, const std::string &caption);
      virtual ~Gui2IconSelector();

      virtual void GetImages(std::vector < boost::intrusive_ptr<Image2D> > &target);

      virtual void Process();
      virtual void Redraw();

      std::string GetSelectedEntryID() { if (entries.size() > 0) return entries.at(selectedEntry).id; else return ""; }
      void SetSelectedEntry(int index);
      int FindEntryIndex(const std::string &id) const;
      void ClearEntries();
      void AddEntry(const std::string &id, const std::string &caption, const std::string &imageFile);

      // pool icons get a white outline around the logo shape (for dark logos)
      void SetDrawOutline(bool drawOutline) { this->drawOutline = drawOutline; }

      virtual void ProcessWindowingEvent(WindowingEvent *event);

      virtual void OnGainFocus();
      virtual void OnLoseFocus();

      boost::signals2::signal<void()> sig_OnClick;
      boost::signals2::signal<void()> sig_OnChange;

    protected:
      boost::intrusive_ptr<Image2D> image;

      std::string caption;

      std::vector<Gui2IconSelectorEntry> entries;
      Gui2Caption *selectedCaption;

      // small pool of icon widgets reused for the few entries currently inside
      // the visible carousel range, so populating a large selector (hundreds of
      // team/country icons) does not allocate a texture per entry
      std::vector<Gui2Image*> iconPool;
      std::vector<int> iconEntryIndex; // entry index each pool icon shows (-1 = free)

      void EnsureIconPool();
      void DeleteIconPool();

      int selectedEntry;
      float visibleSelectedEntry;

      // single step animates ~300ms (exponential ease toward the target).
      // Auto-repeat: a tap steps once immediately; holding must last
      // scrollHoldDelay_ms before it starts repeating, then steps every
      // scrollRepeatDelay_ms while held. The repeat timer only counts while a
      // direction is continuously held, so a noisy analog stick near the
      // deadzone can't bunch up steps into skips.
      float scrollAnimTime_ms = 300.0f;
      int scrollHoldDelay_ms = 250;
      int scrollRepeatDelay_ms = 60;
      int scrollRepeatAccum_ms = 250;
      bool scrollHeld = false;
      int scrollHeldX = 0;
      bool scrollDirEventThisFrame = false;

      int fadeOut_ms;
      int fadeOutTime_ms;

      bool drawOutline = false;

  };

}

#endif
