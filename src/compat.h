/*
    SDL 1.2 / Xbox SDK compatibility for the RXDK (SDL3) port.

    The game was written against SDL 1.2 and the XDK. Rather than rewrite
    every call site, the handful of old APIs it uses are mapped here onto
    SDL3 with the same behaviour.
*/
#ifndef COMPAT_H
#define COMPAT_H

#include <SDL3/SDL.h>
#include <stdio.h>
#include "platform.h"
#include "image_load.h"

// SDL 1.2 surface flags (unused by SDL3).
#ifndef SDL_HWSURFACE
#define SDL_HWSURFACE 0
#endif
#ifndef SDL_SWSURFACE
#define SDL_SWSURFACE 0
#endif
#ifndef SDL_SRCALPHA
#define SDL_SRCALPHA 0x00010000
#endif

// SDL_DisplayFormat: convert to the screen format, dropping per-pixel alpha
// (SDL 1.2 behaviour). A colour key, if any, is kept.
static inline SDL_Surface* SDL_DisplayFormat(SDL_Surface* s)
{
    if (!s) return NULL;
    SDL_Surface* r = SDL_ConvertSurface(s, COMPAT_SCREEN_FORMAT);
    if (r) SDL_SetSurfaceBlendMode(r, SDL_BLENDMODE_NONE);
    return r;
}

// SDL_DisplayFormatAlpha: convert to 32-bit ARGB keeping per-pixel alpha.
static inline SDL_Surface* SDL_DisplayFormatAlpha(SDL_Surface* s)
{
    if (!s) return NULL;
    SDL_Surface* r = SDL_ConvertSurface(s, SDL_PIXELFORMAT_ARGB8888);
    if (r) SDL_SetSurfaceBlendMode(r, SDL_BLENDMODE_BLEND);
    return r;
}

// SDL_SetAlpha(surface, SDL_SRCALPHA, a): whole-surface transparency.
static inline int SDL_SetAlpha(SDL_Surface* s, Uint32 flags, Uint8 alpha)
{
    if (!s) return -1;
    if (flags & SDL_SRCALPHA)
    {
        SDL_SetSurfaceAlphaMod(s, alpha);
        return SDL_SetSurfaceBlendMode(s, SDL_BLENDMODE_BLEND) ? 0 : -1;
    }
    SDL_SetSurfaceAlphaMod(s, 255);
    return SDL_SetSurfaceBlendMode(s, SDL_BLENDMODE_NONE) ? 0 : -1;
}

// SDL3's SDL_GetTicks returns Uint64. The game keeps ticks in int/long/Uint32
// and does signed arithmetic on them (as with SDL 1.2's Uint32), so keep the
// old type.
#define SDL_GetTicks() ((Uint32)SDL_GetTicks())

// XDK: XLaunchNewImage(NULL, NULL) returned to the dashboard. Go through
// the platform layer so audio and SDL are shut down first.
#define XLaunchNewImage(a, b) Platform_Exit(0)

// The game reports errors with fprintf(stderr, ...). Send those to the
// debug output (serial on xemu) so they show up in the run logs.
#define fprintf(f, ...) Platform_FPrintf(f, __VA_ARGS__)

#endif
