/*
    Platform layer for the nxdk port of 2H4U.

    The game always draws into an 800x600 software surface (its artwork is
    authored at that size). This layer owns everything around it: picking a
    video mode that matches the console, scaling the 800x600 frame onto it,
    and where save files live.

    Two implementations:
      platform_xbox.cpp  - nxdk / original Xbox
      platform_host.cpp  - desktop SDL2 build, used to test the port
*/
#ifndef PLATFORM_H
#define PLATFORM_H

#include <SDL.h>

#define GAME_W 800
#define GAME_H 600

// Mount drives / create the save folder. Call before anything touches files.
void        Platform_Init();

// forcedHeight: 0 = pick automatically, or 480 / 720 / 1080 to prefer that mode.
// Returns false if no video mode could be set.
bool        Platform_VideoInit(int forcedHeight);

// Scale the 800x600 game frame onto the screen and show it (waits for vblank).
void        Platform_Present(SDL_Surface* frame);

// Human-readable description of the mode in use, e.g. "1280x720 (scaled 960x720)".
const char* Platform_VideoModeName();

// Full path for a save file (config, high scores). Returns a new string the
// caller may keep.
const char* Platform_SavePath(const char* fileName);

// Leave the game (back to the dashboard on Xbox).
void        Platform_Exit(int code);

#endif
