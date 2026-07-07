# Torrent view v1 to v2 analysis

This document maps the original Win32 torrent screen to the Qt cross-platform
implementation and records the timeout behavior seen during the migration.

## Live endpoint check

On this Linux machine, the v1 default feed source:

`https://www.tokyotosho.info/rss.php?filter=1,11&zwnj=0`

currently responds with Cloudflare `525` after about 19.5 seconds. The v2
network manager previously had a 10 second transfer timeout, so the native app
timed out before it could receive and report the server response. The v1 HTTP
stack uses a 30 second timeout.

`https://nyaa.si/?page=rss&c=1_2&f=0` responds successfully in about 0.3 seconds
from the same machine.

## v1 behavior

Source files:

- `src/ui/dlg/dlg_torrent.cpp`
- `src/track/feed_aggregator.cpp`
- `src/track/feed.cpp`
- `src/track/feed_source.cpp`
- `src/track/feed_filter_manager.cpp`
- `src/taiga/http.cpp`

The v1 Torrent page is not just a feed table. It is a view over
`track::aggregator`, which owns the active `Feed`, archive, download queue, and
filter pipeline.

### Fetch flow

1. Toolbar command `Check new torrents` clears the main search box.
2. Normal click calls `track::aggregator.CheckFeed(settings.GetTorrentDiscoverySource())`.
3. Ctrl-click reloads cached feed XML from disk, then re-examines it.
4. `CheckFeed` sends an HTTP request with:
   - `Accept: application/rss+xml, */*`
   - `Accept-Encoding: gzip`
   - default `User-Agent: Taiga/<major>.<minor>`
   - 30 second timeout
   - proxy and certificate settings from the app settings
5. While transferring, v1 updates the status bar with host and transfer progress.
6. On response, v1 handles connection errors, Cloudflare/DDoS-style errors,
   4xx/5xx status classes, and gzip response bodies.
7. Successful data is saved as `feed.xml`, parsed, examined, filtered, archived,
   sorted, and displayed.

### Parse and classify flow

1. `Feed::Load` parses RSS into `FeedItem`s.
2. `GetFeedSource` detects known sources: AniDex, AnimeTosho, Nyaa, SubsPlease,
   TokyoToshokan, and others.
3. `ParseFeedItemFromSource` applies source-specific parsing:
   - TokyoToshokan: size from description, magnet link, comment description,
     info link.
   - Nyaa: info link, size, seeders, leechers, downloads from `nyaa:*` fields.
   - AnimeTosho/AniDex/etc.: source-specific link and metadata fixes.
4. Anitomy/recognition identifies anime title, episode number, batch status,
   release group, video resolution, and source category.
5. Filters mark items as selected, preferred, discarded, deactivated, hidden, or
   archived.
6. Items are sorted by state priority.

### View behavior

The v1 list has:

- Groups: `Anime`, `Batch`, `Other`.
- Checkboxes.
- Status icons from anime airing status.
- Columns:
  `Anime title`, `Episode`, `Group`, `Size`, `Video`, `S`, `L`, `D`,
  `Description`, `Filename`, `Release date`.
- Column-specific sorting:
  episode range, file size, numeric seed/leech/download counts, RFC822 date, or
  text.
- Shift-click checkbox range selection within the same group.
- Enter and double-click download the selected torrent.
- Right-click actions:
  download torrent, anime info, torrent info, discard one, discard all for the
  anime, select/prefer fansub, more torrents, search service.
- Debug-mode row coloring for selected and discarded states.
- Status bar updates for marked torrent count.

### Download behavior

V1 `Download` is a queue-aware operation:

1. Marked items are sorted by configured queue order.
2. Magnet links are opened directly.
3. `.torrent` files are downloaded with `Accept: application/x-bittorrent, */*`.
4. Files are saved to the configured torrent-file path or feed cache path.
5. Download success archives the item and opens the configured BitTorrent app.
6. Download location can use anime folders, fallback folders, and optional
   created subfolders.

## Current v2 behavior

Source files:

- `src/gui/torrents/torrents_widget.cpp`
- `src/track/torrent_feed.cpp`
- `src/taiga/network.cpp`
- `src/gui/settings/settings_dialog.cpp`

The v2 Torrent page currently implements the view shell and a simpler feed
path. It does not yet fully replace `track::aggregator`.

Already present:

- V1 column inventory.
- V1-style toolbar labels/icons.
- Search field.
- Checkable rows.
- Status icons by matched anime title.
- Feed fetch with v1-compatible timeout and RSS request headers.
- RSS parser plus source-specific metadata extraction.
- Basic archive.
- Basic named filters.
- Basic right-click menu.
- Double-click/open selected link.

Recently aligned:

- Shared network timeout is now 30 seconds, matching v1.
- Torrent requests now send the v1 RSS `Accept` header and Taiga user-agent.
- Settings now expose the v1 feed-source and search-source option lists.
- Ctrl-refresh reloads the cached feed XML for the active source.
- Feed XML is saved after successful fetches.
- Torrent rows are grouped into `Anime`, `Batch`, and `Other`.
- Anitomy recognition now extracts anime title, episode, release group, video
  resolution, and matched anime ID for status icons.
- Filters evaluate the preserved filename/details so title normalization does
  not hide resolution or episode tokens.
- Shift-click checkbox range marking works within the same torrent group.
- Download queue sorting follows the configured episode/date ordering.
- Magnet links and `.torrent` downloads are both supported.
- The context menu includes torrent info, more torrents, service search,
  prefer fansub group, discard anime/group, download, refresh, and settings.
- Automatic torrent checking now runs from the Torrents page and shows a
  countdown in the toolbar action text.
- Enter and numpad Enter trigger the same download/open path as double-click.
- Return in the shared shell search box submits the configured torrent search
  feed URL and replaces the torrent list with those results.

## Remaining translation gaps

The largest remaining gap is not the table. It is the missing v1 aggregator
model.

Needed v2 pieces:

- A fuller `TorrentAggregator` core object equivalent to v1 `track::aggregator`
  would still make the code easier to reason about; the current functionality is
  split between `track::torrent` and `TorrentsWidget`.
- Full feed-filter condition/action/operator parity with v1, beyond the current
  named/default filters and JSON-backed settings UI.
- Column-specific sort comparators for episode ranges, file size, S/L/D counts,
  and RFC822 dates.
- Custom client path/app-mode launch verification on Linux.
- Detailed transfer progress in the status bar.
- More precise Cloudflare/DDoS-style error messaging for 522/503 responses.

## Practical note

TokyoToshokan is currently a poor default for testing on this machine because it
returns Cloudflare `525` slowly. Nyaa is the better active test source right now.
This does not remove the need to support TokyoToshokan; it means v2 should report
the server failure accurately and not fail early with a client-side timeout.
