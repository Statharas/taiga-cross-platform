# Linux migration notes

This branch starts the Linux migration by separating Taiga's reusable C++ code from the current Qt desktop application and by keeping Qt as the cross-platform Linux and Windows UI toolkit.

## Build targets

- `taiga-core`: reusable C++ core library for models, settings, sync services, recognition, media helpers, paths, network, and versioning.
- `taiga`: Qt desktop executable. It remains enabled by default, builds on Linux, and still applies the Windows executable properties on Windows.
- `taiga-core-tests`: native smoke tests for core behavior that should remain stable through the migration.
- `taiga-settings-dialog-tests`: offscreen Qt smoke test for the migrated settings dialog.
- `taiga-main-ui-parity-tests`: offscreen Qt smoke test for the migrated main shell inventory.

## Dependency map

- Qt 6: still used by the C++ core for containers, settings, XML, networking, SQL, and object/event infrastructure. The existing Qt Widgets UI remains isolated in `taiga` and `taiga-gui`.
- anisthesia: media detection dependency. Its Windows media detection sources are already guarded by `CMAKE_SYSTEM_NAME STREQUAL "Windows"`; Linux uses the Qt MPRIS/DBus backend in `src/track/media.cpp`.
- anitomy: anime filename parser, portable C++ dependency.
- monolog: portable logging dependency.
- nstd: portable utility dependency.
- semaver: portable semantic version dependency.
- utf8proc: portable Unicode normalization dependency.
- Win32/DWM APIs: isolated to Windows guarded sources such as `src/gui/platforms/windows.cpp`, the Windows resources file, and Windows-only anisthesia implementations.

## Current boundaries

- Core code that directly referenced GUI model types was left out of `taiga-core` for this first step:
  - `taiga/session.cpp`
  - `media/anime_list_export.cpp`
- The existing Qt executable links those app/UI support files with `taiga-core`, `taiga-gui`, and resources.
- Tests are intentionally small at this stage; they protect the first extracted core seam.
- `docs/v1-ui-component-audit.md` tracks the screen-by-screen UI parity pass
  against the original Win32 implementation, including context menus, hover
  feedback, refresh behavior, and known parity risks.
- `docs/architecture-portability.md` records the Taiga/Anisthesia/Anitomy
  boundary and distro-independent Linux rules for the migration.

## Next migration steps

1. Move session state types out of `gui` so `taiga/session.cpp` can join `taiga-core`.
2. Move list-status formatting out of `gui/utils/format.cpp` so list export can join `taiga-core`.
3. Live-test Linux media detection across native players and browser/streaming providers.
