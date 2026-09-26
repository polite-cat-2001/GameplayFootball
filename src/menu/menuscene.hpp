// written by bastiaan konings schuiling 2008 - 2015
// this work is public domain. the code is undocumented, scruffy, untested, and should generally not be used for anything important.
// i do not offer support, so don't ask. to be used for inspiration :)

#ifndef _HPP_MENUSCENE
#define _HPP_MENUSCENE

#include "scene/scene3d/node.hpp"
#include "scene/objects/camera.hpp"
#include "scene/objects/light.hpp"
#include "scene/objects/geometry.hpp"

#include "managers/environmentmanager.hpp"

#include <vector>

using namespace blunted;

struct MenuSceneLocation {
  MenuSceneLocation() {
    position = Vector3(0.0f, 0.0f, 1.0f);
    orientation = Quaternion(QUATERNION_IDENTITY);
    timeStamp_ms = EnvironmentManager::GetInstance().GetTime_ms();
  }
  Vector3 position;
  Quaternion orientation;
  unsigned long timeStamp_ms;
};

class MenuScene {

  public:
    MenuScene();
    virtual ~MenuScene();

    void Get();
    void Process();
    void Put();

    void RandomizeTargetLocation();
    void SetTargetLocation(const Vector3 &position, radian angle);
    void SetTargetLocation(const Vector3 &position, const Quaternion &orientation);

    // Screen-anchored nodes (e.g. the pre-match hub captain previews): their
    // transform is recomputed together with the camera so they stay locked to
    // the viewport instead of lagging a tick behind the panning camera.
    struct ScreenAnchor {
      boost::intrusive_ptr<Node> node;
      Vector3 offset;      // camera space
      Quaternion rotation; // camera space
    };
    void AddScreenAnchor(boost::intrusive_ptr<Node> node, const Vector3 &offset, const Quaternion &rotation);
    void RemoveScreenAnchor(boost::intrusive_ptr<Node> node);

  protected:
    boost::intrusive_ptr<Node> containerNode;
    boost::intrusive_ptr<Camera> camera;
    boost::intrusive_ptr<Light> mainLight;
    boost::intrusive_ptr<Geometry> geom;

    boost::intrusive_ptr<Light> hoverLights[3];
    Vector3 hoverLightPosition;

    std::vector<ScreenAnchor> screenAnchors;

    boost::shared_ptr<Scene3D> scene3D;

    MenuSceneLocation sourceLocation;
    MenuSceneLocation targetLocation;

    Vector3 currentPosition;
    Quaternion currentOrientation;

    bool seamless;

};

#endif
