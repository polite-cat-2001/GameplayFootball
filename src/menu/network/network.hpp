#ifndef _HPP_MENU_NETWORK
#define _HPP_MENU_NETWORK

#include "utils/gui2/windowmanager.hpp"

#include "utils/gui2/widgets/grid.hpp"
#include "utils/gui2/widgets/button.hpp"
#include "utils/gui2/widgets/caption.hpp"
#include "utils/gui2/widgets/editline.hpp"

#include "utils/gui2/page.hpp"

using namespace blunted;

class NetworkMenuPage : public Gui2Page {

  public:
    NetworkMenuPage(Gui2WindowManager *windowManager, const Gui2PageData &pageData);
    virtual ~NetworkMenuPage();

    void GoHost();
    void GoJoin();
    void GoBack();

  protected:
    std::vector<Gui2Button*> buttons;

};

class NetworkHostPage : public Gui2Page {

  public:
    NetworkHostPage(Gui2WindowManager *windowManager, const Gui2PageData &pageData);
    virtual ~NetworkHostPage();

    void OpenLobby();
    void GoBack();

  protected:
    Gui2EditLine *portInput;
    Gui2EditLine *nameInput;
    Gui2Caption *statusCaption;

};

class NetworkJoinPage : public Gui2Page {

  public:
    NetworkJoinPage(Gui2WindowManager *windowManager, const Gui2PageData &pageData);
    virtual ~NetworkJoinPage();

    void Connect();
    void GoBack();

    virtual void Process();

  protected:
    Gui2EditLine *addressInput;
    Gui2EditLine *portInput;
    Gui2EditLine *nameInput;
    Gui2Caption *statusCaption;
    bool connecting;

};

#endif
