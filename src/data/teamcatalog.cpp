#include "teamcatalog.hpp"

#include "utils/database.hpp"

#include "base/utils.hpp"

#include "../main.hpp"

static std::string EscapeSqlLiteral(const std::string &value) {
  std::string escaped;
  escaped.reserve(value.size());
  for (unsigned int i = 0; i < value.size(); i++) {
    if (value.at(i) == '\'') escaped += "''";
    else escaped += value.at(i);
  }
  return escaped;
}

std::vector<TeamCatalogEntry> QueryTeamCatalog(int offset, int limit, const std::string &search) {
  std::vector<TeamCatalogEntry> catalog;

  std::string query = "select id, name, coalesce(shortname, ''), coalesce(logo_url, '') from teams";
  if (!search.empty()) {
    query += " where name like '%" + EscapeSqlLiteral(search) + "%'";
  }
  query += " order by name limit " + int_to_str(limit) + " offset " + int_to_str(offset);

  DatabaseResult *result = GetDB()->Query(query);
  for (unsigned int r = 0; r < result->data.size(); r++) {
    TeamCatalogEntry entry;
    entry.id = atoi(result->data.at(r).at(0).c_str());
    entry.name = result->data.at(r).at(1);
    entry.shortName = result->data.at(r).at(2);
    entry.logoUrl = result->data.at(r).at(3);
    if (!entry.logoUrl.empty()) entry.logoUrl = "databases/default/" + entry.logoUrl;
    catalog.push_back(entry);
  }
  delete result;

  return catalog;
}
