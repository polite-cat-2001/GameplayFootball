// written by bastiaan konings schuiling 2008 - 2014
// this work is public domain. the code is undocumented, scruffy, untested, and should generally not be used for anything important.
// i do not offer support, so don't ask. to be used for inspiration :)

#ifndef _hpp_sdl_surface
#define _hpp_sdl_surface

#include "SDL3_image/SDL_image.h"
#include "SDL3/SDL_endian.h"

namespace blunted {

  class Triangle;

  #if SDL_BYTEORDER == SDL_BIG_ENDIAN
  static const Uint32 r_mask = 0xFF000000;
  static const Uint32 g_mask = 0x00FF0000;
  static const Uint32 b_mask = 0x0000FF00;
  static const Uint32 a_mask = 0x000000FF;
  #else
  static const Uint32 r_mask = 0x000000FF;
  static const Uint32 g_mask = 0x0000FF00;
  static const Uint32 b_mask = 0x00FF0000;
  static const Uint32 a_mask = 0xFF000000;
  #endif

  #if SDL_BYTEORDER == SDL_BIG_ENDIAN
  static const Uint8 r_mask8 = 0xC0;
  static const Uint8 g_mask8 = 0x80;
  static const Uint8 b_mask8 = 0x40;
  static const Uint8 a_mask8 = 0x00;
  #else
  static const Uint8 r_mask8 = 0x00;
  static const Uint8 g_mask8 = 0x40;
  static const Uint8 b_mask8 = 0x80;
  static const Uint8 a_mask8 = 0xC0;
  #endif

  SDL_Surface *CreateSDLSurface(int width, int height);
  void sdl_putpixel(SDL_Surface *surface, int x, int y, Uint32 pixel);
  Uint32 sdl_getpixel(const SDL_Surface *surface, int x, int y);
  // pads the canvas by `radius` px on all sides, then grows a colored outline of
  // `radius` px around the non-transparent region of a 32-bit surface, following
  // the shape. the padding guarantees full-bleed logos and their outline are not
  // clipped at the source edges. returns a NEW surface (caller replaces the old
  // one; the old surface is not freed here). returns `surface` unchanged when the
  // input is not a 32-bit surface or radius < 1
  SDL_Surface *sdl_addoutline(SDL_Surface *surface, int radius, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
  void sdl_line(SDL_Surface *surface, int x1, int y1, int x2, int y2, Uint32 color);
  void sdl_triangle_filled(SDL_Surface *surface, const Triangle &triangle, Uint8 r, Uint8 g, Uint8 b);
  void sdl_rectangle_filled(SDL_Surface *surface, int x, int y, int width, int height, Uint32 color);
  void sdl_alphablit(SDL_Surface *src, SDL_Rect *srcrect, SDL_Surface *dst, SDL_Rect *dstrect);
  void sdl_flipsurface(SDL_Surface *surface);

  // only works on 32-bits surfaces
  // might have endian problems
  void sdl_setsurfacealpha(SDL_Surface *surface, int alpha);

}

#endif
