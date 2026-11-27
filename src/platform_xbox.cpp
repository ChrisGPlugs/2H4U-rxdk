// nxdk / original Xbox implementation of platform.h
#ifdef NXDK

#include "platform.h"
#include "scaler.h"

#include <hal/video.h>
#include <hal/debug.h>
#include <nxdk/mount.h>
#include <windows.h>
#include <xboxkrnl/xboxkrnl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Saves go to E:\UDATA\<title id>\, which is where dashboards look for them.
// The title ID is written into the XBE by tools/set_titleid.py (same ID as the
// original 2H4UX release, so existing saves show up under the same game).
#define SAVE_DIR "E:\\UDATA\\32483455\\"

#define PCRTC_START       0x00600800
#define PCRTC_INTR        0x00600100
#define PCRTC_INTR_EN     0x00600140
#define NV_PMC_INTR_EN_0  0x00000140

// AC97 (MCPX audio) bus-master control registers, see nxdk hal/audio.c.
#define AC97_MMIO         ((volatile unsigned char*)0xFEC00000)
#define AC97_PCM_OUT_CR   0x11B
#define AC97_PCM_OUT_SR   0x116
#define AC97_SPDIF_CR     0x17B
#define AC97_SPDIF_SR     0x176

extern "C" void usbh_core_deinit(void);

static uint32_t* g_buffers[2] = { NULL, NULL };
// Physical addresses for the CRTC. The CPU sees these buffers through the
// write-combined mapping (0xF0000000+), which the GPU knows nothing about,
// so the scan-out register must be given the real physical address.
static ULONG     g_physical[2] = { 0, 0 };
// False if the scan-out address isn't what we expect: then draw straight into
// the kernel-programmed framebuffer instead of flipping.
static bool      g_flip = false;
static int       g_back = 1;
static int       g_width = 0, g_height = 0;
static char      g_modeName[64] = "none";

void Platform_Init()
{
    if (!nxIsDriveMounted('E'))
        nxMountDrive('E', "\\Device\\Harddisk0\\Partition1\\");
    CreateDirectoryA("E:\\UDATA", NULL);
    CreateDirectoryA("E:\\UDATA\\32483455", NULL);
}

static bool trySetMode(int w, int h)
{
    if (!XVideoSetMode(w, h, 32, REFRESH_DEFAULT))
        return false;
    VIDEO_MODE vm = XVideoGetMode();
    g_width = vm.width;
    g_height = vm.height;
    return g_width > 0 && g_height > 0;
}

bool Platform_VideoInit(int forcedHeight)
{
    // XVideoSetMode only accepts modes that match the AV cable and the
    // HDTV modes enabled in the dashboard, so trying from best to worst
    // gives us "whatever the console is set up for".
    static const int autoModes[][2] = {
        { 1280, 720 },   // 720p  (component, 720p enabled)
        { 1920, 1080 },  // 1080i (component, 1080i enabled)
        { 800,  600 },   // VGA
        { 640,  480 },   // 480p on component if enabled, otherwise SD (480i / PAL)
    };

    bool ok = false;
    if (forcedHeight == 720)  ok = trySetMode(1280, 720);
    if (forcedHeight == 1080) ok = trySetMode(1920, 1080);
    if (forcedHeight == 480)  ok = trySetMode(640, 480);

    for (unsigned i = 0; !ok && i < sizeof(autoModes) / sizeof(autoModes[0]); i++)
        ok = trySetMode(autoModes[i][0], autoModes[i][1]);

    if (!ok)
        return false;

    // Double buffering: nxdk's framebuffer plus one more of the same size.
    const size_t size = (size_t)g_width * g_height * 4;
    g_buffers[0] = (uint32_t*)XVideoGetFB();
    g_buffers[1] = (uint32_t*)MmAllocateContiguousMemoryEx(size, 0, 0x7FFFFFFF, 0x1000,
                                                           PAGE_READWRITE | PAGE_WRITECOMBINE);
    if (g_buffers[1] == NULL)
        return false;
    g_physical[0] = MmGetPhysicalAddress(g_buffers[0]);
    g_physical[1] = MmGetPhysicalAddress(g_buffers[1]);
    // AvSetDisplayMode has just pointed the CRTC at nxdk's framebuffer.
    // Only flip if our idea of its address matches the hardware's.
    g_flip = (VIDEOREG(PCRTC_START) == g_physical[0]);
    memset(g_buffers[0], 0, size);
    memset(g_buffers[1], 0, size);
    XVideoFlushFB();
    g_back = g_flip ? 1 : 0;

    ScaleRect r = Scaler_Configure(GAME_W, GAME_H, g_width, g_height);
    snprintf(g_modeName, sizeof(g_modeName), "%dx%d (picture %dx%d, %s)",
             g_width, g_height, r.w, r.h, g_flip ? "double-buffered" : "single buffer");
    return true;
}

void Platform_Present(SDL_Surface* frame)
{
    if (SDL_MUSTLOCK(frame)) SDL_LockSurface(frame);
    Scaler_Run((const uint32_t*)frame->pixels, frame->pitch / 4,
               g_buffers[g_back], g_width);
    if (SDL_MUSTLOCK(frame)) SDL_UnlockSurface(frame);

    XVideoFlushFB();          // drain write-combine buffers
    XVideoWaitForVBlank();
    if (g_flip)
    {
        VIDEOREG(PCRTC_START) = g_physical[g_back];
        g_back ^= 1;
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
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s%s", SAVE_DIR, fileName);
    return strdup(path);
}

// Quiet every device that can raise an interrupt into this program's code.
// A quick reboot loads the dashboard over our memory; an interrupt arriving
// at one of our handlers during that hangs the console (audio keeps looping
// its last buffers, which is the popping heard on exit).
static void quiesceHardware()
{
    // Audio: stop both AC97 output DMA engines with interrupts disabled,
    // then clear any pending status. nxdk has no call that disconnects its
    // audio ISR, but with the engines stopped it never fires.
    if (SDL_WasInit(SDL_INIT_AUDIO))
    {
        AC97_MMIO[AC97_PCM_OUT_CR] = 0;
        AC97_MMIO[AC97_SPDIF_CR] = 0;
        AC97_MMIO[AC97_PCM_OUT_SR] = 0xFF;
        AC97_MMIO[AC97_SPDIF_SR] = 0xFF;
    }

    // USB: close the pads, then shut down nxdk's OHCI stack, which
    // disconnects its interrupt (SDL deliberately leaves this to the app).
    if (SDL_WasInit(SDL_INIT_JOYSTICK))
        SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK);
    usbh_core_deinit();

    // Video: no vblank interrupts, and show nxdk's own framebuffer again.
    VIDEOREG(PCRTC_INTR_EN) = 0;
    VIDEOREG(PCRTC_INTR) = 1;
    VIDEOREG(NV_PMC_INTR_EN_0) = 0;
    if (g_flip)
        VIDEOREG(PCRTC_START) = g_physical[0];
}

void Platform_Exit(int code)
{
    // Go straight back to the dashboard. exit() would get there too, but
    // only after running static destructors and SDL_Quit; SDL's Xbox audio
    // shutdown re-runs XAudioInit and waits on the audio thread.
    // Config and high scores are already written and closed by now.
    (void)code;
    quiesceHardware();
    HalReturnToFirmware(HalQuickRebootRoutine);
}

#endif // NXDK
