// written by bastiaan konings schuiling 2008 - 2015
// this work is public domain. the code is undocumented, scruffy, untested, and should generally not be used for anything important.
// i do not offer support, so don't ask. to be used for inspiration :)

#ifndef _HPP_MENU_PREMATCH_CAPTAIN
#define _HPP_MENU_PREMATCH_CAPTAIN

#include "defines.hpp"

#include "scene/scene3d/node.hpp"
#include "scene/objects/geometry.hpp"

class PlayerData;
class TeamData;

using namespace blunted;

// A static, kit-dressed fullbody model for the pre-match hub. Built as a private
// deep copy of fullbody.object so its vertices/materials never touch the match
// mesh cache, posed into a calm idle frame (ported from HumanoidBase's bake) and
// rendered by the menu scene's own camera on one side of the hub content.
//
// Scene construction/destruction must run on the game thread under
// GraphicsSystem::getPhaseMutex (structural scene change), like Match does for
// substitutions.
class PreMatchCaptainPreview {

  public:
    PreMatchCaptainPreview(TeamData *teamData, PlayerData *playerData, int kitNumber, bool leftSide);
    ~PreMatchCaptainPreview();

    // Repaint the kit texture only (cheap, no re-skinning).
    void SetKit(int kitNumber);

  protected:
    void Create();
    void Destroy();
    void BakePose();
    void ApplyMaterials();
    boost::intrusive_ptr<Geometry> GetFullbodyGeometry();
    boost::intrusive_ptr < Resource<Surface> > FetchKitResource(int kit);

    TeamData *teamData;
    PlayerData *playerData;
    int kitNumber;
    bool leftSide;

    boost::intrusive_ptr<Node> rootNode;
    boost::intrusive_ptr<Geometry> fullbodyGeometry;
    boost::intrusive_ptr<Node> rigNode; // detached joint hierarchy, never rendered
    boost::intrusive_ptr<Geometry> hairNode;

    boost::intrusive_ptr < Resource<Surface> > kitResource;
    boost::intrusive_ptr < Resource<Surface> > skinResource;
    boost::intrusive_ptr < Resource<Surface> > hairResource;
    std::string currentKitIdent;

    float previewHeight;
    float previewDistance;
    float previewSideOffset;
    float meshScale;    // applied to the baked vertices to hit previewHeight
    Vector3 meshPivot;  // removed before scaling so feet sit on the origin
    Vector3 headLocalPosition;  // neck joint in normalized local space (for the hair)
    Quaternion headLocalRotation;

};

#endif
