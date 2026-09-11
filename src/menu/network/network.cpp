#include "network.hpp"

#include "networklobby.hpp"

#include "../pagefactory.hpp"

#include "../../main.hpp"

#include "utils/gui2/events.hpp"

#include <boost/make_shared.hpp>

#include <cctype>

#include "data/teamcatalog.hpp"
#include "net/netclient.hpp"
#include "net/netmessages.hpp"
#include "net/netserver.hpp"

namespace {
// Small address check for the join field: either dotted-quad IPv4 or a DNS
// hostname. No regex (heavy, constructs per call); this is a one-off at submit.
bool IsValidIPv4(const std::string &s) {
  int parts = 0;
  size_t i = 0;
  while (i < s.size()) {
    int value = 0;
    int digits = 0;
    while (i < s.size() && std::isdigit((unsigned char)s[i])) {
      value = value * 10 + (s[i] - '0');
      if (++digits > 3) return false;
      i++;
    }
    if (digits == 0 || value > 255) return false;
    parts++;
    if (i < s.size()) {
      if (s[i] != '.') return false;
      i++;
      if (i == s.size()) return false; // trailing dot
    }
  }
  return parts == 4;
}

bool IsValidHostname(const std::string &s) {
  if (s.empty() || s.size() > 253) return false;
  size_t labelStart = 0;
  for (size_t i = 0; i <= s.size(); i++) {
    if (i == s.size() || s[i] == '.') {
      size_t len = i - labelStart;
      if (len == 0 || len > 63) return false;
      if (s[labelStart] == '-' || s.at(i - 1) == '-') return false;
      for (size_t j = labelStart; j < i; j++) {
        char c = s[j];
        if (!(std::isalnum((unsigned char)c) || c == '-')) return false;
      }
      labelStart = i + 1;
    }
  }
  return true;
}

bool IsValidAddress(const std::string &s) {
  if (s.empty()) return false;
  bool onlyDigitsAndDots = true;
  for (size_t i = 0; i < s.size(); i++) {
    if (!(std::isdigit((unsigned char)s[i]) || s[i] == '.')) { onlyDigitsAndDots = false; break; }
  }
  // All-digit labels are syntactically a hostname, but here it should be an IP.
  if (onlyDigitsAndDots) return IsValidIPv4(s);
  return IsValidHostname(s);
}
}

NetworkMenuPage::NetworkMenuPage(Gui2WindowManager *windowManager, const Gui2PageData &pageData) : Gui2Page(windowManager, pageData) {

  Gui2Caption *title = new Gui2Caption(windowManager, "caption_network", 20, 20, 60, 3, "Network");
  this->AddView(title);
  title->Show();

  Gui2Button *hostButton = new Gui2Button(windowManager, "button_network_host", 0, 0, 30, 3, "Host game");
  Gui2Button *joinButton = new Gui2Button(windowManager, "button_network_join", 0, 0, 30, 3, "Join game");
  Gui2Button *backButton = new Gui2Button(windowManager, "button_network_back", 0, 0, 30, 3, "Back");

  hostButton->sig_OnClick.connect(boost::bind(&NetworkMenuPage::GoHost, this));
  joinButton->sig_OnClick.connect(boost::bind(&NetworkMenuPage::GoJoin, this));
  backButton->sig_OnClick.connect(boost::bind(&NetworkMenuPage::GoBack, this));

  Gui2Grid *grid = new Gui2Grid(windowManager, "grid_network", 20, 25, 60, 55);
  grid->AddView(hostButton, 0, 0);
  grid->AddView(joinButton, 1, 0);
  grid->AddView(backButton, 2, 0);
  grid->UpdateLayout(0.5);

  this->AddView(grid);
  grid->Show();

  hostButton->SetFocus();
  this->Show();
}

NetworkMenuPage::~NetworkMenuPage() {
}

void NetworkMenuPage::GoHost() {
  CreatePage(e_PageID_NetworkHost);
}

void NetworkMenuPage::GoJoin() {
  CreatePage(e_PageID_NetworkJoin);
}

void NetworkMenuPage::GoBack() {
  CreatePage(e_PageID_MainMenu);
}


NetworkHostPage::NetworkHostPage(Gui2WindowManager *windowManager, const Gui2PageData &pageData) : Gui2Page(windowManager, pageData) {

  Gui2Caption *title = new Gui2Caption(windowManager, "caption_network_host", 20, 12, 60, 3, "Host game");
  this->AddView(title);
  title->Show();

  Gui2Caption *portLabel = new Gui2Caption(windowManager, "caption_network_host_port", 0, 0, 30, 3, "Port");
  portInput = new Gui2EditLine(windowManager, "editline_network_host_port", 0, 0, 20, 3, "27015");
  portInput->SetAllowedChars("0123456789");
  portInput->SetMaxLength(5);

  Gui2Caption *nameLabel = new Gui2Caption(windowManager, "caption_network_host_name", 0, 0, 30, 3, "Name");
  nameInput = new Gui2EditLine(windowManager, "editline_network_host_name", 0, 0, 20, 3, "Host");
  nameInput->SetAllowedChars("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_ ");
  nameInput->SetMaxLength(16);

  Gui2Button *openButton = new Gui2Button(windowManager, "button_network_host_open", 0, 0, 30, 3, "Open lobby");
  openButton->sig_OnClick.connect(boost::bind(&NetworkHostPage::OpenLobby, this));

  Gui2Button *backButton = new Gui2Button(windowManager, "button_network_host_back", 0, 0, 30, 3, "Back");
  backButton->sig_OnClick.connect(boost::bind(&NetworkHostPage::GoBack, this));

  Gui2Grid *grid = new Gui2Grid(windowManager, "grid_network_host", 20, 22, 60, 45);
  grid->AddView(portLabel, 0, 0);
  grid->AddView(portInput, 0, 1);
  grid->AddView(nameLabel, 1, 0);
  grid->AddView(nameInput, 1, 1);
  grid->AddView(openButton, 2, 1);
  grid->AddView(backButton, 2, 0);
  grid->UpdateLayout(0.5);

  this->AddView(grid);
  grid->Show();

  statusCaption = new Gui2Caption(windowManager, "caption_network_host_status", 20, 72, 60, 3, "");
  this->AddView(statusCaption);
  statusCaption->Show();

  portInput->SetFocus();
  this->Show();
}

NetworkHostPage::~NetworkHostPage() {
}

void NetworkHostPage::OpenLobby() {
  int port = atoi(portInput->GetText().c_str());
  if (port <= 0 || port > 65535) {
    statusCaption->SetCaption("Invalid port");
    return;
  }
  std::string name = nameInput->GetText();
  if (name.empty()) {
    statusCaption->SetCaption("Enter a name");
    return;
  }

  boost::shared_ptr<NetServer> server = boost::make_shared<NetServer>((uint16_t)port);
  if (!server->Start()) {
    statusCaption->SetCaption("Could not bind port " + portInput->GetText());
    return;
  }

  server->SetHostName(name);

  std::vector<TeamCatalogEntry> source = QueryTeamCatalog(0, 100000, "");
  std::vector<NetCatalogEntry> catalog;
  catalog.reserve(source.size());
  for (unsigned int i = 0; i < source.size(); i++) {
    NetCatalogEntry entry;
    entry.id = source.at(i).id;
    entry.name = source.at(i).name;
    entry.shortName = source.at(i).shortName;
    catalog.push_back(entry);
  }
  server->SetCatalog(catalog);

  GetMenuTask()->SetNetClient(boost::shared_ptr<NetClient>());
  GetMenuTask()->SetNetServer(server);

  CreatePage(e_PageID_SideSelect);
}

void NetworkHostPage::GoBack() {
  CreatePage(e_PageID_NetworkMenu);
}


NetworkJoinPage::NetworkJoinPage(Gui2WindowManager *windowManager, const Gui2PageData &pageData) : Gui2Page(windowManager, pageData) {

  connecting = false;

  Gui2Caption *title = new Gui2Caption(windowManager, "caption_network_join", 20, 10, 60, 3, "Join game");
  this->AddView(title);
  title->Show();

  Gui2Caption *addressLabel = new Gui2Caption(windowManager, "caption_network_join_address", 0, 0, 30, 3, "Address");
  addressInput = new Gui2EditLine(windowManager, "editline_network_join_address", 0, 0, 20, 3, "127.0.0.1");
  addressInput->SetAllowedChars("0123456789.abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-");
  addressInput->SetMaxLength(64);

  Gui2Caption *portLabel = new Gui2Caption(windowManager, "caption_network_join_port", 0, 0, 30, 3, "Port");
  portInput = new Gui2EditLine(windowManager, "editline_network_join_port", 0, 0, 20, 3, "27015");
  portInput->SetAllowedChars("0123456789");
  portInput->SetMaxLength(5);

  Gui2Caption *nameLabel = new Gui2Caption(windowManager, "caption_network_join_name", 0, 0, 30, 3, "Name");
  nameInput = new Gui2EditLine(windowManager, "editline_network_join_name", 0, 0, 20, 3, "Player");
  nameInput->SetAllowedChars("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_ ");
  nameInput->SetMaxLength(16);

  Gui2Button *connectButton = new Gui2Button(windowManager, "button_network_join_connect", 0, 0, 30, 3, "Connect");
  connectButton->sig_OnClick.connect(boost::bind(&NetworkJoinPage::Connect, this));

  Gui2Button *backButton = new Gui2Button(windowManager, "button_network_join_back", 0, 0, 30, 3, "Back");
  backButton->sig_OnClick.connect(boost::bind(&NetworkJoinPage::GoBack, this));

  Gui2Grid *grid = new Gui2Grid(windowManager, "grid_network_join", 20, 20, 60, 50);
  grid->AddView(addressLabel, 0, 0);
  grid->AddView(addressInput, 0, 1);
  grid->AddView(portLabel, 1, 0);
  grid->AddView(portInput, 1, 1);
  grid->AddView(nameLabel, 2, 0);
  grid->AddView(nameInput, 2, 1);
  grid->AddView(connectButton, 3, 1);
  grid->AddView(backButton, 3, 0);
  grid->UpdateLayout(0.5);

  this->AddView(grid);
  grid->Show();

  statusCaption = new Gui2Caption(windowManager, "caption_network_join_status", 20, 74, 60, 3, "");
  this->AddView(statusCaption);
  statusCaption->Show();

  addressInput->SetFocus();
  this->Show();
}

NetworkJoinPage::~NetworkJoinPage() {
}

void NetworkJoinPage::Connect() {
  int port = atoi(portInput->GetText().c_str());
  if (port <= 0 || port > 65535) {
    statusCaption->SetCaption("Invalid port");
    return;
  }
  std::string host = addressInput->GetText();
  if (!IsValidAddress(host)) {
    statusCaption->SetCaption("Invalid address");
    return;
  }
  std::string name = nameInput->GetText();
  if (name.empty()) {
    statusCaption->SetCaption("Enter a name");
    return;
  }

  boost::shared_ptr<NetClient> client = boost::make_shared<NetClient>();
  client->SetPlayerName(name);
  client->Connect(NetAddress(host, (uint16_t)port));

  GetMenuTask()->SetNetServer(boost::shared_ptr<NetServer>());
  GetMenuTask()->SetNetClient(client);

  connecting = true;
  statusCaption->SetCaption("Connecting...");
}

void NetworkJoinPage::GoBack() {
  GetMenuTask()->SetNetClient(boost::shared_ptr<NetClient>());
  CreatePage(e_PageID_NetworkMenu);
}

void NetworkJoinPage::Process() {
  Gui2View::Process();

  if (!connecting) return;

  boost::shared_ptr<NetClient> client = GetMenuTask()->GetNetClient();
  if (!client) { connecting = false; return; }

  if (client->GetState() == e_NetConnectionState_Connected) {
    connecting = false;
    CreatePage(e_PageID_SideSelect);
  } else if (client->GetState() == e_NetConnectionState_Disconnected) {
    connecting = false;
    std::string reason = client->GetServerHello().reasonText;
    statusCaption->SetCaption(reason.empty() ? "Connection failed" : reason);
  }
}
