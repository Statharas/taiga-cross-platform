# v1 screen requirements and v2 comparison

This document turns the original Win32 UI into migration requirements. It is
broader than the component checklist: it names each screen, view, dialog, menu,
and submit flow that should survive the Qt/Linux migration.

Primary v1 sources:

- `src/v1/ui/dlg/dlg_main.*`
- `src/v1/ui/dlg/dlg_anime_list.*`
- `src/v1/ui/dlg/dlg_history.*`
- `src/v1/ui/dlg/dlg_stats.*`
- `src/v1/ui/dlg/dlg_torrent.*`
- `src/v1/ui/dlg/dlg_settings.*`
- `src/v1/ui/dlg/dlg_settings_page.*`
- `src/v1/ui/dlg/dlg_settings_advanced.*`
- `src/v1/ui/dlg/dlg_feed_filter.*`
- `src/v1/ui/dlg/dlg_feed_condition.*`
- `src/v1/ui/dlg/dlg_format.*`
- `src/v1/ui/dlg/dlg_update.*`
- `src/v1/ui/menu.cpp`
- `src/v1/ui/command.cpp`
- `src/v1/ui/list.cpp`

## Requirement status

- `Done`: v2 has equivalent UI and behavior.
- `Partial`: v2 has the visible surface, but behavior is incomplete or not
  proven with tests.
- `Missing`: v2 does not expose an equivalent yet.
- `Deferred`: v1 behavior is Windows-specific and needs a cross-platform
  replacement design.

## Global shell

| Requirement | V1 behavior | V2 status | Evidence / action |
| --- | --- | --- | --- |
| Shell menu bar | File, Services, Tools, View, Help with command menus. | Done | `MainWindow`, `taiga-main-ui-parity-tests`. |
| Main toolbar | Sync, folder menu, export menu, settings, debug in debug builds, search box. | Partial | Main actions exist; debug button and folder/export submenu fidelity need a command pass. |
| Sidebar navigation | Now Playing, Anime List, History, Statistics, Search, Seasons, Torrents. | Done | `NavigationWidget`, parity test. |
| Page-specific refresh | Refresh dispatches by active page. | Partial | Sync/list and Seasons/Torrents are handled; History/Stats/Search refresh commands need explicit parity checks. |
| Shared search box | Mode changes by page: service search, feed search, list filter, or none. Enter submits in service/feed mode. | Done | `MainWindow::updateSearchBoxForPage`, `taiga-ui-interaction-driver-tests`. |
| Back/forward navigation | V1 tracks page history. | Done | V2 page history is wired to Back/Forward actions and covered by the interaction driver. |
| Status bar | Shows command progress, selection summaries, update timers, feed errors. | Partial | Present, but torrent transfer progress and some command statuses are simpler. |
| Tray integration | Show/hide, tray menu, notification click routes. | Partial | Tray exists; notification click routes and taskbar-specific behavior are not all ported. |
| Single-instance behavior | Existing instance is activated. | Partial | Lock exists; activation is TODO. |
| Drag/drop files | Debug recognition preview. | Missing | Debug-only v1 behavior not ported. |

## Now Playing

| Requirement | V1 behavior | V2 status | Evidence / action |
| --- | --- | --- | --- |
| Idle view | Shows blank page when no media is recognized. | Done | `taiga-main-ui-parity-tests`. |
| Detected media details | Poster, title, now playing episode/group, action links, alt titles, metadata, synopsis. | Partial | UI exists; live behavior depends on Anisthesia bridge and media recognition completeness. |
| Edit action | Opens anime/list edit dialog for current anime. | Partial | Shared dialog exists; now-playing edit workflow needs live test. |
| Share action | Announces current episode via enabled sharing providers. | Partial | Sharing settings exist; sharing backends are not fully validated. |
| Watch next action | Plays next episode from library folders. | Partial | Play helper exists; scanner/library folder workflow needs end-to-end test. |
| Taskbar notification route | Clicking now-playing notification opens Now Playing. | Missing | Tray notification routes not implemented. |

## Anime List

| Requirement | V1 behavior | V2 status | Evidence / action |
| --- | --- | --- | --- |
| Status tabs | Watching, Completed, On hold, Dropped, Plan to watch counts. | Done | `ListWidget`. |
| Table columns | Icon, title, progress, score, type, season, last updated. | Done | `AnimeListModel`, parity test. |
| Progress rendering | Unknown totals show `0/?` without arbitrary fill. | Done | `taiga-core-tests`. |
| Card/details view | V1 list can use alternate list/card-like views. | Partial | v2 has list/card delegates; view switch persistence needs more tests. |
| Filtering | Shared search filters list and can submit service search. | Done | `ListViewBase`, driver test covers submit mode. |
| Sorting | Header sorting with numeric/date-aware behavior. | Partial | Basic sorting exists; exact v1 comparators need fixtures. |
| Header context menu | Column visibility/order and list options. | Partial | Column visibility context menu is implemented; persistent order/list-option parity remains. |
| Score cell menu | Right-click score cell opens score-specific menu. | Missing | Shared media menu does not replace cell-specific popup. |
| Double/middle click actions | Settings determine action: none/edit/folder/play/info/service page. | Partial | Implemented; needs driver tests for all configured values. |
| Keyboard actions | Enter default action, Delete remove, Ctrl+A select all. | Partial | Delete/Enter paths exist; broader shortcuts need driver tests. |
| Shared media menu | Details, search, external, edit, remove, open folder, play, torrents. | Partial | Shared menu exists; action availability rules need exact v1 comparison. |

## History

| Requirement | V1 behavior | V2 status | Evidence / action |
| --- | --- | --- | --- |
| History table | Watched item rows with anime, episode, date/time, player info. | Partial | `HistoryWidget` exists; exact column parity needs populated fixtures. |
| Double-click | Opens details for matching anime. | Partial | Depends on resolved anime IDs. |
| Context menu | Details, search, delete/clear history actions. | Partial | Some actions exist; menu inventory needs direct v1 comparison. |
| History counter | Sidebar history counter refreshes. | Missing | V2 sidebar does not yet show v1 counter behavior. |
| Clear history | Deletes selected/all history. | Partial | UI exists; destructive path needs temp-profile test. |

## Statistics

| Requirement | V1 behavior | V2 status | Evidence / action |
| --- | --- | --- | --- |
| List totals | Counts by status and totals. | Done | `StatisticsWidget`. |
| Watched time | Episodes/time watched summaries. | Partial | UI exists; needs fixture parity against v1 formulas. |
| Score distribution | Score buckets/averages. | Partial | Functional, not pixel-identical. |
| Cache stats | Database, image, torrent, history counts/sizes. | Partial | Paths differ cross-platform; cache calculations need tests. |

## Search

| Requirement | V1 behavior | V2 status | Evidence / action |
| --- | --- | --- | --- |
| Search submit | Enter in shared search runs active service title search. | Done | AniList, MyAnimeList, and Kitsu search routes are wired; live-provider fixtures remain useful. |
| Result list | Service results are added to DB and shown in Search page. | Partial | AniList updates DB; Search page filtering/result refresh needs a live or mocked test. |
| Filters | Year, season, type, status. | Done | Year list is descending. |
| Sort/view controls | Sort and view controls available. | Partial | UI exists; persistence/actions need tests. |
| Result context menu | Shared media actions for results. | Partial | Shared menu exists; provider links need service IDs. |

## Seasons

| Requirement | V1 behavior | V2 status | Evidence / action |
| --- | --- | --- | --- |
| Toolbar | Select season/year, refresh data, group, sort, view. | Done | `SeasonsWidget`. |
| Year ordering | Recent/future years first. | Done | Descending year picklist. |
| Refresh | Calls active service season endpoint. | Done | MAL/Kitsu/AniList fetch paths exist; core test covers DB handoff. |
| Details view | Poster cards with aired, episodes, genres, producers, score, popularity, synopsis. | Partial | Implemented; image/network/live data still need screenshot parity. |
| Table view | Alternate table/details modes. | Partial | Implemented; exact column/view switch persistence needs tests. |
| Group/sort | Group by type/season and sort choices. | Partial | Basic controls exist; exact v1 settings keys/persistence need tests. |
| Context menu | Same anime actions as list/search. | Partial | Shared menu exists. |

## Torrents

| Requirement | V1 behavior | V2 status | Evidence / action |
| --- | --- | --- | --- |
| Toolbar | Check new torrents, download marked, discard all, settings. | Done | `TorrentsWidget`, parity tests. |
| Shared search submit | Enter searches configured torrent search URL and replaces feed results. | Done | `taiga-ui-interaction-driver-tests`. |
| Fetch headers/timeouts | RSS Accept header, user-agent, 30 second timeout, proxy/cert settings. | Partial | Headers/timeout done; proxy/cert settings still TODO. |
| TokyoToshokan failures | Cloudflare/server failures such as `525` must be reported as server errors, not masked as local timeout. | Partial | Timeout is fixed; error wording should include HTTP status when provided. |
| Ctrl-refresh | Loads cached `feed.xml`. | Done | Needs driver/manual shortcut test. |
| Feed cache | Saves successful feed per source. | Done | `track::torrent`. |
| Source parsers | TokyoToshokan, Nyaa, AnimeTosho, AniDex, Minglong, SubsPlease. | Partial | Implemented core cases; needs real-feed fixtures. |
| Recognition | Anitomy recognition extracts title, episode, group, resolution, anime ID. | Done | `taiga-core-tests`. |
| Filters | Select/prefer/discard rules, hidden/inactive/archive states, full condition editor. | Partial | Named/default filters and a structured action-field-condition-value editor exist; multi-condition/operator parity remains. |
| Groups/columns | Anime/Batch/Other groups and all v1 columns. | Done | `TorrentsWidget`, parity tests. |
| Checkbox range | Shift-click range in same group. | Done | Needs interaction test. |
| Context menu | Download, anime info, torrent info, discard, discard anime, prefer group, more torrents, search service. | Partial | Most actions exist; anime-info-specific action and exact menu order need refinement. |
| Download queue | Queue sorted by configured order, magnet support, `.torrent` save/open/archive. | Partial | Core behavior implemented; custom client path/app mode needs Linux workflow testing. |
| Auto-check | Timer checks feed and updates countdown. | Done | Needs timer-controlled test. |

## Settings

| Requirement | V1 behavior | V2 status | Evidence / action |
| --- | --- | --- | --- |
| Services pages | Main/MAL/Kitsu/AniList auth and account controls. | Partial | UI exists; live auth flows need tests. |
| Library page | Folder list, add/remove, drag/drop, monitor checkbox. | Partial | UI exists; real watcher/scanner path needs backend tests. |
| Application pages | List actions/appearance/progress, startup/tray/links. | Partial | UI exists; startup/tray platform behaviors need Linux-specific implementation. |
| Recognition page | Media players, streaming media, ignored strings, interval, folders. | Partial | UI exists; Anisthesia bridge still evolving. |
| Sharing pages | Discord, HTTP POST, mIRC/DDE settings and format editors. | Partial/Deferred | HTTP/Discord need runtime tests; mIRC DDE is Windows-specific and needs cross-platform strategy. |
| Torrent pages | Discovery, downloads, filters. | Partial | UI exists; structured filter editor exists; custom client launch and full v1 multi-condition parity remain. |
| Advanced pages | Advanced key/value settings and cache clear. | Partial | UI exists; clear paths need temp-profile tests. |

## Auxiliary dialogs

| Requirement | V1 behavior | V2 status | Evidence / action |
| --- | --- | --- | --- |
| Anime information / media dialog | Details/list editing/external links/poster. | Partial | `MediaDialog` exists; field-by-field v1 comparison still needed. |
| Feed filter dialog | Full filter editor with conditions/actions/operators. | Partial | Current settings UI supports add/edit for single structured condition rows. |
| Feed condition dialog | Add/edit a single torrent filter condition. | Partial | Action, field, match operator, and value are editable; every v1 operator/action still needs direct mapping. |
| Format string dialog | Edit HTTP/mIRC/notification templates. | Partial | Format editor UI exists; variable preview/testing needs parity check. |
| Update dialog | Check/update workflow. | Partial | Cross-platform release-page check is available; apply/download workflow is not ported. |
| About dialog | About/support/donate links. | Done | `AboutDialog`. |
