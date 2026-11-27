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
    bool hasAlpha = false;
    if (comp == 2 || comp == 4)
    {
        const size_t n = (size_t)w * h;
        for (size_t i = 0; i < n; i++)
            if (rgba[i * 4 + 3] != 255) { hasAlpha = true; break; }
    }

    SDL_Surface* wrapped = SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_RGBA32, rgba, w * 4);
    SDL_Surface* result = NULL;
    if (wrapped)
    {
        result = SDL_ConvertSurface(wrapped, hasAlpha ? SDL_PIXELFORMAT_ARGB8888 : COMPAT_SCREEN_FORMAT);
        SDL_DestroySurface(wrapped);
    }
    stbi_image_free(rgba);

    if (!result)
    {
        Platform_Log("2H4U: can't convert %s: %s\n", path, SDL_GetError());
        return NULL;
    }
    SDL_SetSurfaceBlendMode(result, hasAlpha ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);
    return result;
}
