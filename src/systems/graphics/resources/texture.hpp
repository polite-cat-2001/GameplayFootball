// written by bastiaan konings schuiling 2008 - 2014
// this work is public domain. the code is undocumented, scruffy, untested, and should generally not be used for anything important.
// i do not offer support, so don't ask. to be used for inspiration :)

#ifndef _HPP_SYSTEM_GRAPHICS_RESOURCE_TEXTURE
#define _HPP_SYSTEM_GRAPHICS_RESOURCE_TEXTURE

#include "defines.hpp"

#include "base/sdl_surface.hpp"

#include "systems/graphics/rendering/interface_renderer3d.hpp"

#include "types/resource.hpp"

namespace blunted {

  class Renderer3D;

  class Texture {

    public:
      Texture();
      virtual ~Texture();

      void SetRenderer3D(Renderer3D *renderer3D);

      void DeleteTexture();
      int CreateTexture(e_InternalPixelFormat internalPixelFormat, e_PixelFormat pixelFormat, int width, int height, bool alpha, bool repeat, bool mipmaps, bool filter, bool compareDepth = false);
      // fire-and-forget variant: creates the GL texture (and uploads `source` if given) on the
      // renderer thread without blocking the caller; the id is written back to the resource there
      void CreateTextureAsync(boost::intrusive_ptr<Resource<Texture> > resource, e_InternalPixelFormat internalPixelFormat, e_PixelFormat pixelFormat, int width, int height, bool alpha, bool repeat, bool mipmaps, bool filter, SDL_Surface *source = NULL);
      void ResizeTexture(boost::intrusive_ptr<Resource<Texture> > resource, SDL_Surface *image, e_InternalPixelFormat internalPixelFormat, e_PixelFormat pixelFormat, bool alpha, bool mipmaps);
      void UpdateTexture(boost::intrusive_ptr<Resource<Texture> > resource, SDL_Surface *image, bool alpha, bool mipmaps);

      void SetID(int value);
      int GetID();

      void GetSize(int &width, int &height) const { width = this->width; height = this->height; }

    protected:
      int textureID;
      Renderer3D *renderer3D;
      int width, height;

  };

}

#endif
