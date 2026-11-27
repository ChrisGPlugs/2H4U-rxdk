#include "image_load.h"
#include "platform.h"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#define STBI_NO_SIMD          // Pentium III: no SSE2
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_ASSERT(x) SDL_assert(x)
#define STBI_MALLOC SDL_malloc
#define STBI_REALLOC SDL_realloc
#define STBI_FREE SDL_free
#include "../third_party/stb_image.h"

SDL_Surface* Compat_IMG_Load(const char* path)
{
    size_t size = 0;
    void* file = SDL_LoadFile(path, &size);
    if (!file)
    {
        Platform_Log("2H4U: can't read %s: %s\n", path, SDL_GetError());
        return NULL;
    }

    int w = 0, h = 0, comp = 0;
    stbi_uc* rgba = stbi_load_from_memory((const stbi_uc*)file, (int)size, &w, &h, &comp, 4);
    SDL_free(file);
    if (!rgba)
    {
        Platform_Log("2H4U: can't decode %s: %s\n", path, stbi_failure_reason());
        return NULL;
    }

    // stb gives R,G,B,A bytes in memory order: SDL_PIXELFORMAT_RGBA32.
    // Like SDL_image: partial alpha -> ARGB8888 with blending; only fully
    // clear/fully solid pixels (palette PNGs with one transparent entry, such
    // as jeu/transparent.png) -> the opaque screen format with a colour key.
    // The game relies on the latter: SDL_DisplayFormat() drops per-pixel
    // alpha but keeps a colour key, so the pause/game-over overlay stays
    // see-through.
    const size_t n = (size_t)w * h;
    bool partial = false, clear = false;
    if (comp == 2 || comp == 4)
    {
        for (size_t i = 0; i < n && !partial; i++)
        {
            const stbi_uc a = rgba[i * 4 + 3];
            if (a == 0) clear = true;
            else if (a != 255) partial = true;
        }
    }

    Uint32 key = 0;
    const bool keyed = clear && !partial;
    if (keyed)
    {
        // Key colour: the transparent pixels' own colour if no visible pixel
        // uses it, otherwise the first of a few unlikely colours that is free.
        static const Uint32 candidates[] = { 0xFF00FF, 0x00FF01, 0x010203, 0xFE01FE };
        Uint32 first = 0;
        for (size_t i = 0; i < n; i++)
            if (rgba[i * 4 + 3] == 0) { first = (rgba[i*4] << 16) | (rgba[i*4+1] << 8) | rgba[i*4+2]; break; }
        key = first;
        for (int c = -1; c < (int)SDL_arraysize(candidates); c++)
        {
            if (c >= 0) key = candidates[c];
            bool used = false;
            for (size_t i = 0; i < n && !used; i++)
                used = rgba[i * 4 + 3] == 255 &&
                       ((Uint32)((rgba[i*4] << 16) | (rgba[i*4+1] << 8) | rgba[i*4+2])) == key;
            if (!used) break;
        }
        for (size_t i = 0; i < n; i++)
        {
            if (rgba[i * 4 + 3] != 0) continue;
            rgba[i * 4 + 0] = (stbi_uc)(key >> 16);
            rgba[i * 4 + 1] = (stbi_uc)(key >> 8);
            rgba[i * 4 + 2] = (stbi_uc)key;
        }
    }

    SDL_Surface* wrapped = SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_RGBA32, rgba, w * 4);
    SDL_Surface* result = NULL;
    if (wrapped)
    {
        result = SDL_ConvertSurface(wrapped, partial ? SDL_PIXELFORMAT_ARGB8888 : COMPAT_SCREEN_FORMAT);
        SDL_DestroySurface(wrapped);
    }
    stbi_image_free(rgba);

    if (!result)
    {
        Platform_Log("2H4U: can't convert %s: %s\n", path, SDL_GetError());
        return NULL;
    }
    if (keyed)
        SDL_SetSurfaceColorKey(result, true, SDL_MapSurfaceRGB(result, (Uint8)(key >> 16), (Uint8)(key >> 8), (Uint8)key));
    SDL_SetSurfaceBlendMode(result, partial ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);
    return result;
}
