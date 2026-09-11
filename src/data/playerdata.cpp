// written by bastiaan konings schuiling 2008 - 2015
// this work is public domain. the code is undocumented, scruffy, untested, and should generally not be used for anything important.
// i do not offer support, so don't ask. to be used for inspiration :)

#include "playerdata.hpp"

#include "utils/database.hpp"

#include "base/utils.hpp"

#include "../main.hpp"

PlayerData::PlayerData(int playerDatabaseID) : databaseID(playerDatabaseID) {

  DatabaseResult *result = GetDB()->Query("select firstname, lastname, role, base_stat, profile_xml, age, skincolor, hairstyle, haircolor, height from players where id = " + int_to_str(databaseID) + " limit 1");

  raw.databaseID = playerDatabaseID;
  raw.baseStat = 0.0f;
  raw.age = 15;
  raw.skinColor = int(round(random(1, 4)));
  raw.hairStyle = "short01";
  raw.hairColor = "darkblonde";
  raw.height = 1.8f;

  for (unsigned int c = 0; c < result->data.at(0).size(); c++) {
    if (result->header.at(c).compare("firstname") == 0) raw.firstName = result->data.at(0).at(c);
    if (result->header.at(c).compare("lastname") == 0) raw.lastName = result->data.at(0).at(c);
    if (result->header.at(c).compare("role") == 0) raw.roleString = result->data.at(0).at(c);
    if (result->header.at(c).compare("base_stat") == 0) raw.baseStat = atof(result->data.at(0).at(c).c_str());
    if (result->header.at(c).compare("profile_xml") == 0) raw.profileXml = result->data.at(0).at(c);
    if (result->header.at(c).compare("age") == 0) raw.age = atoi(result->data.at(0).at(c).c_str());
    if (result->header.at(c).compare("skincolor") == 0) raw.skinColor = atoi(result->data.at(0).at(c).c_str());
    if (result->header.at(c).compare("hairstyle") == 0) raw.hairStyle = result->data.at(0).at(c);
    if (result->header.at(c).compare("haircolor") == 0) raw.hairColor = result->data.at(0).at(c);
    if (result->header.at(c).compare("height") == 0) {
      float parsedHeight = atof(result->data.at(0).at(c).c_str());
      // NULL/empty or implausible values (scraper artifacts) would give
      // zMultiplier == 0 and collapse the fullbody model: invisible body with
      // the hairstyle lying flat on the pitch. Keep the default instead.
      if (parsedHeight >= 1.4f && parsedHeight <= 2.3f) raw.height = parsedHeight;
    }
  }

  delete result;

  Init();
}

PlayerData::PlayerData(const PlayerDataRaw &source) : databaseID(source.databaseID), raw(source) {
  Init();
}

void PlayerData::Init() {

  roles.clear();

  std::vector<std::string> roleStrings;
  tokenize(raw.roleString, roleStrings);

  for (int i = 0; i < (signed int)roleStrings.size(); i++) {
    roles.push_back(GetRoleFromString(roleStrings.at(i)));
  }


  // get average stat for current age

  XMLLoader loader;
  XMLTree tree = loader.Load(raw.profileXml);

  //printf("player: %s, %s (age %i)\n", lastName.c_str(), firstName.c_str(), age);
  map_XMLTree::const_iterator iter = tree.children.begin();
  while (iter != tree.children.end()) {
    float profileStat = atof((*iter).second.value.c_str()); // profile value

    float value = CalculateStat(raw.baseStat, profileStat, raw.age, e_DevelopmentCurveType_Normal);
    //printf("base: %f; profile: %f; result: %f\n", baseStat, profileStat, value);

    stats.Set((*iter).first.c_str(), value);
    iter++;
  }

}

PlayerData::PlayerData() {
  raw.skinColor = int(round(random(1, 4)));
  raw.hairStyle = "short01";
  raw.hairColor = "darkblonde";
  raw.height = 1.8f;

  stats.Set("physical_balance", 0.6);
  stats.Set("physical_reaction", 0.6);
  stats.Set("physical_acceleration", 0.6);
  stats.Set("physical_velocity", 0.6);
  stats.Set("physical_stamina", 0.6);
  stats.Set("physical_agility", 0.6);
  stats.Set("physical_shotpower", 0.6);
  stats.Set("technical_standingtackle", 0.6);
  stats.Set("technical_slidingtackle", 0.6);
  stats.Set("technical_ballcontrol", 0.6);
  stats.Set("technical_dribble", 0.6);
  stats.Set("technical_shortpass", 0.6);
  stats.Set("technical_highpass", 0.6);
  stats.Set("technical_header", 0.6);
  stats.Set("technical_shot", 0.6);
  stats.Set("technical_volley", 0.6);
  stats.Set("mental_calmness", 0.6);
  stats.Set("mental_workrate", 0.6);
  stats.Set("mental_resilience", 0.6);
  stats.Set("mental_defensivepositioning", 0.6);
  stats.Set("mental_offensivepositioning", 0.6);
  stats.Set("mental_vision", 0.6);
}

PlayerData::~PlayerData() {
}

const std::vector<e_PlayerRole> &PlayerData::GetRoles() const {
  return roles;
}

float PlayerData::GetStat(const char *name) {
  bool exists = stats.Exists(name);
  if (!exists) printf("Stat named '%s' does not exist!\n", name);
  assert(exists);
  return stats.GetReal(name, 1.0f);
}
