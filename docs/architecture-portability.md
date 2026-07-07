# Architecture and portability guardrails

This document pins the cross-platform migration to Taiga's published
architecture notes and the local dependency layout.

## Source notes

Taiga's public site describes recognition as two steps:

1. media detection: find active players or browsers and retrieve a filename,
   page title, URL, or equivalent media identity;
2. filename identification: parse that media identity into anime-specific
   elements, then match those elements against Taiga's anime database.

The same page names Anisthesia as the media-detection helper and Anitomy as the
anime filename parser. The migration must preserve that split.

## Dependency roles

| Layer | Dependency | Role | Portability rule |
| --- | --- | --- | --- |
| Media detection model/data | Anisthesia | Player definitions plus media/player/result model types. | Keep portable model parsing usable on every platform. |
| Windows media detection strategy | Anisthesia `win_*` sources | Win32 window enumeration, open file handles, UI Automation. | Compile only on Windows. Never include `win_*` headers from Linux code. |
| Linux media detection strategy | Anisthesia `linux_mpris` backend | MPRIS/DBus adapter that produces Anisthesia media/player-shaped data. | Keep OS detection in Anisthesia; Taiga consumes the shared result API. |
| Filename parsing | Anitomy | Parse filenames/titles into anime elements. | Treat as platform independent; no Win32 assumptions in Taiga wrappers. |
| Anime identification | Taiga recognition/cache/db | Normalize Anitomy output and match against list/search metadata. | Shared core logic, no platform UI or OS APIs. |

## Current local state

- `deps/anitomy` is platform-neutral C++23 and should remain a pure parser
  dependency.
- `deps/anisthesia` exposes portable player/media/result types and parser
  sources. Its public cross-platform entry point is now
  `anisthesia::GetResults`.
- `deps/anisthesia/CMakeLists.txt` compiles `win_*` sources only when
  `CMAKE_SYSTEM_NAME STREQUAL "Windows"` and compiles the Linux MPRIS backend
  only when Qt DBus is available.
- `src/track/media.cpp` uses Anisthesia's shared result API on all platforms.
  It no longer knows about Win32 handles or Linux DBus metadata.
- Qt DBus is now required/linked only when the target platform is not Windows.
- Data paths use `QStandardPaths::AppDataLocation` with a home-directory
  fallback, so Linux builds follow the user's XDG-compatible writable app-data
  location instead of distro-specific hardcoded paths.

## Distro independence rules

- Do not rely on distro-specific absolute paths such as `/usr/bin/mpv`,
  `/usr/share`, or package-manager-specific locations.
- Use Qt resource files for bundled data such as `players.anisthesia`.
- Use `QStandardPaths` for writable app data, config, cache, downloads, and
  desktop integration paths.
- Keep media-player detection behind Anisthesia platform backends:
  - Windows: Anisthesia Win32 strategies.
  - Linux: Anisthesia MPRIS/DBus first, future optional adapters behind CMake
    feature checks.
- Avoid mandatory runtime dependencies outside Qt, standard desktop portals,
  DBus/MPRIS, and bundled libraries unless they are guarded by feature options.
- Prefer AppImage/Flatpak-style packaging assumptions for Linux distribution:
  resources travel with the app, writable state goes to XDG locations, and
  external media players are discovered at runtime.

## Migration guardrails

- Anitomy must not depend on Qt, Win32, DBus, or UI code.
- Anisthesia `win_*` headers must not leak into Linux compilation units.
- Taiga recognition must consume generic media facts, not OS handles or window
  objects.
- Tests should cover parser and recognition behavior without requiring a live
  desktop session.
- Live media-detection tests should be separate smoke tests because they depend
  on the user's desktop session and running media players.

## Open work

- Expand the Anisthesia Linux backend beyond MPRIS only if needed, while keeping
  the output in Anisthesia-compatible media/player terms.
- Add a compile-time portability check that fails if Linux sources include
  Anisthesia `win_*` headers or `<windows.h>`.
- Add packaging notes for AppImage and Flatpak once the UI parity pass settles.
- Add a live MPRIS smoke test fixture that can be skipped when no session bus is
  available.
