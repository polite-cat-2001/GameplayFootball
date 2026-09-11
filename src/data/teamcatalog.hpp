#ifndef _HPP_TEAMCATALOG
#define _HPP_TEAMCATALOG

#include "defines.hpp"

#include <string>
#include <vector>

// A lightweight team row for the LAN lobby: enough to render a pick list, not
// enough to build a match (that is TeamDataRaw via netdata).
struct TeamCatalogEntry {
  int id = 0;
  std::string name;
  std::string shortName;
  std::string logoUrl;
};

// Paged, optionally filtered by name. Runs on the host (uses its database).
std::vector<TeamCatalogEntry> QueryTeamCatalog(int offset, int limit, const std::string &search);

#endif
