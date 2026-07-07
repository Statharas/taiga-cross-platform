# UI and integration test driver strategy

## Current test layers

| Layer | Target | What it catches |
| --- | --- | --- |
| Core smoke | `taiga-core-tests` | Parser behavior, service payload conversion, torrent recognition, queue sorting, season DB handoff. |
| Settings inventory | `taiga-settings-dialog-tests` | Settings sections/pages and representative controls. |
| Static UI parity | `taiga-main-ui-parity-tests` | Menu/sidebar/page inventory, icon resources, torrent table inventory, Now Playing idle state. |
| Interaction driver | `taiga-ui-interaction-driver-tests` | Offscreen key-driven UI behavior, starting with shared search mode and Enter submit routing. |

## Driver choice

The best local driver for this Qt application is `Qt6::Test` / `QTest`.

Why:

- It runs offscreen in CI and local terminal builds.
- It can send real key and mouse events to Qt widgets.
- It does not require a browser, accessibility bridge, or a live display server.
- It can test widgets and the real `MainWindow` with the Taiga application
  singleton.

The repo now exposes `Application::setMainWindowForTest()` so tests can build a
real `MainWindow` without taking the single-instance lock or entering the full
event loop.
Tests set `TAIGA_DATA_PATH` to a temporary directory so fixture settings do not
pollute the portable runtime data under `bin/data`.

## New coverage added

`taiga-ui-interaction-driver-tests` currently verifies:

- Search page switches the shared search box to service-search mode.
- Enter on Search submits through `taiga_sync::searchTitle()`.
- Torrents page switches the shared search box to feed-search mode.
- Torrents page has no duplicate local search box.
- Typing in the shared torrent search box does not live-filter the current
  torrent table.
- Enter on Torrents submits through torrent feed search.

The torrent submit test intentionally uses an invalid configured URL to verify
the submit path without depending on network availability.
For offscreen reliability, the test uses `QTest` for typing and invokes the
`QLineEdit::returnPressed` signal directly for submit; this validates the app's
submit wiring without depending on focus behavior from a headless platform
plugin.

## Recommended next driver tests

1. Sidebar click/keyboard navigation should switch pages and update search
   placeholder modes.
2. Anime List keyboard triggers: Enter, Delete, Ctrl+A, configured double-click
   and middle-click actions.
3. Torrent checkbox interactions: click, Shift-click range, context menu actions,
   Ctrl-refresh cache reload.
4. Settings page navigation and persistence: change representative controls,
   reopen settings, assert values persist.
5. Media dialog edit workflow: open details, change status/progress/score, save,
   assert database entry changes.
6. Seasons toolbar: year descending, season change, view/group/sort actions,
   refresh callback success/failure states.
7. Temporary-profile destructive flows: clear history/cache/torrent archive
   without touching the user's real data.
8. Mocked network responses for service search, season fetch, torrent feeds, and
   TokyoToshokan-style `525`/Cloudflare errors.

## CI direction

The existing CTest setup is enough for Linux CI. Windows CI can run the same
tests with the Qt offscreen/minimal platform plugin once the Windows target is
available in the workflow.

For screenshot matching, keep it separate from these driver tests:

- Driver tests should assert behavior and widget state.
- Screenshot tests should compare visual regressions across selected v1/v2
  reference screens.
