// written by bastiaan konings schuiling 2008 - 2015
// this work is public domain. the code is undocumented, scruffy, untested, and should generally not be used for anything important.
// i do not offer support, so don't ask. to be used for inspiration :)

#include "cameramenu.hpp"

#include "../main.hpp"

using namespace blunted;

CameraPage::CameraPage(Gui2WindowManager *windowManager, const Gui2PageData &pageData) : Gui2Page(windowManager, pageData) {

  sliderZoom = new Gui2Slider(windowManager, "camzoomslider", 0, 0, 30, 6, "zoom");
  sliderHeight = new Gui2Slider(windowManager, "camheightslider", 0, 0, 30, 6, "height");
  sliderFOV = new Gui2Slider(windowManager, "camfovslider", 0, 0, 30, 6, "fov (perspective)");
  sliderAngleFactor = new Gui2Slider(windowManager, "camangleslider", 0, 0, 30, 6, "horizontal angle");
  sliderZoom->AddHelperValue(Vector3(80, 80, 250), "default", _default_CameraZoom);
  sliderHeight->AddHelperValue(Vector3(80, 80, 250), "default", _default_CameraHeight);
  sliderFOV->AddHelperValue(Vector3(80, 80, 250), "default", _default_CameraFOV);
  sliderAngleFactor->AddHelperValue(Vector3(80, 80, 250), "default", _default_CameraAngleFactor);

  Gui2Grid *grid = new Gui2Grid(windowManager, "camgrid", 30, 30, 50, 50);

  grid->AddView(sliderZoom, 0, 0);
  grid->AddView(sliderHeight, 1, 0);
  grid->AddView(sliderFOV, 2, 0);
  grid->AddView(sliderAngleFactor, 3, 0);

  grid->UpdateLayout(0.5);

  sliderZoom->sig_OnChange.connect(boost::bind(&CameraPage::UpdateCamera, this));
  sliderHeight->sig_OnChange.connect(boost::bind(&CameraPage::UpdateCamera, this));
  sliderFOV->sig_OnChange.connect(boost::bind(&CameraPage::UpdateCamera, this));
  sliderAngleFactor->sig_OnChange.connect(boost::bind(&CameraPage::UpdateCamera, this));
  this->sig_OnClose.connect(boost::bind(&CameraPage::OnClose, this));

  this->AddView(grid);
  grid->Show();

  sliderZoom->SetFocus();

  // Pre-match there is no Match yet: fall back to the saved config values.
  float zoom, height, fov, angleFactor;
  Match *match = GetGameTask()->GetMatch();
  if (match) {
    match->GetCameraParams(zoom, height, fov, angleFactor);
  } else {
    zoom = GetConfiguration()->GetReal("camera_zoom", _default_CameraZoom);
    height = GetConfiguration()->GetReal("camera_height", _default_CameraHeight);
    fov = GetConfiguration()->GetReal("camera_fov", _default_CameraFOV);
    angleFactor = GetConfiguration()->GetReal("camera_anglefactor", _default_CameraAngleFactor);
  }

  sliderZoom->SetValue(zoom);
  sliderHeight->SetValue(height);
  sliderFOV->SetValue(fov);
  sliderAngleFactor->SetValue(angleFactor);

  this->Show();
}

CameraPage::~CameraPage() {
}

void CameraPage::OnClose() {
  if (Verbose()) printf("saving camera settings\n");
  GetConfiguration()->SaveFile(GetConfigFilename());
}

void CameraPage::UpdateCamera() {
  GetConfiguration()->Set("camera_zoom", sliderZoom->GetValue());
  GetConfiguration()->Set("camera_height", sliderHeight->GetValue());
  GetConfiguration()->Set("camera_fov", sliderFOV->GetValue());
  GetConfiguration()->Set("camera_anglefactor", sliderAngleFactor->GetValue());
  Match *match = GetGameTask()->GetMatch();
  if (match) {
    match->SetCameraParams(sliderZoom->GetValue(), sliderHeight->GetValue(), sliderFOV->GetValue(), sliderAngleFactor->GetValue());
    // On a thin LAN client the camera otherwise comes from the host snapshot; let
    // this peer use its own settings from now on.
    if (match->IsRemotePresentation()) match->SetRemoteCameraOverride(true);
  }
}
