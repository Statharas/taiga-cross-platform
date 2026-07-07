# Settings migration map

This document maps the original Win32 settings dialog to the Qt settings dialog.
The v1 source of truth is:

- `src/v1/ui/dlg/dlg_settings.cpp`: section/page routing and save behavior.
- `src/v1/ui/dlg/dlg_settings_page.cpp`: per-page controls and command handlers.
- `src/v1/ui/dlg/dlg_settings_advanced.cpp`: editable advanced setting list.
- `src/v1/taiga/settings_keys.h` and `src/v1/taiga/settings_keys.cpp`: setting keys and defaults.

## Dialog shape

The original dialog has 7 sections and 18 pages. The Qt dialog keeps the same
shape:

| v1 section | v1 pages | Qt status |
| --- | --- | --- |
| Services | Main, MyAnimeList, Kitsu, AniList | Present as Services / Main, MyAnimeList, Kitsu, AniList |
| Library | Folders | Present |
| Application | Anime list, General | Present as Application / General and Anime List |
| Recognition | General, Media players, Streaming media | Present |
| Sharing | Discord, HTTP, mIRC | Present |
| Torrents | Discovery, Downloads, Filters | Present |
| Advanced | Settings, Cache | Present |

## Page inventory

| v1 page | Original components | Qt migration |
| --- | --- | --- |
| Services / Main | Active service combo; login on startup checkbox | Present; persisted through `v1.service` and `account.sync.autoLogin` |
| Services / MyAnimeList | Username field; authorize/re-authorize button; access/refresh token settings | Present; opens MAL OAuth page, accepts pasted code/redirect URL, exchanges it for tokens, and looks up username |
| Services / Kitsu | Email field; password field; hidden advanced partial-library/rating settings | Present; logs in with Kitsu password grant, saves access token, and looks up username/display name |
| Services / AniList | Username field; authorize/re-authorize button; token/rating settings | Present; opens AniList OAuth page, accepts pasted token/redirect URL, verifies token, and looks up username |
| Library / Folders | Watch folders checkbox; folder list; add/remove; drag/drop; double-click open | Present; watch checkbox, add/remove, drag/drop folder add, and double-click folder open work cross-platform |
| Application / General | Autostart; start minimized; close/minimize to tray; startup update/episode checks; external links | Present; also exposes proxy/reuse/no-revoke values that v1 kept in Advanced |
| Application / Anime list | Double/middle click actions; title language; highlight options; progress bars | Present |
| Recognition / General | Update confirmation/player/range/root/wait options; update delay; notification and navigation options | Present; detection interval, ignored strings, parent-directory lookup exposed too |
| Recognition / Media players | Detect media players; checked supported-player list | Present; populated from `players.anisthesia`; select-all/clear-all controls added |
| Recognition / Streaming media | Detect streaming media; checked provider list; provider double-click link in v1 | Present as checked provider list with select-all/clear-all controls |
| Sharing / Discord | Enable; username/group/time toggles; advanced application ID | Present; application ID exposed in Advanced |
| Sharing / HTTP | Enable; URL; format button | Present; URL and format persist; format button opens an editor for the format field |
| Sharing / mIRC | Enable; service; target mode; multi-server; `/me`; channels; test DDE; format button | Present; settings persist; format button opens an editor; Linux uses a configurable external command instead of Windows DDE |
| Torrents / Discovery | Feed source presets; search URL presets; auto-check; interval; new torrent action | Present; source/search preset dropdowns are editable and auto-check controls enable/disable dependents |
| Torrents / Downloads | Queue sort; anime folder fallback; download folder browse; subfolder; open app; app mode/path browse; magnet setting | Present; folder/file browse controls are cross-platform and dependent controls follow their toggles |
| Torrents / Filters | Enable; filter list; add/edit/remove/move/import/export/reset toolbar; archive limit | Present; filter rows are saved to Qt settings as JSON, can be imported/exported/reset, and structured single-condition rules can be edited |
| Advanced / Settings | Editable list of 18 advanced values | Present as editable grouped controls |
| Advanced / Cache | History, image cache, torrent files, torrent archive counts; clear selected | Present for history, poster cache, torrent files, and the v1 torrent archive file |

## Backend migration notes

The Qt dialog now opens without crashing and persists all currently modeled
settings. Account authorization has been mapped from the original Win32 flows:

- MyAnimeList opens `https://myanimelist.net/v1/oauth2/authorize`, asks the user
  to paste the returned code or redirect URL, posts the authorization code and
  PKCE verifier to `https://myanimelist.net/v1/oauth2/token`, saves access and
  refresh tokens to `accounts.json`, then calls `/v2/users/@me` for the username.
- Kitsu posts email/username and password to
  `https://kitsu.app/api/oauth/token` using the original Taiga client id/secret,
  saves the returned access token to `accounts.json`, then calls
  `/api/edge/users?filter[self]=true` for username/display name.
- AniList opens `https://anilist.co/api/v2/oauth/authorize`, asks the user to
  paste the returned token or redirect URL, saves the token to `accounts.json`,
  then verifies it with the GraphQL `Viewer` query and saves the username.

Backend status:

- MyAnimeList, Kitsu, and AniList list synchronization are wired to the main
  Synchronize action. MAL uses `/v2/users/{username}/animelist`; Kitsu uses
  `/api/edge/library-entries`; AniList uses GraphQL `MediaListCollection`.
- Torrent filter rows now have a Qt settings backend with add/edit/remove/move,
  import/export, and reset. The Qt Torrents page now fetches the configured RSS
  feed, applies named defaults and structured action-field-condition-value rows,
  shows archive matches, and can open or archive selected torrents.
- mIRC sharing: v1 uses Windows DDE. The Linux Qt build exposes an external
  command setting and test launcher instead.
- Torrent archive cache is exposed through the existing v1 archive file at
  `v1/rss/archive.xml`, with count/size and clear support in Advanced / Cache.
- v1 settings import now recursively maps account, library, recognition,
  program, sharing, and torrent settings from `settings.xml` into Qt settings.

## Fresh component audit

The fresh pass compared `SettingsDialog::save()` in
`src/v1/ui/dlg/dlg_settings.cpp`, per-page initialization and command handlers in
`src/v1/ui/dlg/dlg_settings_page.cpp`, the 18 advanced setting descriptors in
`src/v1/ui/dlg/dlg_settings_advanced.cpp`, and the current Qt
`src/gui/settings/settings_dialog.cpp`.

Findings:

- All 7 original sections and 18 original pages are present in Qt.
- The 18 original Advanced values are present; `Application / Remember main
  window position and size` is now also represented inside the editable advanced
  group, matching the original label.
- HTTP and mIRC format buttons were present but inert; they now open an editor
  for their adjacent format fields.
- Library folder drag/drop and double-click-open behavior from Win32 is now
  recreated.
- Recognition media/streaming pages persist checked lists and expose
  select-all/clear-all controls.
- Torrent Discovery source/search controls are editable preset dropdowns.
- Torrent Downloads dependent-control enable/disable behavior is implemented
  for folder fallback and torrent application fields.
- Torrent Filters now has a saved Qt-side filter list backend, structured single
  condition editor, and the Torrents page consumes those filters when it parses
  the configured RSS feed.

## Regression coverage

- `taiga-core-smoke`: guards core normalization/version behavior, AniList,
  MyAnimeList, and Kitsu list-entry parsers, plus RSS/torrent feed parsing.
- `taiga-settings-dialog-smoke`: constructs the Qt settings dialog offscreen,
  verifies the 7-section / 18-page structure, and checks representative labels
  and buttons from every original settings section.
