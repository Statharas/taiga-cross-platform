# UI parity map

This document tracks the v1 Win32 screens against the Qt cross-platform shell.
The comparison source is the v1 dialog code under `src/v1/ui/dlg`, plus the
season settings and menu commands that remain in `src/v1/taiga/settings_keys.*`
and `src/v1/ui/command.cpp`.

For the component-by-component checklist, see
`docs/screen-component-parity-map.md`.

| v1 screen or shell part | Qt migration status |
| --- | --- |
| Main menu | Present: File, Services, Tools, View, Help. Services keeps sync/export actions; View exposes Now Playing and status bar toggles. |
| Main toolbar | Present: icon-only sync, folder add, export, settings, and right-aligned list/search field, matching the v1 shell density. |
| Sidebar | Present: Now Playing, Anime List, History, Statistics, Search, Seasons, Torrents with the v1 grouping order. |
| Now Playing | Present as a v1-style detected-media detail page with poster, current episode text, action links, alternative titles, details, and synopsis. When no media is detected, the page stays blank like v1 instead of showing orphan detail headers. |
| Anime List | Present as a table-first page with v1-style status tabs, list/search filtering, and the v1 status icon column. Status icons follow the original mapping: green airing, blue finished, red not-yet-aired, gray unknown. Unknown episode totals render as `0/?` without a misleading percentage fill. |
| History | Present through the existing Qt history widget. |
| Statistics | Present: list totals, watched episodes/time, mean score, status counts, score distribution, database/image/torrent cache totals. |
| Search | Present as a table-first searchable list view. |
| Seasons | Present: v1-style season toolbar, poster detail cards grouped by type/season, refresh through the active service, sort/view controls, table fallback, hover feedback, and shared anime right-click menu. |
| Torrents | Present: v1 torrent columns, checkable rows, v1 toolbar icon/text actions, feed refresh, marked download/open, discard marked/all, torrent search placeholder, settings shortcut, row status icons, and a right-click menu for the primary torrent actions. |
| Settings | Present: all 7 sections and 18 v1 pages, with account authorization and torrent/cache backends mapped in `docs/settings-migration-map.md`. |

## Regression coverage

- `taiga-main-ui-parity-tests` checks the main menu, page count, sidebar
  inventory, imported v1 icon resources, anime-list status indicator column,
  torrent toolbar/search/table inventory, and the quiet Now Playing idle state
  against the v1 shell.
- `taiga-settings-dialog-tests` checks the settings section/page inventory and
  representative controls from every v1 settings section.
- `taiga-core-tests` covers parser/core behavior used by sync and torrents.
  It also checks the season database handoff used by the Seasons refresh path.
