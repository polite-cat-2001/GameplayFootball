// written by bastiaan konings schuiling 2008 - 2015
// this work is public domain. the code is undocumented, scruffy, untested, and should generally not be used for anything important.
// i do not offer support, so don't ask. to be used for inspiration :)

#include "prematchcaptain.hpp"

#include <boost/filesystem.hpp>

#include <cmath>
#include <map>
#include <vector>

#include "base/geometry/trianglemeshutils.hpp"
#include "base/log.hpp"
#include "base/utils.hpp"

#include "managers/resourcemanagerpool.hpp"

#include "scene/objectfactory.hpp"
#include "scene/resources/geometrydata.hpp"

#include "utils/animation.hpp"
#include "utils/objectloader.hpp"

#include "../../data/playerdata.hpp"
#include "../../data/teamdata.hpp"
#include "../../gamedefines.hpp"
#include "../../main.hpp"

using namespace blunted;

namespace {

  // Instance counter: the geometry data copy is keyed by resource name, so a
  // unique postfix per instance is what keeps two previews from sharing one kit.
  int previewSerial = 0;

  void GatherNodeMap(boost::intrusive_ptr<Node> node,
                     std::map<const std::string, boost::intrusive_ptr<Node> > &nodeMap) {
    nodeMap.insert(std::make_pair(node->GetName(), node));
    std::vector<boost::intrusive_ptr<Node> > children;
    node->GetNodes(children);
    for (unsigned int i = 0; i < children.size(); i++) GatherNodeMap(children.at(i), nodeMap);
  }

  Vector3 ReadVector(const float *data, int index) {
    return Vector3(data[index], data[index + 1], data[index + 2]);
  }

  void WriteVector(float *data, int index, const Vector3 &value) {
    data[index] = value.coords[0];
    data[index + 1] = value.coords[1];
    data[index + 2] = value.coords[2];
  }

}

PreMatchCaptainPreview::PreMatchCaptainPreview(TeamData *teamData, PlayerData *playerData, int kitNumber, bool leftSide)
    : teamData(teamData), playerData(playerData), kitNumber(kitNumber), leftSide(leftSide) {
  // A world-space height of 0.85 at 0.6 from the menu camera keeps the model
  // around 70% of the screen height; the side offset puts it left/right of the
  // 20..80% content strip.
  previewHeight = 0.72f;
  previewDistance = 0.60f;
  previewSideOffset = 0.72f;
  meshScale = 1.0f;
  meshPivot.Set(0, 0, 0);
  currentKitIdent = "kit_template.png";
  Create();
}

PreMatchCaptainPreview::~PreMatchCaptainPreview() {
  Destroy();
}

boost::intrusive_ptr < Resource<Surface> > PreMatchCaptainPreview::FetchKitResource(int kit) {
  if (kit < 1) kit = 1;
  std::string suffix = int_to_str(kit);
  if (suffix.size() < 2) suffix = "0" + suffix;
  std::string filename = teamData->GetKitUrl() + "_kit_" + suffix + ".png";
  if (!boost::filesystem::exists(filename)) filename = "media/textures/almost_white.png";
  return ResourceManagerPool::GetInstance().GetManager<Surface>(e_ResourceType_Surface)->Fetch(filename);
}

boost::intrusive_ptr<Geometry> PreMatchCaptainPreview::GetFullbodyGeometry() {
  return fullbodyGeometry;
}

void PreMatchCaptainPreview::Create() {

  // Structural scene change; the graphics thread traverses the scene each frame.
  GetGraphicsSystem()->getPhaseMutex.lock();

  const std::string postfix = "_preview" + int_to_str(previewSerial++);

  skinResource = ResourceManagerPool::GetInstance().GetManager<Surface>(e_ResourceType_Surface)->Fetch(
      "media/objects/players/textures/skin0" + int_to_str(playerData->GetSkinColor()) + ".png", true, true);
  hairResource = ResourceManagerPool::GetInstance().GetManager<Surface>(e_ResourceType_Surface)->Fetch(
      "media/objects/players/textures/hair/" + playerData->GetHairColor() + ".png", true, true);
  kitResource = FetchKitResource(kitNumber);

  // The joint hierarchy. Its geometry parts are dropped immediately: only the
  // node transforms are needed to bake the skinned fullbody mesh.
  ObjectLoader loader;
  rigNode = loader.LoadObject(GetScene3D(), "media/objects/players/player.object");
  rigNode->SetName("player");
  rigNode->SetLocalMode(e_LocalMode_Absolute);
  rigNode->DeleteAllObjects(true);

  // The skinned mesh, as a private deep copy of the template.
  {
    boost::intrusive_ptr < Resource<GeometryData> > geometry =
        ResourceManagerPool::GetInstance().GetManager<GeometryData>(e_ResourceType_GeometryData)->FetchCopy(
            "media/objects/players/models/fullbody.ase", "fullbody.ase" + postfix);
    fullbodyGeometry = boost::static_pointer_cast<Geometry>(
        ObjectFactory::GetInstance().CreateObject("prematch_captain_body" + postfix, e_ObjectType_Geometry));
    GetScene3D()->CreateSystemObjects(fullbodyGeometry);
    fullbodyGeometry->SetGeometryData(geometry);
    fullbodyGeometry->SetLocalMode(e_LocalMode_Relative);
    fullbodyGeometry->GetGeometryData()->GetResource()->SetDynamic(true);
    fullbodyGeometry->SetProperty("no_cull", "true");
  }

  BakePose();

  rootNode = boost::intrusive_ptr<Node>(new Node("prematch_captain_root" + postfix));
  GetScene3D()->AddNode(rootNode);
  rootNode->AddObject(fullbodyGeometry);

  // Hairstyle: a scaled copy so it matches the normalized body size.
  {
    std::string hairPath = "media/objects/players/hairstyles/" + playerData->GetHairStyle() + ".ase";
    boost::intrusive_ptr < Resource<GeometryData> > hairData =
        ResourceManagerPool::GetInstance().GetManager<GeometryData>(e_ResourceType_GeometryData)->FetchCopy(
            hairPath, get_file_name(hairPath) + postfix);
    std::vector < MaterializedTriangleMesh > &hairMeshes = hairData->GetResource()->GetTriangleMeshesRef();
    for (unsigned int i = 0; i < hairMeshes.size(); i++) {
      float *vertices = hairMeshes.at(i).vertices;
      int elementOffset = hairMeshes.at(i).verticesDataSize / GetTriangleMeshElementCount();
      for (int v = 0; v < elementOffset; v += 3) {
        Vector3 p = ReadVector(vertices, v) * meshScale;
        WriteVector(vertices, v, p);
      }
      if (hairMeshes.at(i).material.diffuseTexture) {
        hairMeshes.at(i).material.diffuseTexture = hairResource;
        hairMeshes.at(i).material.specular_amount = 0.01f;
        hairMeshes.at(i).material.shininess = 0.05f;
      }
    }
    hairData->GetResource()->InvalidateAABB();
    hairData->GetResource()->SetDynamic(true);

    hairNode = boost::static_pointer_cast<Geometry>(
        ObjectFactory::GetInstance().CreateObject("prematch_captain_hair" + postfix, e_ObjectType_Geometry));
    GetScene3D()->CreateSystemObjects(hairNode);
    hairNode->SetGeometryData(hairData);
    hairNode->SetLocalMode(e_LocalMode_Relative);
    hairNode->SetProperty("no_cull", "true");
    hairNode->SetPosition(headLocalPosition);
    hairNode->SetRotation(headLocalRotation);
    rootNode->AddObject(hairNode);
  }

  // The menu scene recomputes the transform alongside the camera each frame,
  // which keeps the model locked to the viewport while the camera pans.
  MenuScene *scene = GetGameTask() ? GetGameTask()->GetMenuScene() : 0;
  if (scene && rootNode) {
    Quaternion baseRot;
    baseRot.SetAngleAxis(-0.5f * pi, Vector3(1, 0, 0));
    float x = (leftSide ? -previewSideOffset : previewSideOffset);
    Vector3 offset(x, -previewHeight * 0.42f, -previewDistance);
    scene->AddScreenAnchor(rootNode, offset, baseRot);
  }

  GetGraphicsSystem()->getPhaseMutex.unlock();
}

void PreMatchCaptainPreview::Destroy() {
  GetGraphicsSystem()->getPhaseMutex.lock();
  MenuScene *scene = GetGameTask() ? GetGameTask()->GetMenuScene() : 0;
  if (scene && rootNode) scene->RemoveScreenAnchor(rootNode);
  if (rootNode) {
    GetScene3D()->DeleteNode(rootNode);
    rootNode.reset();
  }
  fullbodyGeometry.reset();
  hairNode.reset();
  if (rigNode) {
    rigNode->Exit();
    rigNode.reset();
  }
  kitResource.reset();
  skinResource.reset();
  hairResource.reset();
  GetGraphicsSystem()->getPhaseMutex.unlock();
}

void PreMatchCaptainPreview::BakePose() {

  boost::intrusive_ptr < Resource<GeometryData> > geometry = fullbodyGeometry->GetGeometryData();
  GeometryData *data = geometry->GetResource();

  // skin weights per vertex live in the vertex colors of the fullbody mesh
  std::map<Vector3, Vector3> colorCoords;
  GetVertexColors(colorCoords);

  std::map<const std::string, boost::intrusive_ptr<Node> > nodeMap;
  GatherNodeMap(rigNode, nodeMap);

  std::vector<boost::intrusive_ptr<Node> > joints;
  rigNode->GetNodes(joints, true);

  std::map < std::string, BiasedOffset > offsets;

  // base pose: the mesh was authored with the joints at these angles
  Animation baseAnim;
  baseAnim.Load("media/animations/base.anim.util");
  baseAnim.Apply(nodeMap, 0, 0, false, 0.0f, Vector3(0), 0, offsets, 0, 10, false, true);

  std::vector<Vector3> basePos(joints.size());
  std::vector<Quaternion> baseRotInv(joints.size());
  for (unsigned int i = 0; i < joints.size(); i++) {
    basePos.at(i) = joints.at(i)->GetDerivedPosition();
    baseRotInv.at(i) = joints.at(i)->GetDerivedRotation().GetInverse();
  }

  // target pose: a calm idle frame
  Animation idleAnim;
  idleAnim.Load("media/animations/movement/idle/000_idlelevel1.anim");
  idleAnim.Apply(nodeMap, idleAnim.GetEffectiveFrameCount() / 2, 0, false, 0.0f, Vector3(0), 0, offsets, 0, 10, false, true);

  std::vector<Vector3> targetPos(joints.size());
  std::vector<Quaternion> targetRot(joints.size());
  for (unsigned int i = 0; i < joints.size(); i++) {
    targetPos.at(i) = joints.at(i)->GetDerivedPosition();
    targetRot.at(i) = joints.at(i)->GetDerivedRotation();
  }

  // skinning: (v - basePos) rotated by targetRot * inverse(baseRot), then moved
  // to targetPos, blended with the per-vertex bone weights.
  int elementCount = GetTriangleMeshElementCount();
  std::vector < MaterializedTriangleMesh > &meshes = data->GetTriangleMeshesRef();
  for (unsigned int m = 0; m < meshes.size(); m++) {
    float *vertices = meshes.at(m).vertices;
    int elementOffset = meshes.at(m).verticesDataSize / elementCount;
    for (int v = 0; v < elementOffset; v += 3) {
      Vector3 original = ReadVector(vertices, v);
      const Vector3 *color = 0;
      std::map<Vector3, Vector3>::iterator colorIter = colorCoords.find(original);
      if (colorIter != colorCoords.end()) color = &colorIter->second;

      if (color) {
        float totalWeight = 0.0f;
        int jointID[3];
        float weight[3];
        for (int c = 0; c < 3; c++) {
          jointID[c] = (int)floor(color->coords[c] * 0.1f);
          weight[c] = (color->coords[c] - jointID[c] * 10.0f) / 9.0f;
          totalWeight += weight[c];
        }
        if (totalWeight <= 0.0001f) totalWeight = 1.0f;

        Vector3 resultVertex(0);
        Vector3 resultNormal(0);
        Vector3 resultTangent(0);
        Vector3 resultBitangent(0);
        Vector3 originalNormal = ReadVector(vertices, v + elementOffset);
        Vector3 originalTangent = ReadVector(vertices, v + elementOffset * 3);
        Vector3 originalBitangent = ReadVector(vertices, v + elementOffset * 4);

        for (int c = 0; c < 3; c++) {
          if (jointID[c] < 0 || jointID[c] >= (int)joints.size() || weight[c] <= 0.001f) continue;
          float w = weight[c] / totalWeight;
          Quaternion boneRot = (targetRot.at(jointID[c]) * baseRotInv.at(jointID[c])).GetNormalized();

          Vector3 offset = original - basePos.at(jointID[c]);
          offset.Rotate(boneRot);
          resultVertex += (targetPos.at(jointID[c]) + offset) * w;

          Vector3 n = originalNormal;
          n.Rotate(boneRot);
          resultNormal += n * w;
          Vector3 t = originalTangent;
          t.Rotate(boneRot);
          resultTangent += t * w;
          Vector3 b = originalBitangent;
          b.Rotate(boneRot);
          resultBitangent += b * w;
        }

        resultNormal.FastNormalize();
        resultTangent.FastNormalize();
        resultBitangent.FastNormalize();

        WriteVector(vertices, v, resultVertex);
        WriteVector(vertices, v + elementOffset, resultNormal);
        WriteVector(vertices, v + elementOffset * 3, resultTangent);
        WriteVector(vertices, v + elementOffset * 4, resultBitangent);
      }
    }
  }

  // normalize size: fit the standing model to previewHeight and drop its feet
  // onto the local origin.
  AABB bounds;
  bounds.Reset();
  for (unsigned int m = 0; m < meshes.size(); m++) {
    float *vertices = meshes.at(m).vertices;
    int elementOffset = meshes.at(m).verticesDataSize / elementCount;
    for (int v = 0; v < elementOffset; v += 3) {
      Vector3 p = ReadVector(vertices, v);
      if (p.coords[0] < bounds.minxyz.coords[0]) bounds.minxyz.coords[0] = p.coords[0];
      if (p.coords[1] < bounds.minxyz.coords[1]) bounds.minxyz.coords[1] = p.coords[1];
      if (p.coords[2] < bounds.minxyz.coords[2]) bounds.minxyz.coords[2] = p.coords[2];
      if (p.coords[0] > bounds.maxxyz.coords[0]) bounds.maxxyz.coords[0] = p.coords[0];
      if (p.coords[1] > bounds.maxxyz.coords[1]) bounds.maxxyz.coords[1] = p.coords[1];
      if (p.coords[2] > bounds.maxxyz.coords[2]) bounds.maxxyz.coords[2] = p.coords[2];
    }
  }
  float rawHeight = bounds.maxxyz.coords[2] - bounds.minxyz.coords[2];
  meshScale = (rawHeight > 0.001f) ? (previewHeight / rawHeight) : 1.0f;
  meshPivot.Set((bounds.minxyz.coords[0] + bounds.maxxyz.coords[0]) * 0.5f,
                (bounds.minxyz.coords[1] + bounds.maxxyz.coords[1]) * 0.5f,
                bounds.minxyz.coords[2]);
  for (unsigned int m = 0; m < meshes.size(); m++) {
    float *vertices = meshes.at(m).vertices;
    int elementOffset = meshes.at(m).verticesDataSize / elementCount;
    for (int v = 0; v < elementOffset; v += 3) {
      Vector3 p = (ReadVector(vertices, v) - meshPivot) * meshScale;
      WriteVector(vertices, v, p);
    }
  }

  data->InvalidateAABB();

  // materials: skin + kit, then upload the freshly baked mesh
  {
    geometry->resourceMutex.lock();
    for (unsigned int m = 0; m < meshes.size(); m++) {
      if (!meshes.at(m).material.diffuseTexture) continue;
      std::string ident = meshes.at(m).material.diffuseTexture->GetIdentString();
      if (ident == "skin.jpg") {
        meshes.at(m).material.diffuseTexture = skinResource;
        meshes.at(m).material.specular_amount = 0.002f;
        meshes.at(m).material.shininess = 0.2f;
      } else if (ident == "kit_template.png" || ident == currentKitIdent) {
        meshes.at(m).material.diffuseTexture = kitResource;
        meshes.at(m).material.specular_amount = 0.01f;
        meshes.at(m).material.shininess = 0.01f;
      }
    }
    geometry->resourceMutex.unlock();
  }
  currentKitIdent = kitResource->GetIdentString();
  fullbodyGeometry->OnUpdateGeometryData(true);

  // hair sits on the neck joint (index 2 in the recursive node order)
  if (joints.size() > 2) {
    headLocalPosition = (targetPos.at(2) - meshPivot) * meshScale;
    headLocalRotation = targetRot.at(2);
  }
}

void PreMatchCaptainPreview::SetKit(int kit) {
  if (kitNumber == kit) return;
  kitNumber = kit;
  kitResource = FetchKitResource(kit);

  GetGraphicsSystem()->getPhaseMutex.lock();
  boost::intrusive_ptr < Resource<GeometryData> > geometry = fullbodyGeometry->GetGeometryData();
  geometry->resourceMutex.lock();
  std::vector < MaterializedTriangleMesh > &meshes = geometry->GetResource()->GetTriangleMeshesRef();
  for (unsigned int m = 0; m < meshes.size(); m++) {
    if (!meshes.at(m).material.diffuseTexture) continue;
    if (meshes.at(m).material.diffuseTexture->GetIdentString() == currentKitIdent) {
      meshes.at(m).material.diffuseTexture = kitResource;
      meshes.at(m).material.specular_amount = 0.01f;
      meshes.at(m).material.shininess = 0.01f;
    }
  }
  geometry->resourceMutex.unlock();
  currentKitIdent = kitResource->GetIdentString();
  fullbodyGeometry->OnUpdateGeometryData(true);
  GetGraphicsSystem()->getPhaseMutex.unlock();
}

