# Screen component parity map

This map compares each original Win32 v1 screen/component to the Qt
cross-platform UI. It is intended to be the working checklist for visual,
behavioral, and trigger parity.

Sources used for v1 comparison:

- `src/v1/ui/dlg/dlg_main.cpp`
- `src/v1/ui/dlg/dlg_anime_list.cpp`
- `src/v1/ui/dlg/dlg_history.cpp`
- `src/v1/ui/dlg/dlg_settings.cpp`
- `src/v1/ui/dlg/dlg_torrent.cpp`
- `src/v1/ui/menu.cpp`
- `src/v1/ui/command.cpp`
- `src/v1/track/feed_aggregator.cpp`
- `src/v1/track/feed_source.cpp`
- `src/v1/track/feed_filter_manager.cpp`
- `src/v1/taiga/settings_keys.cpp`

## Global shell

| Component | v1 behavior | Qt v2 status | Follow-up |
| --- | --- | --- | --- |
| Window title/icon | Taiga title, active anime suffix, Taiga cat icon | Implemented | Keep comparing title state while Now Playing changes. |
| Menu bar | File, Services, Tools, View, Help | Implemented | Command-by-command smoke testing remains useful after backend work. |
| Toolbar | Refresh/sync, folder, export, settings, search box | Implemented | Refresh dispatch should continue to route by active page. |
| Sidebar order | Now Playing, Anime List, History, Statistics, Search, Seasons, Torrents | Implemented | Selection highlight is native Qt; keep Win32-like contrast. |
| Status bar | Page and command feedback, list update countdown | Implemented | Torrent transfer progress is not yet as detailed as v1. |
| Search field | Placeholder changes by page/service | Implemented | Check every page placeholder during manual passes. |

## Now Playing

| Component | v1 behavior | Qt v2 status | Follow-up |
| --- | --- | --- | --- |
| Idle state | Blank detail area until detection | Implemented | Covered by UI parity test. |
| Poster | Shows matched anime poster | Implemented | Depends on image cache/network availability. |
| Title header | Blue title link, episode in window title | Implemented | Verify title suffix while playback changes. |
| Current episode text | `Now playing: Episode n by group` | Implemented | Depends on Anisthesia and recognition bridge. |
| Action links | Edit, Share, Watch next episode | Implemented UI | Share/play actions depend on sharing/library backends. |
| Alternative titles | Section with title aliases | Implemented | Verify title language preference. |
| Details | Type, episodes, status, season, genres, producers, score | Implemented | Ensure service-specific missing fields stay blank, not bogus. |
| Synopsis | Text block with source | Implemented | Needs live service data coverage. |
| Context menu | Shared anime actions | Implemented through shared media menu | Keep in sync with Anime List and Seasons. |

## Anime List

| Component | v1 behavior | Qt v2 status | Follow-up |
| --- | --- | --- | --- |
| Status tabs | Watching, Completed, On hold, Dropped, Plan to watch with counts | Implemented | Counts follow loaded account data. |
| Status icon column | Small colored square by airing status | Implemented | Covered by UI parity test. |
| Columns | Title, progress, score, type, season, last updated | Implemented | Header context/column visibility still needs a dedicated pass. |
| Progress bar | Filled only when total episode count is known | Implemented | Core test guards `0/?` from arbitrary fill. |
| Score display/editor | Score value or dash | Implemented display | Cell-specific score popup parity remains a risk. |
| Filtering | Main search field filters list | Implemented | Verify against service display names and alt titles. |
| Sorting | Column sorting | Implemented | Numeric/date sorting should be compared against v1. |
| Double click | Configured action from settings | Implemented | Covered by action setting wiring, needs workflow spot checks. |
| Middle click | Configured action from settings | Implemented | Needs live mouse/manual verification. |
| Enter/Delete | Enter triggers default action, Delete removes entry | Implemented | Delete uses confirmation. |
| Right-click menu | Details, edit, remove, open folder, play, torrents, external | Mostly implemented through shared media menu | Header and score-cell menus are still separate parity work. |

## History

| Component | v1 behavior | Qt v2 status | Follow-up |
| --- | --- | --- | --- |
| History table | Watched anime/episode/group/date rows | Implemented | Verify exact column set against populated v1 history. |
| Double click | Opens resolved anime details | Implemented where entry resolves | Need fixture with known history entries. |
| Right-click menu | Details and history management actions | Partial | Compare every `HistoryList` menu action after backend history import. |
| Clear history | Removes history entries | Implemented UI | Needs destructive-action workflow test with temp profile. |

## Statistics

| Component | v1 behavior | Qt v2 status | Follow-up |
| --- | --- | --- | --- |
| List totals | Counts by list status | Implemented | Compare against same imported list. |
| Time/episode totals | Watched episode/time summaries | Implemented | Service data dependent. |
| Score distribution | Score buckets | Implemented | Visual style is functional, not pixel-identical. |
| Cache/storage totals | Database, image, torrent cache totals | Implemented | Cache paths differ by platform but should report accurately. |

## Search

| Component | v1 behavior | Qt v2 status | Follow-up |
| --- | --- | --- | --- |
| Search field | Service search placeholder and query | Implemented | Verify MAL/Kitsu/AniList request differences. |
| Filters | Year, season, type, status | Implemented | Year list is descending. |
| Result table/cards | Anime result list with poster/details | Implemented | Needs service result fixture screenshots. |
| Sort/view controls | Table/card mode and sort controls | Implemented | Persisted settings parity should be checked. |
| Right-click menu | Shared anime/result actions | Implemented through shared media menu | External links depend on provider IDs. |

## Seasons

| Component | v1 behavior | Qt v2 status | Follow-up |
| --- | --- | --- | --- |
| Season toolbar | Season/year selector, refresh, group, sort, view | Implemented | Year list is descending. |
| Refresh data | Calls active service season endpoint | Implemented for MAL, Kitsu, AniList | Covered by core season handoff test; live credentials still needed. |
| Details cards | Poster, title, aired, episodes, genres, producers, score, popularity, synopsis | Implemented | Image loading depends on cache/network. |
| Table view | Columns comparable to search/list data | Implemented | Compare with v1 Details/Table switch. |
| Grouping | Group by Type/none | Implemented | Persist exact v1 setting keys if needed. |
| Sorting | Title/score/etc. | Implemented | Numeric sort spot checks needed. |
| Hover feedback | Win32 highlight on rows/cards | Implemented for custom cards, native for widgets | Keep visual QA pass. |
| Right-click menu | Details, search, external, edit, remove, open folder, play, torrents | Implemented through shared media menu | Menu outline is Qt native; verify theme contrast. |

## Torrents

| Component | v1 behavior | Qt v2 status | Follow-up |
| --- | --- | --- | --- |
| Toolbar | Check new, download marked, discard all, settings | Implemented | Auto-check countdown now updates the check action text. |
| Search slot | Top-right shell search changes to `Search for torrents` | Implemented | The torrent page intentionally has no local search field. |
| Fetch | 30s timeout, RSS Accept, user-agent, proxy/cert settings | Implemented | Transfer progress text is still simpler than v1. |
| Ctrl-refresh | Loads cached `feed.xml` | Implemented | Needs manual Ctrl-click verification. |
| Feed cache | Saves successful feed per source | Implemented | Cache path is platform data path. |
| Source parsing | TokyoToshokan, Nyaa, AnimeTosho, AniDex/Minglong/SubsPlease metadata | Implemented core coverage | More real feeds should be fixture-tested. |
| Recognition | Anitomy extracts title, episode, group, resolution, anime ID | Implemented | Core test guards the basic path. |
| Groups | Anime, Batch, Other | Implemented | Hidden/archived groups are skipped. |
| Columns | Title, episode, group, size, video, S/L/D, description, filename, release date | Implemented | Column sort types are not yet all v1-specific. |
| Checkboxes | Mark/select rows, Shift range by group | Implemented | Manual range-selection check still needed. |
| Status icons | Anime airing state square | Implemented | Depends on matched anime ID. |
| Double-click/Enter | Download/open selected queue | Implemented | Manual shortcut check still useful. |
| Context menu | Download, anime/torrent info, discard, prefer group, more torrents, search service | Mostly implemented | Anime info command currently maps through shared screens, not a dedicated torrent submenu. |
| Download queue | Sort marked items by configured order | Implemented | Core test covers episode order. |
| Magnet links | Opens magnet when setting is enabled | Implemented | Core test covers preference. |
| `.torrent` files | Downloads, saves, archives, optionally opens app | Implemented | Custom client path/app mode needs Linux workflow testing. |
| Filters | Named defaults plus JSON-backed custom list | Partial | Full v1 filter editor/action/operator model remains the biggest torrent gap. |

## Settings

| Component | v1 behavior | Qt v2 status | Follow-up |
| --- | --- | --- | --- |
| Services/Main | Active service/provider and auto-sync startup | Implemented | Startup auto-sync needs live app-start verification. |
| Services/MAL | Username, authorize, account link | Implemented | OAuth flow opens browser and persists token. |
| Services/Kitsu | Email/password, account link/profile URL note | Implemented | Kitsu login/token refresh should be live-tested. |
| Services/AniList | Username, authorize, account link | Implemented | OAuth flow opens browser and persists token. |
| Library/Folders | Folder list, add/remove, drag/drop hint, monitor checkbox | Implemented UI | Filesystem watcher scan needs backend workflow test. |
| Application/Anime list | Double/middle click, title language, highlight, progress options | Implemented | Check action parity with Anime List. |
| Application/General | Startup, updates, scan at startup, tray, external links | Implemented UI | Windows-only startup label should remain platform-neutral where possible. |
| Recognition | Media detection interval, ignored strings, folders | Implemented UI | Anisthesia backend behavior is platform bridge dependent. |
| Sharing/Discord | Rich presence and display options | Implemented UI | Discord IPC backend needs Linux runtime validation. |
| Sharing/HTTP | POST URL and format string | Implemented UI | Request dispatch needs end-to-end test. |
| Sharing/mIRC | Message, server, channel, action options | Implemented UI | Windows DDE/mIRC behavior needs platform replacement strategy. |
| Torrents/Discovery | Source/search URLs, auto-check, action | Implemented | Auto-check now exists in Torrents widget. |
| Torrents/Downloads | Queue sort, download folder, open app, magnet options | Implemented UI/core | Custom client launch still needs Linux workflow test. |
| Torrents/Filters | Enable, archive limit, add/remove/reorder/import/export/reset | Implemented UI model | Full condition/action parity still pending. |
| Advanced/Settings | Editable advanced key/value list | Implemented | Keep key names mapped in `settings-migration-map.md`. |
| Advanced/Cache | Clear history/images/torrents/torrent history | Implemented UI | Verify deletion paths with temporary profile. |

## Shared dialogs and menus

| Component | v1 behavior | Qt v2 status | Follow-up |
| --- | --- | --- | --- |
| Anime/media details dialog | Poster, metadata, list entry edits, external links | Implemented | Compare every edit field with v1 dialog. |
| Account authorization dialogs | Browser/token flow for MAL/AniList, password/token for Kitsu | Implemented | Needs live login checks for all three providers. |
| Shared anime context menu | Details/search/external/edit/remove/open/play/torrents | Implemented | Ensure every screen uses the same action availability rules. |
| Hover/selection states | Win32 list and menu hover outlines | Native Qt plus custom card hover | Menu outline follows current Qt style; theme contrast should be visually checked. |

## Regression coverage

- `taiga-main-ui-parity-tests` covers the global shell, page inventory, sidebar,
  status icon column, torrent toolbar/table inventory, and Now Playing idle
  behavior.
- `taiga-settings-dialog-tests` covers settings section/page inventory and key
  controls from every v1 settings page.
- `taiga-core-tests` covers service parsers, season handoff, unknown episode
  progress, torrent RSS parsing, Nyaa metadata, Anitomy torrent recognition,
  torrent queue sort, and magnet preference.
