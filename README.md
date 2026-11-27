# 2H4UX: 2H4U for the original Xbox (RXDK + SDL3 port)

**2H4U ("Too Hard For You")** is a mix of Tetris and Breakout by Pierre-Yves Ricau and Pierre Lagouge (2006).
You drop tetrominoes at the top of the screen and keep a ball in play with a paddle at the bottom.
Neobomb ported it to the original Xbox as **2H4UX**, using the Microsoft XDK and SDLx.

This repository builds 2H4UX with the open [RXDK](https://github.com/Team-Resurgent) toolchain and
[RXDK-SDL3](https://github.com/Team-Resurgent/RXDK-SDL3). It started from the nxdk port
([2H4U-nxdk](https://github.com/ChrisGPlugs/2H4U-nxdk) v1.0.0).

## Differences from the nxdk port

- **SDL3 on RXDK.** Video goes through Direct3D 8 and audio through DirectSound on the MCPX APU, which
  are the paths commercial games use. The nxdk-only workarounds are gone: direct CRTC writes, AC97
  register pokes, and the manual quit-to-dashboard sequence.
- **GPU scaling.** The game still draws at 800x600. Each frame is uploaded to a texture and the GPU
  draws it filtered at 4:3, centred on the screen. HD modes and widescreen SD get side bars. Plain SD
  (640x480, or PAL 640x576) fills the screen.
- **Video modes** come from the dashboard settings: 720p, 1080i, or SD (480i/480p, PAL 576i).
  `data/RESOLUTION` can still prefer `480`, `720` or `1080`. RXDK-SDL3 has no 800x600 VGA mode.
- **Saves** go to `U:\`, which the console maps to `E:\UDATA\32483455\`. This is the same title ID as the
  original release and the nxdk port, so existing settings and high scores carry over.
- SDL_image and SDL_ttf aren't available on RXDK, so there are small replacements:
  [stb_image](https://github.com/nothings/stb) for PNG/JPEG and stb_truetype for the two fonts. The
  text is rasterised without hinting, so it can look slightly different from the FreeType builds.

## Controls

| Input | Action |
| --- | --- |
| Triggers / right stick | Move paddle |
| D-pad / left stick left, right | Move tetromino |
| D-pad / left stick down | Drop tetromino faster |
| D-pad up / B | Rotate tetromino |
| A | Fast drop / select |
| Start | Pause |
| Back | Back / exit (B also leaves the Scores screen) |
| Hold LT + RT + Back + Start, or LT + RT + Back + Black | In-game reset (reboot to the dashboard) |

## Building

You need the RXDK tools in `C:\ProgramData\RXDK`, installed by the RXDK VS Code extension
("RXDK: Complete Setup"). You also need RXDK-SDL3 checked out at `../../RXDK-SDL3`, relative to this folder.

```
C:\ProgramData\RXDK\tools\Rxdk.Cli.exe build --project-root .
```

In the RXDK workspace you can use `tools/rxdk-run.ps1` instead:

```
pwsh -File tools/rxdk-run.ps1 -Project ports/2h4ux-rxdk -NoRun                          # build
pwsh -File tools/rxdk-run.ps1 -Project ports/2h4ux-rxdk -Seconds 60 -Until '2H4U READY' # boot in xemu
```

Output:

- `out/Release/Build/2H4UX/`, which holds `default.xbe` and `data/`. Copy that folder to the Xbox.
- `out/Release/XISO/2H4UX.iso`, a disc image.

The debug output (serial on xemu) logs the video mode, any file that fails to load,
`2H4U SCREEN <name>` for each screen, and `2H4U READY` once the first screen is up.

## Credits and license

- Game by Pierre-Yves Ricau and Pierre Lagouge, GPL v2 or later (`COPYING.txt`, `README.txt`).
  The game data in `data/` is part of that GPL release.
- Original Xbox port by Neobomb (`XBOX.README.txt`). nxdk port by ChrisGPlugs.
- CEngine framework by Anders "Sion" Nissen (GFDL 1.2, see `src/Engine.cpp`).
- dr_mp3 by David Reid, and stb_image and stb_truetype by Sean Barrett: public domain / MIT.
- Fonts: see `data/DIGITAL.TXT`.

This repository is distributed under the GPL (see `LICENSE`).
