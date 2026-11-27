// RXDK-SDL3 / original Xbox implementation of platform.h
//
// Video: SDL creates the full-screen window and owns the console's Direct3D 8
// device (published as a window property). Each frame the game's 800x600
// surface is copied into a linear texture and the GPU draws it, filtered, as
// a 4:3 quad centred on the screen. This replaces the nxdk port's CPU scaler
// and its direct CRTC register writes.
//
// If the device is not there (it always is on the console), SDL's software
// renderer is used instead.

#include <xtl.h>

#include "platform.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

namespace {

SDL_Window*        g_window = NULL;
char               g_modeName[96] = "none";
char               g_displayLabel[32] = "AUTO";
int                g_width = 0, g_height = 0;
SDL_FRect          g_picture = { 0, 0, 0, 0 };

// Direct3D path.
D3DDevice*         g_device = NULL;
D3DTexture*        g_textures[2] = { NULL, NULL };
int                g_next = 0;

// Fallback path.
SDL_Renderer*      g_renderer = NULL;
SDL_Texture*       g_sdlTexture = NULL;

const char*        g_saveDir = NULL;
bool               g_ready = false;
const char*        g_pendingScreen = NULL;   // logged once its first frame is on screen

struct QuadVertex
{
    float x, y, z, rhw;
    float u, v;
};

const SDL_DisplayMode* findMode(SDL_DisplayMode** modes, int count, int w, int h)
{
    for (int i = 0; i < count; i++)
        if (modes[i]->w == w && modes[i]->h == h)
            return modes[i];
    return NULL;
}

// Same preference as the nxdk port: a forced mode if available, then 720p,
// 1080i, and standard definition (480 lines, or 576 on 50 Hz PAL).
// The console only lists modes its AV cable and dashboard settings allow.
const SDL_DisplayMode* pickMode(int forcedHeight)
{
    int count = 0;
    SDL_DisplayMode** modes = SDL_GetFullscreenDisplayModes(SDL_GetPrimaryDisplay(), &count);
    if (!modes) return NULL;

    const SDL_DisplayMode* m = NULL;
    if (forcedHeight == 720)  m = findMode(modes, count, 1280, 720);
    if (forcedHeight == 1080) m = findMode(modes, count, 1920, 1080);
    if (forcedHeight == 480)  m = findMode(modes, count, 640, 480);

    static const int autoModes[][2] = { { 1280, 720 }, { 1920, 1080 }, { 640, 480 }, { 640, 576 } };
    for (unsigned i = 0; !m && i < SDL_arraysize(autoModes); i++)
        m = findMode(modes, count, autoModes[i][0], autoModes[i][1]);

    // SDL_GetFullscreenDisplayModes returns one allocation; the modes stay
    // valid in the display, so keep a pointer to the display's own copy.
    const SDL_DisplayMode* result = NULL;
    if (m)
    {
        static SDL_DisplayMode chosen;
        chosen = *m;
        result = &chosen;
    }
    SDL_free(modes);
    return result;
}

// The 800x600 picture at 4:3 on this mode. HD modes are 16:9, and so is SD
// when the dashboard is set to widescreen; plain SD is a 4:3 screen whatever
// its pixel count (640x480 or PAL 640x576), so the picture fills it.
void computePicture()
{
    const bool wide = g_height >= 720 || (XGetVideoFlags() & XC_VIDEO_FLAGS_WIDESCREEN);
    float w = (float)g_width;
    if (wide) w = (float)(int)(g_width * 3 / 4);
    g_picture.w = w;
    g_picture.h = (float)g_height;
    g_picture.x = (float)(int)((g_width - w) / 2);
    g_picture.y = 0;
}

bool initD3D()
{
    g_device = (D3DDevice*)SDL_GetPointerProperty(SDL_GetWindowProperties(g_window),
                                                  SDL_PROP_WINDOW_XBOXOG_D3DDEVICE_POINTER, NULL);
    if (!g_device) return false;

    for (int i = 0; i < 2; i++)
    {
        if (FAILED(g_device->CreateTexture(GAME_W, GAME_H, 1, 0, D3DFMT_LIN_X8R8G8B8, D3DPOOL_DEFAULT,
                                           &g_textures[i])))
        {
            Platform_Log("2H4U: CreateTexture failed, using SDL's renderer\n");
            for (int j = 0; j < i; j++) { g_textures[j]->Release(); g_textures[j] = NULL; }
            g_device = NULL;
            return false;
        }
    }

    g_device->SetRenderState(D3DRS_ZENABLE, FALSE);
    g_device->SetRenderState(D3DRS_LIGHTING, FALSE);
    g_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    g_device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    g_device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    g_device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    g_device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    g_device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    g_device->SetTextureStageState(0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
    g_device->SetTextureStageState(0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
    // Exact size (480p on component with no scaling) needs no filtering.
    const DWORD filter = (g_picture.w == GAME_W && g_picture.h == GAME_H) ? D3DTEXF_POINT : D3DTEXF_LINEAR;
    g_device->SetTextureStageState(0, D3DTSS_MAGFILTER, filter);
    g_device->SetTextureStageState(0, D3DTSS_MINFILTER, filter);
    g_device->SetTextureStageState(0, D3DTSS_MIPFILTER, D3DTEXF_NONE);
    g_device->SetVertexShader(D3DFVF_XYZRHW | D3DFVF_TEX1);
    return true;
}

bool initRenderer()
{
    g_renderer = SDL_CreateRenderer(g_window, NULL);
    if (!g_renderer) return false;
    SDL_SetRenderVSync(g_renderer, 1);
    g_sdlTexture = SDL_CreateTexture(g_renderer, COMPAT_SCREEN_FORMAT, SDL_TEXTUREACCESS_STREAMING, GAME_W, GAME_H);
    if (!g_sdlTexture) return false;
    SDL_SetTextureScaleMode(g_sdlTexture, SDL_SCALEMODE_LINEAR);
    return true;
}

void presentD3D(SDL_Surface* frame)
{
    // Alternate between two textures: the one drawn last frame may still be
    // in use by the GPU, and locking it would wait for it.
    D3DTexture* tex = g_textures[g_next];
    g_next ^= 1;

    D3DLOCKED_RECT locked;
    if (SUCCEEDED(tex->LockRect(0, &locked, NULL, 0)))
    {
        const Uint8* src = (const Uint8*)frame->pixels;
        Uint8* dst = (Uint8*)locked.pBits;
        for (int y = 0; y < GAME_H; y++)
            memcpy(dst + (size_t)y * locked.Pitch, src + (size_t)y * frame->pitch, GAME_W * 4);
        tex->UnlockRect(0);
    }

    // Linear textures are addressed in texels, not 0..1.
    const float x0 = g_picture.x - 0.5f, y0 = g_picture.y - 0.5f;
    const float x1 = x0 + g_picture.w,   y1 = y0 + g_picture.h;
    const QuadVertex quad[4] = {
        { x0, y0, 0.0f, 1.0f, 0.0f,          0.0f },
        { x1, y0, 0.0f, 1.0f, (float)GAME_W, 0.0f },
        { x0, y1, 0.0f, 1.0f, 0.0f,          (float)GAME_H },
        { x1, y1, 0.0f, 1.0f, (float)GAME_W, (float)GAME_H },
    };

    g_device->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
    g_device->BeginScene();
    g_device->SetTexture(0, tex);
    g_device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(QuadVertex));
    g_device->SetTexture(0, NULL);
    g_device->EndScene();
    g_device->Present(NULL, NULL, NULL, NULL);   // waits for vblank (interval one)
}

void presentRenderer(SDL_Surface* frame)
{
    SDL_UpdateTexture(g_sdlTexture, NULL, frame->pixels, frame->pitch);
    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_renderer);
    SDL_RenderTexture(g_renderer, g_sdlTexture, NULL, &g_picture);
    SDL_RenderPresent(g_renderer);
}

void vlog(const char* fmt, va_list ap)
{
    char line[512];
    SDL_vsnprintf(line, sizeof(line), fmt, ap);
    OutputDebugStringA(line);
}

} // namespace

void Platform_Init()
{
    // U:\ is this title's saved-game folder (E:\UDATA\32483455\, from the
    // XBE's title ID). Same place the nxdk and original XDK builds used.
    const char* dir = SDL_GetUserFolder(SDL_FOLDER_SAVEDGAMES);
    g_saveDir = dir ? dir : "U:\\";
}

bool Platform_VideoInit(int forcedHeight)
{
    if (!SDL_WasInit(SDL_INIT_VIDEO) && !SDL_InitSubSystem(SDL_INIT_VIDEO))
    {
        Platform_Log("2H4U: SDL video init failed: %s\n", SDL_GetError());
        return false;
    }

    const SDL_DisplayMode* mode = pickMode(forcedHeight);
    const SDL_DisplayMode* desktop = SDL_GetDesktopDisplayMode(SDL_GetPrimaryDisplay());
    if (!mode) mode = desktop;
    if (!mode) return false;

    g_window = SDL_CreateWindow("2H4U", mode->w, mode->h, SDL_WINDOW_FULLSCREEN);
    if (!g_window)
    {
        Platform_Log("2H4U: SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }
    if (!SDL_SetWindowFullscreenMode(g_window, mode))
        Platform_Log("2H4U: SDL_SetWindowFullscreenMode %dx%d failed: %s\n", mode->w, mode->h, SDL_GetError());
    SDL_SyncWindow(g_window);
    SDL_HideCursor();

    const SDL_DisplayMode* current = SDL_GetWindowFullscreenMode(g_window);
    if (!current) current = desktop;
    g_width = current->w;
    g_height = current->h;
    computePicture();

    const bool d3d = initD3D();
    if (!d3d && !initRenderer())
    {
        Platform_Log("2H4U: no way to present: %s\n", SDL_GetError());
        return false;
    }

    SDL_snprintf(g_modeName, sizeof(g_modeName), "%dx%d@%gHz (picture %dx%d, %s)",
                 g_width, g_height, current->refresh_rate, (int)g_picture.w, (int)g_picture.h,
                 d3d ? "Direct3D" : "SDL renderer");

    // Progressive or interlaced comes from the Direct3D mode. Without the
    // device, go by what the console outputs at each height: 1080 is always
    // interlaced, 720 always progressive, and 480 is taken as interlaced.
    bool progressive = (g_height == 720);
    if (d3d)
    {
        D3DDISPLAYMODE dm;
        g_device->GetDisplayMode(&dm);
        progressive = (dm.Flags & D3DPRESENTFLAG_PROGRESSIVE) != 0;
    }
    const bool forced = (forcedHeight != 0 && forcedHeight == g_height);
    SDL_snprintf(g_displayLabel, sizeof(g_displayLabel), "%s%d%c",
                 forced ? "" : "AUTO ", g_height, progressive ? 'p' : 'i');
    Platform_Log("2H4U: display label %s\n", g_displayLabel);
    return true;
}

const char* Platform_DisplayLabel()
{
    return g_displayLabel;
}

void Platform_Present(SDL_Surface* frame)
{
    if (g_device) presentD3D(frame);
    else if (g_renderer) presentRenderer(frame);

    if (g_pendingScreen)
    {
        Platform_Log("2H4U SCREEN %s\n", g_pendingScreen);
        g_pendingScreen = NULL;
        if (!g_ready)
        {
            g_ready = true;
            Platform_Log("2H4U READY\n");
        }
    }
}

const char* Platform_VideoModeName()
{
    return g_modeName;
}

const char* Platform_SavePath(const char* fileName)
{
    // Callers keep the pointer (CParam/CHighscore store it), so each call
    // gets its own copy. Only called a couple of times at startup.
    char path[260];
    SDL_snprintf(path, sizeof(path), "%s%s", g_saveDir ? g_saveDir : "U:\\", fileName);
    return SDL_strdup(path);
}

void Platform_CommitSave(const char* path)
{
    // fclose leaves the data in the kernel's FATX cache; a power cut (or an
    // emulator being killed) before it is written back loses the save.
    HANDLE h = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    FlushFileBuffers(h);
    CloseHandle(h);
}

void Platform_Exit(int code)
{
    // Config and high scores are already written and closed by now.
    // SDL_Quit closes the audio device (stopping DirectSound) and releases
    // the Direct3D device; XAPI's XLaunchNewImage then returns to the
    // dashboard. This is also what SDL does when main() returns.
    (void)code;
    Platform_Log("2H4U: exit\n");
    for (int i = 0; i < 2; i++)
        if (g_textures[i]) { g_textures[i]->Release(); g_textures[i] = NULL; }
    g_device = NULL;
    SDL_Quit();
    XLaunchNewImage(NULL, NULL);
}

bool Platform_CheckIGR(SDL_Gamepad* pad)
{
    if (pad == NULL) return false;

    // Triggers are 0..32767 in SDL; count them as held past half travel.
    const int held = 16384;
    if (SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER)  < held ||
        SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) < held)
        return false;

    // Start or Black, so it works on both the Duke and the Controller S.
    // RXDK-SDL3 maps Black to the right shoulder button.
    if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_BACK) &&
        (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_START) ||
         SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)))
    {
        // Saves are already flushed when written (Platform_CommitSave).
        // HalRebootRoutine (1), a full reboot, not the quick reboot (2).
        Platform_Log("2H4U: IGR\n");
        HalReturnToFirmware(HalRebootRoutine);
    }
    return true;
}

void Platform_Log(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vlog(fmt, ap);
    va_end(ap);
}

int Platform_FPrintf(FILE* f, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r;
    if (f == stderr || f == stdout)
    {
        vlog(fmt, ap);
        r = 0;
    }
    else
        r = vfprintf(f, fmt, ap);
    va_end(ap);
    return r;
}

void Platform_ScreenShown(const char* name)
{
    g_pendingScreen = name;
}
