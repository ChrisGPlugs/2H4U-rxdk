/*
    Platform layer for the RXDK port of 2H4U.

    The game always draws into an 800x600 software surface (its artwork is
    authored at that size). This layer owns everything around it: picking a
    video mode that matches the console, showing the 800x600 frame on it at
    4:3, where save files live, debug output and leaving the game.

    Implementation: platform_rxdk.cpp (RXDK-SDL3 on the original Xbox).
*/
#ifndef PLATFORM_H
#define PLATFORM_H

#include <SDL3/SDL.h>
#include <stdio.h>

#define GAME_W 800
#define GAME_H 600

// The game's screen surface is 32-bit XRGB, the format of the console's
// back buffer and of the texture it is shown through.
#define COMPAT_SCREEN_FORMAT SDL_PIXELFORMAT_XRGB8888

// Call before anything touches files.
void        Platform_Init();

// forcedHeight: 0 = pick automatically, or 480 / 720 / 1080 to prefer that mode.
// Returns false if no video mode could be set.
bool        Platform_VideoInit(int forcedHeight);

// Show the 800x600 game frame at 4:3 on the screen (waits for vblank).
void        Platform_Present(SDL_Surface* frame);

// Human-readable description of the mode in use, e.g. "1280x720 (picture 960x720)".
const char* Platform_VideoModeName();

// Full path for a save file (config, high scores). Returns a new string the
// caller may keep.
const char* Platform_SavePath(const char* fileName);

// Make sure a save file just written and closed is on the disk.
void        Platform_CommitSave(const char* path);

// Leave the game (back to the dashboard).
void        Platform_Exit(int code);

// In-game reset (IGR). Call once per frame with the open gamepad (may be NULL).
// LT + RT + Back + Start, or LT + RT + Back + Black, reboots the console to
// the dashboard and does not return. Returns true while LT and RT are both
// held, so the caller can hold back Start/Back (pause, exit) during the combo.
bool        Platform_CheckIGR(SDL_Gamepad* pad);

// printf-style line to the debug output (serial on xemu).
void        Platform_Log(const char* fmt, ...) SDL_PRINTF_VARARG_FUNC(1);

// fprintf replacement used by compat.h: stderr/stdout go to the debug output.
int         Platform_FPrintf(FILE* f, const char* fmt, ...) SDL_PRINTF_VARARG_FUNC(2);

// Call when the game switches to a new screen (launcher, intro, menu...).
// Once its first frame has been presented, "2H4U SCREEN <name>" is logged,
// and the first time also "2H4U READY", which tools/rxdk-run.ps1 waits for.
void        Platform_ScreenShown(const char* name);

#endif
