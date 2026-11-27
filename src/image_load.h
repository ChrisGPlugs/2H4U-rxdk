/*
    Stand-in for SDL_image's IMG_Load (SDL3 has no JPEG loader and RXDK has
    no SDL3_image). PNG and JPEG files are decoded with stb_image.
*/
#ifndef IMAGE_LOAD_H
#define IMAGE_LOAD_H

#include <SDL3/SDL.h>

// Load a PNG or JPEG, already converted to a 32-bit format the blitters
// handle quickly:
//  - images with any transparency become ARGB8888 with blending on;
//  - opaque images become the screen format (XRGB8888) with blending off.
// Full-screen backgrounds and the intro fades are blitted every frame, so
// this conversion matters on the Xbox CPU.
SDL_Surface* Compat_IMG_Load(const char* path);

#define IMG_Load(p) Compat_IMG_Load(p)

#endif
