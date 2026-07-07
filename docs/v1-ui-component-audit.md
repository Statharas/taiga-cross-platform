# v1 UI component audit

This audit maps the original Win32 Taiga UI to the Qt cross-platform UI. The
source of truth is the original code under `src/v1`, especially:

- `src/v1/ui/dlg/dlg_main.cpp`
- `src/v1/ui/dlg/dlg_anime_list.cpp`
- `src/v1/ui/dlg/dlg_history.cpp`
- `src/v1/ui/dlg/dlg_torrent.cpp`
- `src/v1/ui/menu.cpp`
- `src/v1/ui/command.cpp`
- `src/v1/sync/sync.cpp`
- `src/v1/sync/myanimelist.cpp`
- `src/v1/sync/kitsu.cpp`
- `src/v1/sync/anilist.cpp`
- `src/v1/taiga/settings_keys.cpp`

## Global shell

| v1 component | Qt status | Notes |
| --- | --- | --- |
| Top menus: File, Services, Tools, View, Help | Implemented | Main window exposes the original menu strip instead of the earlier icon-only shell. |
| Main toolbar | Implemented | Refresh, folder, export, settings, and right-aligned list/search field are present. |
| Sidebar groups | Implemented | Now Playing, Anime List, History, Statistics, Search, Seasons, Torrents are in the original order. |
| Status bar | Implemented | Main window status bar remains available through View. |
| Shared anime context menu | Partial but reused | Anime List, Search, and Seasons now use the same Qt `MediaMenu`. Some v1 commands still depend on later backend work, such as full now-playing state. |
| Hover feedback | In progress | Qt native widgets cover standard controls. Seasons cards now add hover feedback; remaining custom surfaces should be checked screen-by-screen. |

## Trigger map

| v1 trigger | Source | Qt status |
| --- | --- | --- |
| Toolbar Synchronize | `dlg_main_controls.cpp`, `sync::Synchronize()` | Implemented through `actionSynchronize`. |
| Toolbar Settings | `dlg_main_controls.cpp`, `ShowDialog(Settings)` | Implemented through `actionSettings`. |
| File > Exit | v1 menu command | Implemented through `QApplication::quit`. |
| Services > Synchronize | v1 menu command | Implemented through `actionSynchronize`. |
| Services > Export > Markdown/XML | `ui::ExecuteCommand(ExportAsMarkdown/ExportAsMalXml)` | Implemented through `MainWindow::exportListAsMarkdown/exportListAsXml`. |
| Library > Add folder | `ui::ExecuteCommand(AddFolder)` | Partially implemented: opens a folder picker; watch-folder refresh still needs a backend pass. |
| Library > Scan available episodes | `ui::ExecuteCommand(ScanEpisodesAll)` | Implemented as a Linux scanner pass over configured library folders with persisted per-episode availability. |
| Library > Play next episode | `track::PlayNextEpisodeOfLastWatchedAnime()` | Implemented as `track::playNextEpisodeOfLastWatchedAnime()`. |
| Library > Play random anime | `track::PlayRandomAnime()` | Implemented as `track::playRandomAnime()`. |
| Tools > Enable detection | `ToggleRecognition()` | Implemented as a persisted toggle plus status text. Full media-recognition side effects need a backend pass. |
| Tools > Enable sharing | `ToggleSharing()` | Implemented as a persisted toggle plus status text. Full announce side effects need a backend pass. |
| Tools > Enable synchronization | `ToggleSynchronization()` | Implemented as a persisted toggle plus status text. |
| View > Show Now Playing | v1 View menu | Implemented. |
| View > Show statusbar | v1 View menu | Implemented. |
| Help > Support/Donate/About | v1 Help menu | Implemented. Check-for-updates opens the cross-platform release page when a newer version is available. |
| Anime List double-click | `GetAppListDoubleClickAction()` | Implemented for list and card views: do nothing, edit details, open folder, play next, info, service page. |
| Anime List middle-click | `GetAppListMiddleClickAction()` | Implemented for list and card views with the same action set. |
| Anime List Enter | Simulates configured double-click | Implemented for list and card views. |
| Anime List Delete | `EditDelete()` | Implemented as remove-from-list with confirmation. |
| Anime List right-click | `RightClick`, `Edit`, `EditScore` menus | Mostly implemented through shared `MediaMenu`; header context menu and score-cell-specific popup still need parity work. |
| History right-click | `HistoryList` menu | Partially implemented: details and clear history. |
| History double-click | Opens item details | Implemented through the details action path where history entries resolve an anime item. |
| Seasons refresh | `sync::GetSeason(current_season)` | Implemented for MyAnimeList, Kitsu, and AniList. |
| Seasons right-click | `SeasonList` menu | Implemented through shared `MediaMenu`. |
| Torrents double-click | Open/download selected torrent | Implemented. |
| Torrents toolbar buttons | Check, download/open, discard marked/all, settings | Implemented for the current torrent model. Quick-filter/client-specific v1 commands need more backend work. |
| Settings account buttons | MAL/AniList authorize, Kitsu login | Implemented. |
| Settings library buttons | Add/remove folder, folder double-click | Implemented for UI list; full persistence/watch-folder integration needs backend verification. |
| Settings sharing format buttons | Edit format string | Implemented. |
| Settings torrent filter buttons | Add/edit/remove/reorder/import/export/reset | Implemented for the settings UI model with structured single-condition rules; full multi-condition parity is still a backlog item. |
| Settings advanced/cache buttons | Refresh cache, clear selected cache data | Implemented at UI level; clear behavior depends on current cache backends. |

## Screen map

| Screen | v1 behavior | Qt status |
| --- | --- | --- |
| Now Playing | Shows detected anime poster, current episode, Edit/Share/Watch next links, alternate titles, details, synopsis. | Implemented as a v1-style detail page. Action links are visible; some commands still depend on media/library backend completeness. |
| Anime List | Status tabs, table columns, list filtering, right-click anime menu, column/header menus. | Implemented table and tabs. Shared anime right-click menu exists. Header customization parity still needs a dedicated pass. |
| History | Table of watched items with context actions. | Implemented with a context menu. Needs final comparison against every v1 HistoryList menu command. |
| Statistics | Totals, status counts, score distribution, cache/storage statistics. | Implemented. Visual chart parity is functional rather than pixel-identical. |
| Search | Search page and anime result actions. | Implemented as a table-first searchable list. Remote-service search parity needs a later backend pass. |
| Seasons | Season selector, refresh data, group, sort, view, poster detail tiles, table/details modes, right-click anime actions. | Implemented with active-service refresh, details/table views, shared context menu, card hover, and poster loading. |
| Torrents | Feed refresh, torrent rows, filters, discard/download/open commands, settings shortcut. | Implemented core feed view and actions. Full v1 quick-filter menu and client integration still need deeper backend verification. |
| Settings | Services, Library, Application, Recognition, Sharing, Torrents, Advanced pages. | Implemented inventory and persistence map in `docs/settings-migration-map.md`. |

## Seasons refresh behavior

Original v1 flow:

1. `Season_Load` stores the selected season in `anime::season_db`.
2. If the season database is empty, `sync::GetSeason(current_season)` runs.
3. The main refresh button dispatches by active sidebar page.
4. For Seasons, refresh always calls `sync::GetSeason(anime::season_db.current_season)`.
5. `sync::GetSeason` dispatches to MyAnimeList, Kitsu, or AniList.
6. Each service parses season anime into `anime::season_db.items`.
7. The UI redraws through `OnLibraryGetSeason` and loads posters.

Qt status:

- `sync::fetchSeason` now dispatches to MyAnimeList, Kitsu, and AniList.
- The season database is represented by `anime::season_db`.
- Seasons refresh fetches the selected season and redraws from that database.
- Poster loading is lazy through `ImageProvider` and coalesced to avoid refresh crashes.

## Known parity risks

- Main menu submenus need a command-by-command comparison against v1 menu XML and `ui::Menus`.
- Anime List header context menu and column visibility/order need a dedicated pass.
- Seasons group/sort/view names are implemented but not yet persisted to v1-compatible settings keys.
- Search routes exist for MyAnimeList, Kitsu, and AniList; they still need authenticated live-provider fixtures.
- Torrents need full quick-filter menu parity and client launch behavior verification on Linux.
- Some shared anime menu actions depend on library folder detection, media playback, or service update calls and need live workflow testing.
- Custom Qt surfaces should be checked for hover/selection states against the original Win32 behavior.

## Regression checklist

Before calling a UI parity pass complete:

1. Build `taiga`, `taiga-core-tests`, `taiga-settings-dialog-tests`, and `taiga-main-ui-parity-tests`.
2. Run the full CTest suite.
3. Launch the Linux Qt app.
4. Compare the running Linux UI against the Proton v1 app for each sidebar page.
5. Test each top menu and per-screen right-click menu.
6. Test Refresh on every sidebar page where v1 had page-specific refresh behavior.
7. Test hover/selection feedback on custom card/list/toolbar surfaces.
