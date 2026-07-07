# Torrent view v1 to v2 analysis

This document maps the original Win32 torrent screen to the Qt cross-platform
implementation and records the timeout behavior seen during the migration.

## Live endpoint check

On this Linux machine, the v1 default feed source:

`https://www.tokyotosho.info/rss.php?filter=1,11&zwnj=0`

currently responds with Cloudflare `522` after about 19.5 seconds. The v2
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
- Basic feed fetch.
- Basic RSS parser.
- Basic archive.
- Basic named filters.
- Basic right-click menu.
- Double-click/open selected link.

Recently aligned:

- Shared network timeout is now 30 seconds, matching v1.
- Torrent requests now send the v1 RSS `Accept` header and Taiga user-agent.
- Settings now expose the v1 feed-source and search-source option lists.

## Remaining translation gaps

The largest remaining gap is not the table. It is the missing v1 aggregator
model.

Needed v2 pieces:

- A `TorrentAggregator` core object equivalent to v1 `track::aggregator`.
- Cached feed XML reload and Ctrl-refresh behavior.
- Per-source parser parity with `feed_source.cpp`.
- Recognition/Anitomy pass over torrent titles, not regex-only extraction.
- Full feed-filter model with actions, match modes, operators, options, and
  import/export.
- Grouped view model for `Anime`, `Batch`, and `Other`.
- Checkbox range behavior.
- Correct state colors and inactive/hidden handling.
- Full context menu parity.
- Queue-aware download behavior.
- Magnet-link handling.
- `.torrent` file download, save, archive, and launch configured client.
- Automatic checking timer with toolbar countdown.
- Detailed status-bar transfer progress.
- Better server-error reporting, especially Cloudflare 522/503 cases.

## Practical note

TokyoToshokan is currently a poor default for testing on this machine because it
returns Cloudflare `522` slowly. Nyaa is the better active test source right now.
This does not remove the need to support TokyoToshokan; it means v2 should report
the server failure accurately and not fail early with a client-side timeout.
