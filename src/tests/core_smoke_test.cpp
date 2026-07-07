#include <QCoreApplication>

#include <cstdlib>
#include <iostream>
#include <string>

#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QFile>

#include <anisthesia.hpp>

#include "base/rss.hpp"
#include "media/anime.hpp"
#include "media/anime_db.hpp"
#include "media/anime_list.hpp"
#include "media/anime_list_utils.hpp"
#include "media/anime_season.hpp"
#include "media/anime_season_db.hpp"
#include "sync/anilist_parsers.hpp"
#include "sync/kitsu_parsers.hpp"
#include "sync/myanimelist_parsers.hpp"
#include "taiga/settings.hpp"
#include "taiga/version.hpp"
#include "track/torrent_feed.hpp"
#include "track/recognition_cache.hpp"
#include "track/recognition_normalize.hpp"
#include "track/scanner.hpp"

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(EXIT_FAILURE);
  }
}

}  // namespace

int main(int argc, char* argv[]) {
  QCoreApplication app(argc, argv);
  QTemporaryDir dataDir;
  require(dataDir.isValid(), "Could not create isolated test data directory");
  qputenv("TAIGA_DATA_PATH", dataDir.path().toUtf8());
  anime::db.init();

  require(taiga::version().major == 2, "Unexpected major version");
  require(track::recognition::normalize("Nisekoi: Season 2") == "nisekoi2",
          "Season normalization changed");
  require(track::recognition::normalize("The iDOLM@STER") == "idolmaster",
          "Title transliteration changed");

  const auto entryJson = QJsonDocument::fromJson(R"({
    "id": 42,
    "status": "REPEATING",
    "score": 87,
    "progress": 12,
    "repeat": 1,
    "private": true,
    "notes": "rewatch",
    "startedAt": {"year": 2024, "month": 1, "day": 2},
    "completedAt": {"year": 2024, "month": 2, "day": 3},
    "updatedAt": 1700000000,
    "media": {"id": 100}
  })").object();
  const auto entry = taiga_sync::anilist::parseMediaListEntry(entryJson);
  require(entry.has_value(), "AniList list entry parser rejected valid JSON");
  require(entry->anime_id == 100, "AniList list entry anime id changed");
  require(entry->rewatching, "AniList REPEATING status should become rewatching");
  require(entry->status == anime::list::Status::Watching,
          "AniList REPEATING status should display as watching");

  const auto anilistMediaJson = QJsonDocument::fromJson(R"({
    "id": 100,
    "episodes": null,
    "duration": 24,
    "status": "RELEASING",
    "format": "TV",
    "startDate": {"year": 2026, "month": 7, "day": 1},
    "endDate": {},
    "averageScore": 75,
    "popularity": 1234,
    "coverImage": {"extraLarge": "https://example.test/poster.jpg"},
    "title": {"romaji": "Example", "english": "Example", "native": "Example"},
    "countryOfOrigin": "JP"
  })").object();
  const auto anilistMedia = taiga_sync::anilist::parseMedia(anilistMediaJson);
  require(anilistMedia.has_value(), "AniList media parser rejected valid JSON");
  require(anilistMedia->episode_count == anime::kUnknownEpisodeCount,
          "AniList null episode count should remain unknown");

  const auto malJson = QJsonDocument::fromJson(R"({
    "status": "watching",
    "score": 8,
    "num_episodes_watched": 4,
    "is_rewatching": true,
    "updated_at": "2024-01-02T03:04:05+00:00",
    "start_date": "2024-01-01",
    "finish_date": "2024-02-01",
    "num_times_rewatched": 1,
    "comments": "notes"
  })").object();
  const auto malEntry = taiga_sync::myanimelist::parseListEntry(malJson, 200);
  require(malEntry.has_value(), "MAL list entry parser rejected valid JSON");
  require(malEntry->anime_id == 200, "MAL list entry anime id changed");
  require(malEntry->score == 80, "MAL score conversion changed");
  require(malEntry->rewatching, "MAL rewatching flag changed");

  const auto kitsuJson = QJsonDocument::fromJson(R"({
    "id": "55",
    "attributes": {
      "status": "current",
      "progress": 9,
      "ratingTwenty": 16,
      "private": true,
      "reconsumeCount": 2,
      "reconsuming": true,
      "startedAt": "2024-01-02T00:00:00.000Z",
      "finishedAt": "2024-02-03T00:00:00.000Z",
      "updatedAt": "2024-02-04T00:00:00.000Z",
      "notes": "kitsu notes"
    }
  })").object();
  const auto kitsuEntry = taiga_sync::kitsu::parseListEntry(kitsuJson, 300);
  require(kitsuEntry.has_value(), "Kitsu list entry parser rejected valid JSON");
  require(kitsuEntry->id == 55, "Kitsu list entry id changed");
  require(kitsuEntry->anime_id == 300, "Kitsu list entry anime id changed");
  require(kitsuEntry->score == 80, "Kitsu score conversion changed");

  const auto feed = rss::parseDocument(R"(
    <rss><channel><title>Nyaa</title>
      <item>
        <title>[Group] Example Anime - 01 [1080p]</title>
        <link>magnet:?xt=urn:btih:test</link>
        <pubDate>Tue, 07 Jul 2026 12:00:00 GMT</pubDate>
        <enclosure url="https://example.test/file.torrent" length="1048576" type="application/x-bittorrent"/>
      </item>
    </channel></rss>
  )");
  require(feed.items.size() == 1, "RSS parser did not read torrent item");
  taiga::settings.setBoolValue("rss.torrent.filters.enabled", true);
  taiga::settings.setStringValue("rss.torrent.filters.itemsJson", {});
  Anime exampleAnime;
  exampleAnime.id = 9001;
  exampleAnime.status = anime::Status::Airing;
  exampleAnime.titles.romaji = "Example Anime";
  anime::db.updateItem(exampleAnime);
  require(anime::db.item(9001) != nullptr, "Anime DB fixture insert failed");
  track::recognition::cache()->update(exampleAnime);
  ListEntry exampleEntry;
  exampleEntry.id = 9001;
  exampleEntry.anime_id = 9001;
  exampleEntry.status = anime::list::Status::Watching;
  anime::db.updateEntry(exampleEntry);
  anime::db.deleteEntry(exampleAnime.id);
  require(anime::db.entry(exampleAnime.id) == nullptr, "Anime DB list entry delete failed");
  anime::db.updateEntry(exampleEntry);
  const auto torrents = track::torrent::parseFeed(R"(
    <rss><channel><title>Nyaa</title>
      <item><title>[Group] Example Anime - 01 [1080p]</title><link>magnet:?xt=urn:btih:test</link></item>
    </channel></rss>
  )");
  require(torrents.size() == 1, "Torrent feed parser rejected valid RSS");
  require(torrents.front().title == "Example Anime", "Torrent recognition title extraction changed");
  require(torrents.front().episode == "01", "Torrent recognition episode extraction changed");
  require(torrents.front().group == "Group", "Torrent recognition group extraction changed");
  require(torrents.front().video == "1080p", "Torrent recognition resolution extraction changed");
  require(torrents.front().torrent_category == track::torrent::Category::Anime,
          "Torrent category detection changed");
  require(torrents.front().anime_id == 9001, "Torrent recognition did not match list anime");
  require(torrents.front().state == track::torrent::ItemState::Preferred,
          "Torrent default resolution preference changed");

  const auto unrelatedTorrents = track::torrent::parseFeed(R"(
    <rss><channel><title>Nyaa</title>
      <item>
        <title>[OtherSubs] Random Feed Show - 01 [1080p]</title>
        <link>magnet:?xt=urn:btih:random</link>
        <pubDate>Tue, 07 Jul 2026 12:00:00 GMT</pubDate>
      </item>
    </channel></rss>
  )");
  require(unrelatedTorrents.size() == 1, "Unrelated torrent fixture did not parse");
  require(unrelatedTorrents.front().state == track::torrent::ItemState::DiscardedInactive,
          "Not-in-list high-resolution torrents must not be auto-marked");

  taiga::settings.setStringValue("rss.torrent.filters.itemsJson", R"([
    {"name":"Select currently watching","enabled":true},
    {"name":"Discard and deactivate not-in-list anime","enabled":true},
    {"name":"prefer:OtherSubs","enabled":true}
  ])");
  const auto fansubTorrents = track::torrent::parseFeed(R"(
    <rss><channel><title>Nyaa</title>
      <item><title>[OtherSubs] Random Feed Show - 01 [1080p]</title><link>magnet:?xt=urn:btih:random</link></item>
    </channel></rss>
  )");
  require(fansubTorrents.front().state == track::torrent::ItemState::DiscardedInactive,
          "Preferred fansub filters must not select anime outside the user's list");

  taiga::settings.setStringValue("rss.torrent.filters.itemsJson", R"([
    {"name":"Select fansub","enabled":true,"action":"select","field":"group","match":"equals","value":"Group"},
    {"name":"Prefer 1080p","enabled":true,"action":"prefer","field":"video","match":"contains","value":"1080p"},
    {"name":"Discard raws","enabled":true,"action":"discard","field":"filename","match":"regex","value":"RAW"}
  ])");
  const auto structuredTorrents = track::torrent::parseFeed(R"(
    <rss><channel><title>Nyaa</title>
      <item><title>[Group] Example Anime - 02 [1080p]</title><link>magnet:?xt=urn:btih:structured</link></item>
    </channel></rss>
  )");
  require(structuredTorrents.front().state == track::torrent::ItemState::Preferred,
          "Structured torrent filters should select and prefer matching rows");

  taiga::settings.setStringValue("rss.torrent.filters.itemsJson", R"([
    {"name":"Multi-condition exact group","enabled":true,"action":"select","logic":"all","conditions":[
      {"field":"group","match":"equals","value":"Group"},
      {"field":"video","match":"notcontains","value":"480p"}
    ]},
    {"name":"Hide low resolution","enabled":true,"action":"discard","option":"hide","logic":"any","conditions":[
      {"field":"video","match":"equals","value":"480p"},
      {"field":"filename","match":"beginswith","value":"RAW"}
    ]}
  ])");
  const auto multiConditionTorrents = track::torrent::parseFeed(R"(
    <rss><channel><title>Nyaa</title>
      <item><title>[Group] Example Anime - 03 [720p]</title><link>magnet:?xt=urn:btih:multi1</link></item>
      <item><title>[Group] Example Anime - 04 [480p]</title><link>magnet:?xt=urn:btih:multi2</link></item>
    </channel></rss>
  )");
  require(multiConditionTorrents.size() == 2, "Multi-condition torrent fixture did not parse");
  require(multiConditionTorrents.at(0).state == track::torrent::ItemState::Selected,
          "All-condition torrent filter should select matching rows");
  require(multiConditionTorrents.at(1).state == track::torrent::ItemState::DiscardedHidden,
          "Discard hide option should map to a hidden discarded torrent row");

  Anime otherAnime;
  otherAnime.id = 9002;
  otherAnime.status = anime::Status::Airing;
  otherAnime.titles.romaji = "Other Anime";
  anime::db.updateItem(otherAnime);
  track::recognition::cache()->update(otherAnime);
  ListEntry otherEntry;
  otherEntry.id = 9002;
  otherEntry.anime_id = 9002;
  otherEntry.status = anime::list::Status::Watching;
  anime::db.updateEntry(otherEntry);
  taiga::settings.setStringValue("rss.torrent.filters.itemsJson", R"([
    {"name":"Only Example Anime","enabled":true,"action":"discard","animeIds":[9001],"conditions":[
      {"field":"video","match":"equals","value":"720p"}
    ]}
  ])");
  const auto animeLimitedTorrents = track::torrent::parseFeed(R"(
    <rss><channel><title>Nyaa</title>
      <item><title>[Group] Example Anime - 05 [720p]</title><link>magnet:?xt=urn:btih:limit1</link></item>
      <item><title>[Group] Other Anime - 05 [720p]</title><link>magnet:?xt=urn:btih:limit2</link></item>
    </channel></rss>
  )");
  auto limitedState = [](const auto& items, const QString& title) {
    for (const auto& item : items) {
      if (item.title == title) return item.state;
    }
    return track::torrent::ItemState::Archived;
  };
  require(limitedState(animeLimitedTorrents, "Example Anime") == track::torrent::ItemState::Discarded,
          "Anime-limited torrent filter should apply to listed anime IDs");
  require(limitedState(animeLimitedTorrents, "Other Anime") == track::torrent::ItemState::Normal,
          "Anime-limited torrent filter should ignore other anime IDs");

  taiga::settings.setStringValue("rss.torrent.filters.itemsJson", {});

  const auto libraryPath = dataDir.filePath("library");
  QDir{}.mkpath(libraryPath);
  QFile episodeFile{libraryPath + "/Example Anime - 01.mkv"};
  require(episodeFile.open(QIODevice::WriteOnly), "Could not create scanner fixture file");
  episodeFile.write("test");
  episodeFile.close();
  const auto scanSummary = track::scanAvailableEpisodes({libraryPath.toStdString()});
  require(scanSummary.folders == 1, "Scanner did not count configured library folder");
  require(scanSummary.files == 1, "Scanner did not count library media file");
  require(scanSummary.recognized == 1, "Scanner did not recognize known fixture episode");
  require(scanSummary.anime == 1, "Scanner did not count unique recognized anime");
  require(!anime::db.availableEpisodePath(exampleAnime.id, 1).isEmpty(),
          "Scanner did not persist episode availability");

  exampleEntry.watched_episodes = 0;
  anime::db.updateEntry(exampleEntry);
  const auto availableTorrents = track::torrent::parseFeed(R"(
    <rss><channel><title>Nyaa</title>
      <item><title>[Group] Example Anime - 01 [720p]</title><link>magnet:?xt=urn:btih:available</link></item>
    </channel></rss>
  )");
  require(availableTorrents.front().state == track::torrent::ItemState::Discarded,
          "Available local episodes should be discarded by the default torrent filters");

  auto groupedItems = std::vector<track::torrent::Item>{
      {.published = "Tue, 07 Jul 2026 13:00:00 GMT",
       .torrent_category = track::torrent::Category::Anime},
      {.published = "Tue, 07 Jul 2026 12:30:00 GMT",
       .torrent_category = track::torrent::Category::Batch},
      {.published = "Tue, 07 Jul 2026 12:00:00 GMT",
       .torrent_category = track::torrent::Category::Anime},
      {.published = "Tue, 07 Jul 2026 11:30:00 GMT",
       .torrent_category = track::torrent::Category::Batch},
  };
  track::torrent::sortItems(groupedItems);
  require(groupedItems.at(0).torrent_category == track::torrent::Category::Anime &&
              groupedItems.at(1).torrent_category == track::torrent::Category::Anime &&
              groupedItems.at(2).torrent_category == track::torrent::Category::Batch &&
              groupedItems.at(3).torrent_category == track::torrent::Category::Batch,
          "Torrent sorting should keep one contiguous block per v1 group");

  const auto nyaaFeed = track::torrent::parseFeedDocument(R"(
    <rss xmlns:nyaa="https://nyaa.si/xmlns/nyaa"><channel>
      <title>Nyaa</title>
      <link>https://nyaa.si/?page=rss&amp;c=1_2&amp;f=0</link>
      <item>
        <title>[Subs] Another Show - 02 [720p]</title>
        <link>https://nyaa.si/download/123.torrent</link>
        <guid>https://nyaa.si/view/123</guid>
        <pubDate>Tue, 07 Jul 2026 12:00:00 GMT</pubDate>
        <nyaa:size>2.0 GiB</nyaa:size>
        <nyaa:seeders>11</nyaa:seeders>
        <nyaa:leechers>2</nyaa:leechers>
        <nyaa:downloads>99</nyaa:downloads>
      </item>
    </channel></rss>
  )");
  require(nyaaFeed.items.size() == 1, "Nyaa torrent feed parser rejected valid RSS");
  require(nyaaFeed.items.front().size == "2.0 GiB", "Nyaa torrent size parsing changed");
  require(nyaaFeed.items.front().seeders == "11", "Nyaa torrent seed parsing changed");
  require(nyaaFeed.items.front().leechers == "2", "Nyaa torrent leech parsing changed");
  require(nyaaFeed.items.front().downloads == "99", "Nyaa torrent download parsing changed");
  require(nyaaFeed.items.front().info_link == "https://nyaa.si/view/123",
          "Nyaa torrent info link parsing changed");

  auto queueItems = std::vector<track::torrent::Item>{
      {.anime_id = 7,
       .title = "Queue Show",
       .link = "https://example.test/02.torrent",
       .episode = "02",
       .filename = "Queue Show - 02",
       .state = track::torrent::ItemState::Selected},
      {.anime_id = 7,
       .title = "Queue Show",
       .link = "https://example.test/01.torrent",
       .episode = "01",
       .filename = "Queue Show - 01",
       .state = track::torrent::ItemState::Selected},
  };
  taiga::settings.setStringValue("rss.torrent.downloadSortBy", "episode_number");
  taiga::settings.setStringValue("rss.torrent.downloadSortOrder", "ascending");
  const auto queue = track::torrent::sortedDownloadQueue(queueItems);
  require(queue.size() == 2, "Torrent download queue missed selected items");
  require(queue.front().episode == "01", "Torrent download queue episode sort changed");

  taiga::settings.setBoolValue("rss.torrent.useMagnet", true);
  auto planItem = torrents.front();
  planItem.link = "https://example.test/file.torrent";
  planItem.magnet_link = "magnet:?xt=urn:btih:test";
  const auto magnetPlan = track::torrent::downloadPlan(planItem);
  require(magnetPlan.has_value(), "Torrent magnet plan was not created");
  require(magnetPlan->magnet, "Torrent magnet preference was ignored");
  require(magnetPlan->url.scheme() == "magnet", "Torrent magnet URL changed");
  taiga::settings.setBoolValue("rss.torrent.useMagnet", false);

  track::torrent::archiveItems({planItem});
  require(track::torrent::archiveCount() == 1, "Torrent archive count changed");
  require(QFile::remove(track::torrent::archivePath()), "Torrent archive clear path failed");
  require(track::torrent::archiveCount() == 0, "Torrent archive clear did not remove archived items");

  Anime unknownEpisodeAnime;
  unknownEpisodeAnime.episode_count = anime::kUnknownEpisodeCount;
  ListEntry unknownEpisodeEntry;
  unknownEpisodeEntry.watched_episodes = 0;
  require(anime::list::getProgressRatio(&unknownEpisodeAnime, &unknownEpisodeEntry) == 0.0f,
          "Unknown episode count should not draw arbitrary progress");

  const auto season = anime::Season{anime::SeasonName::Summer, std::chrono::year{2026}};
  anime::season_db.set(season, {100, 200});
  require(anime::season_db.matches(season), "Season database did not retain current season");
  require(anime::season_db.items.size() == 2, "Season database did not retain season ids");
  anime::season_db.reset();
  require(!anime::season_db.matches(season), "Season database reset did not clear current season");
  require(anime::season_db.items.isEmpty(), "Season database reset did not clear ids");

  std::vector<anisthesia::Result> mediaResults;
  const auto rejectAllMedia = [](const anisthesia::MediaInfo&) { return false; };
  require(!anisthesia::GetResults({}, rejectAllMedia, mediaResults),
          "Anisthesia platform bridge should reject all media when the filter rejects it");
  require(mediaResults.empty(), "Anisthesia platform bridge should leave no rejected results");

  return EXIT_SUCCESS;
}
