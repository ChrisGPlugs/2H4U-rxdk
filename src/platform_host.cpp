// Desktop SDL2 implementation of platform.h.
//
// Not a supported way to play the game - it exists so the port (scaler,
// input mapping, audio, SDL2 changes) can be built and checked on a PC.
//
// Environment variables:
//   HOST_OUTPUT=1280x720   output size to emulate (default 1280x720)
//   HOST_DATA=path         folder that contains "data/" (default: current dir)
//   HOST_SHOTS=dir         write the scaled output of selected frames as BMPs
//   HOST_SHOT_EVERY=n      ...every n frames (default 60)
#ifndef NXDK

#include "platform.h"
#include "scaler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

static SDL_Window*   g_window = NULL;
static SDL_Renderer* g_renderer = NULL;
static SDL_Texture*  g_texture = NULL;
static std::vector<uint32_t> g_out;
static int  g_width = 1280, g_height = 720;
static char g_modeName[64] = "none";
static unsigned g_frame = 0;

void Platform_Init()
{
}

bool Platform_VideoInit(int forcedHeight)
{
    const char* env = getenv("HOST_OUTPUT");
    if (env) sscanf(env, "%dx%d", &g_width, &g_height);
    else if (forcedHeight == 480)  { g_width = 640;  g_height = 480; }
    else if (forcedHeight == 1080) { g_width = 1920; g_height = 1080; }

    g_window = SDL_CreateWindow("2H4U (host test build)", SDL_WINDOWPOS_CENTERED,
                                SDL_WINDOWPOS_CENTERED, g_width, g_height, 0);
    if (!g_window) return false;
    g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_PRESENTVSYNC);
    if (!g_renderer) g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_SOFTWARE);
    if (!g_renderer) return false;
    g_texture = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_RGB888,
                                  SDL_TEXTUREACCESS_STREAMING, g_width, g_height);
    if (!g_texture) return false;
    g_out.assign((size_t)g_width * g_height, 0);

    ScaleRect r = Scaler_Configure(GAME_W, GAME_H, g_width, g_height);
    snprintf(g_modeName, sizeof(g_modeName), "%dx%d (picture %dx%d)",
             g_width, g_height, r.w, r.h);
    return true;
}

static void saveShot()
{
    const char* dir = getenv("HOST_SHOTS");
    if (!dir) return;
    const char* everyEnv = getenv("HOST_SHOT_EVERY");
    unsigned every = everyEnv ? (unsigned)atoi(everyEnv) : 60;
    if (every == 0 || g_frame % every != 0) return;

    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormatFrom(g_out.data(), g_width, g_height, 32,
                                                        g_width * 4, SDL_PIXELFORMAT_RGB888);
    char path[512];
    snprintf(path, sizeof(path), "%s/frame_%05u.bmp", dir, g_frame);
    SDL_SaveBMP(s, path);
    SDL_FreeSurface(s);
}

void Platform_Present(SDL_Surface* frame)
{
    if (SDL_MUSTLOCK(frame)) SDL_LockSurface(frame);
    Scaler_Run((const uint32_t*)frame->pixels, frame->pitch / 4, g_out.data(), g_width);
    if (SDL_MUSTLOCK(frame)) SDL_UnlockSurface(frame);

    g_frame++;
    saveShot();

    SDL_UpdateTexture(g_texture, NULL, g_out.data(), g_width * 4);
    SDL_RenderClear(g_renderer);
    SDL_RenderCopy(g_renderer, g_texture, NULL, NULL);
    SDL_RenderPresent(g_renderer);
}

const char* Platform_VideoModeName()
{
    return g_modeName;
}

const char* Platform_SavePath(const char* fileName)
{
    // Callers keep the pointer (CParam/CHighscore store it), so each call
    // gets its own copy. Only called a couple of times at startup.
    char path[512];
    snprintf(path, sizeof(path), "%s", fileName);
    return strdup(path);
}

void Platform_Exit(int code)
{
    exit(code);
}

// Game code uses Xbox paths like "D:\\data\\images\\x.png". Map them to the
// local filesystem for the host build.
const char* Host_Path(const char* xboxPath)
{
    static char buffers[4][512];
    static int  next = 0;
    char* out = buffers[next];
    next = (next + 1) & 3;

    const char* p = xboxPath;
    if (p[0] && p[1] == ':' && (p[2] == '\\' || p[2] == '/')) p += 3;   // drop "D:\"
    const char* base = getenv("HOST_DATA");
    snprintf(out, 512, "%s%s%s", base ? base : "", base ? "/" : "", p);
    for (char* c = out; *c; c++) if (*c == '\\') *c = '/';
    return out;
}

#endif // !NXDK
