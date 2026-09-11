// written by bastiaan konings schuiling 2008 - 2015
// this work is public domain. the code is undocumented, scruffy, untested, and should generally not be used for anything important.
// i do not offer support, so don't ask. to be used for inspiration :)

#ifndef _HPP_PLAYERDATA
#define _HPP_PLAYERDATA

#include "defines.hpp"

#include "../gamedefines.hpp"
#include "../utils.hpp"

#include "base/properties.hpp"

// Raw database fields of a player. This is the serializable representation used
// to stream a match setup from the host to clients (no database needed client-side).
struct PlayerDataRaw {
  int databaseID = 0;
  std::string firstName;
  std::string lastName;
  std::string roleString;
  std::string profileXml;
  float baseStat = 0.0f;
  int age = 15;
  int skinColor = 1;
  std::string hairStyle = "short01";
  std::string hairColor = "darkblonde";
  float height = 1.8f;
};

class PlayerData {

  public:
    PlayerData(int playerDatabaseID);
    PlayerData(const PlayerDataRaw &raw);
    PlayerData();
    virtual ~PlayerData();

    std::string GetFirstName() const { return raw.firstName; }
    // Some players (Transfermarkt data) have no surname and are known by a single
    // name; fall back to it so captions/menus never show an empty string.
    std::string GetLastName() const { return raw.lastName.empty() ? raw.firstName : raw.lastName; }
    int GetDatabaseID() const { return databaseID; }
    const std::vector<e_PlayerRole> &GetRoles() const;

    float GetStat(const char *name);

    int GetSkinColor() { return raw.skinColor; }
    std::string GetHairStyle() { return raw.hairStyle; }
    std::string GetHairColor() { return raw.hairColor; }
    float GetHeight() { return raw.height; }

    const PlayerDataRaw &GetRaw() const { return raw; }

  protected:
    void Init();

    int databaseID;
    PlayerDataRaw raw;
    std::vector<e_PlayerRole> roles;

    Properties stats;

};

#endif
