// written by bastiaan konings schuiling 2008 - 2015
// this work is public domain. the code is undocumented, scruffy, untested, and should generally not be used for anything important.
// i do not offer support, so don't ask. to be used for inspiration :)

#ifndef _HPP_TEAMDATA
#define _HPP_TEAMDATA

#include "defines.hpp"
#include "base/properties.hpp"

#include "../gamedefines.hpp"
#include "playerdata.hpp"

struct TeamTactics {

  TeamTactics() {
  }

  Properties factoryProperties;
  Properties userProperties;

  Properties humanReadableNames;
  Properties descriptions;

};

// Serializable team payload streamed from host to clients. XML strings are kept
// raw so the receiving side can rebuild formation/tactics without a database.
struct TeamDataRaw {
  int databaseID = 0;
  std::string name;
  std::string shortName;
  std::string logoUrl;
  std::string kitUrl;
  std::string formationXml;
  std::string formationFactoryXml;
  std::string tacticsXml;
  std::string tacticsFactoryXml;
  Vector3 color1;
  Vector3 color2;
  std::vector<PlayerDataRaw> players;
};

class TeamData {

  public:
    TeamData(int teamDatabaseID);
    TeamData(const TeamDataRaw &raw);
    virtual ~TeamData();

    std::string GetName() { return name; }
    std::string GetShortName() { return shortName; }
    std::string GetLogoUrl() { return logo_url; }
    std::string GetKitUrl() { return kit_url; }
    Vector3 GetColor1() { return color1; }
    Vector3 GetColor2() { return color2; }

    int GetDatabaseID() const { return databaseID; }

    const TeamTactics &GetTactics() const { return tactics; }
    TeamTactics &GetTacticsWritable() { return tactics; }

    FormationEntry GetFormationEntry(int num);
    void SetFormationEntry(int num, FormationEntry entry);

    void SwitchPlayers(int databaseID1, int databaseID2);

    // vector index# is entry in formation[index#]
    const std::vector<PlayerData*> &GetPlayerData() { return playerData; }
    int GetPlayerNum() { return playerData.size(); }
    PlayerData *GetPlayerData(int num) { return playerData.at(num); }
    PlayerData *GetPlayerDataByDatabaseID(int id);

    const TeamDataRaw &GetRaw() const { return raw; }

    void SaveLineup();
    void SaveTactics();
    void Save();

  protected:
    void InitFromRaw();

    int databaseID;

    std::string name;
    std::string shortName;
    std::string logo_url;
    std::string kit_url;
    Vector3 color1, color2;

    TeamTactics tactics;

    FormationEntry formation[playerNum];

    std::vector<PlayerData*> playerData;

    TeamDataRaw raw;

};

#endif
