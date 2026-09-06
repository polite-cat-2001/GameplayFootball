// Copyright 2019 Google LLC & Bastiaan Konings
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// written by bastiaan konings schuiling 2008 - 2014
// this work is public domain. the code is undocumented, scruffy, untested, and should generally not be used for anything important.
// i do not offer support, so don't ask. to be used for inspiration :)

#include "image.hpp"

#include "../windowmanager.hpp"

namespace blunted {

Gui2Image::Gui2Image(Gui2WindowManager *windowManager, const std::string &name,
                     float x_percent, float y_percent, float width_percent,
                     float height_percent)
    : Gui2View(windowManager, name, x_percent, y_percent, width_percent,
               height_percent) {
    int x, y, w, h;
    windowManager->GetCoordinates(x_percent, y_percent, width_percent, height_percent, x, y, w, h);
    image = windowManager->CreateImage2D(name, w, h, true);
  }

  Gui2Image::~Gui2Image() {
  }

  void Gui2Image::LoadImage(const std::string &filename) {
    SDL_Surface *imageSurfTmp = IMG_Load(filename.c_str());
    if (!imageSurfTmp) {
      Log(e_Warning, "Gui2Image", "LoadImage", "Could not load image '" + filename + "': " + SDL_GetError());
      return;
    }
    imageSource = windowManager->CreateImage2D(name + "source", imageSurfTmp->w, imageSurfTmp->h, false);

    boost::intrusive_ptr < Resource<Surface> > surfaceRes = imageSource->GetImage();
    surfaceRes->resourceMutex.lock();
    surfaceRes->GetResource()->SetData(imageSurfTmp);
    surfaceRes->resourceMutex.unlock();

    if (drawOutline) {
      surfaceRes->resourceMutex.lock();
      SDL_Surface *surf = surfaceRes->GetResource()->GetData();
      if (surf) {
        int radius = clamp(int(std::max(surf->w, surf->h) / 25), 2, 10);
        SDL_Surface *outlined = sdl_addoutline(surf, radius, 255, 255, 255, 255);
        if (outlined != surf) surfaceRes->GetResource()->SetData(outlined); // SetData frees the old surface
      }
      surfaceRes->resourceMutex.unlock();
    }

    Redraw();
  }

  void Gui2Image::Redraw() {

    // paste source image onto screen image
    if (imageSource != boost::intrusive_ptr<Image2D>()) {

      // get image
      boost::intrusive_ptr < Resource<Surface> > surfaceRes = imageSource->GetImage();
      surfaceRes->resourceMutex.lock();

      SDL_Surface *imageSurfTmp = surfaceRes->GetResource()->GetData();
      if (!imageSurfTmp) { surfaceRes->resourceMutex.unlock(); return; }

      int x, y, w, h;
      windowManager->GetCoordinates(x_percent, y_percent, width_percent, height_percent, x, y, w, h);

      double zoomx;
      zoomx = (double)w / imageSurfTmp->w;
      double zoomy;
      zoomy = (double)h / imageSurfTmp->h;
      int newW = int(imageSurfTmp->w * zoomx);
      int newH = int(imageSurfTmp->h * zoomy);
      if (newW < 1) newW = 1;
      if (newH < 1) newH = 1;
      SDL_Surface *imageSurf = SDL_ScaleSurface(imageSurfTmp, newW, newH, SDL_SCALEMODE_LINEAR);
      if (!imageSurf) { surfaceRes->resourceMutex.unlock(); return; }
      //printf("actually resized to %i %i\n", imageSurf->w, imageSurf->h);

      surfaceRes->resourceMutex.unlock();

      surfaceRes = image->GetImage();
      surfaceRes->resourceMutex.lock();

      surfaceRes->GetResource()->SetData(imageSurf);

      surfaceRes->resourceMutex.unlock();

      image->OnChange();
    }

  }

  void Gui2Image::GetImages(std::vector < boost::intrusive_ptr<Image2D> > &target) {
    target.push_back(image);
    Gui2View::GetImages(target);
  }

  void Gui2Image::SetSize(float new_width_percent, float new_height_percent) {
    Gui2View::SetSize(new_width_percent, new_height_percent);

    int x, y, w, h;
    windowManager->GetCoordinates(x_percent, y_percent, width_percent, height_percent, x, y, w, h);
    //printf("resized to %i %i\n", w, h);

    image->Resize(w, h);
    Redraw();
  }

  void Gui2Image::SetZoom(float zoomx, float zoomy) {

    // paste source image onto screen image
    if (imageSource != boost::intrusive_ptr<Image2D>()) {

      // get image
      boost::intrusive_ptr < Resource<Surface> > surfaceRes = imageSource->GetImage();
      surfaceRes->resourceMutex.lock();

      SDL_Surface *imageSurfTmp = surfaceRes->GetResource()->GetData();
      if (!imageSurfTmp) { surfaceRes->resourceMutex.unlock(); return; }

      int x, y, w, h;
      windowManager->GetCoordinates(x_percent, y_percent, width_percent, height_percent, x, y, w, h);

      double zoomx1;
      zoomx1 = (double)w / imageSurfTmp->w * zoomx;
      double zoomy1;
      zoomy1 = (double)h / imageSurfTmp->h * zoomy;
      int newW = int(imageSurfTmp->w * zoomx1);
      int newH = int(imageSurfTmp->h * zoomy1);
      if (newW < 1) newW = 1;
      if (newH < 1) newH = 1;
      SDL_Surface *imageSurf = SDL_ScaleSurface(imageSurfTmp, newW, newH, SDL_SCALEMODE_LINEAR);
      if (!imageSurf) { surfaceRes->resourceMutex.unlock(); return; }
      //printf("actually resized to %i %i\n", imageSurf->w, imageSurf->h);

      surfaceRes->resourceMutex.unlock();

      image->DrawRectangle(0, 0, w, h, Vector3(0, 0, 0), 0);

      surfaceRes = image->GetImage();
      surfaceRes->resourceMutex.lock();

      SDL_Surface *surface = surfaceRes->GetResource()->GetData();
      SDL_Rect rect;
      rect.x = w * 0.5 - imageSurf->w * 0.5;
      rect.y = h * 0.5 - imageSurf->h * 0.5;
      if (surface) SDL_BlitSurface(imageSurf, NULL, surface, &rect);

      surfaceRes->resourceMutex.unlock();

      SDL_DestroySurface(imageSurf);

      image->OnChange();
    }
  }

}
