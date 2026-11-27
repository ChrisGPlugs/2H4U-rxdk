# 2h4ux-rxdk (2H4U on RXDK + SDL3)

Follow the workspace rules in `../../CLAUDE.md`. Port-specific notes:

- **The game data is committed on purpose** (`data/`). 2H4U and its data are GPL v2+ and the upstream
  repos ship them, so the workspace's "never commit game data" rule (meant for copyrighted assets) does
  not apply here. Don't add any other assets without checking their license.
- `../2H4U-nxdk` is the read-only nxdk reference. Don't edit it.
- Build and boot: `pwsh -File tools/rxdk-run.ps1 -Project ports/2h4ux-rxdk -Seconds 60 -Until '2H4U READY'`
  from the workspace root.
- Layout: game code (`src/*.cpp`, French identifiers, CRLF line endings, mixed Latin-1 and UTF-8; keep
  each file's encoding); port layer: `platform_rxdk.cpp` (video/D3D8, saves, exit, logging),
  `compat.h` (SDL 1.2 names), `image_load.cpp`, `ttf_shim.cpp`, `mixer.cpp` (Mix_* on SDL3 audio streams).
- The title ID 0x32483455 is set by `imageBuild.testId`; saves live in `U:\` (= `E:\UDATA\32483455\`).
