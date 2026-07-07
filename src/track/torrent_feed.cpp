/**
 * Taiga
 * Copyright (C) 2010-2024, Eren Okka
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "torrent_feed.hpp"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpressionMatch>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QUrl>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <algorithm>

#include "base/rss.hpp"
#include "base/string.hpp"
#include "media/anime_db.hpp"
#include "media/anime_list.hpp"
#include "taiga/path.hpp"
#include "taiga/settings.hpp"
#include "track/episode.hpp"
#include "track/recognition.hpp"

namespace track::torrent {

namespace {

QJsonArray filterSettings() {
  return {
      QJsonObject{{"name", "Select currently watching"}, {"enabled", true}},
      QJsonObject{{"name", "Select airing anime in plan to watch"}, {"enabled", true}},
      QJsonObject{{"name", "Discard dropped"}, {"enabled", true}},
      QJsonObject{{"name", "Discard and deactivate not-in-list anime"}, {"enabled", true}},
      QJsonObject{{"name", "Discard watched and available episodes"}, {"enabled", true}},
      QJsonObject{{"name", "Prefer high-resolution files"}, {"enabled", true}},
  };
}

bool isLegacyDefaultFilterList(const QJsonArray& filters) {
  bool hasLegacyDefault = false;
  bool hasListAwareDefault = false;
  for (const auto& value : filters) {
    const auto name = value.toObject().value("name").toString();
    hasLegacyDefault = hasLegacyDefault || name == "Prefer best resolution" ||
                       name == "Ignore unknown episodes" || name == "Discard batches";
    hasListAwareDefault = hasListAwareDefault || name == "Select currently watching" ||
                          name == "Discard and deactivate not-in-list anime";
  }
  return hasLegacyDefault && !hasListAwareDefault;
}

QJsonArray configuredFilterSettings() {
  const auto value = taiga::settings.stringValue("rss.torrent.filters.itemsJson");
  const auto document = QJsonDocument::fromJson(value.toUtf8());
  if (document.isArray() && !isLegacyDefaultFilterList(document.array())) return document.array();
  return filterSettings();
}

QString cacheKey(const QString& source) {
  return QCryptographicHash::hash(source.toUtf8(), QCryptographicHash::Sha1).toHex();
}

QString archiveKey(const QString& title) {
  return title.simplified().toCaseFolded();
}

QString itemLink(const rss::Item& item) {
  if (!item.enclosure.url.empty()) return QString::fromStdString(item.enclosure.url);
  if (!item.link.empty()) return QString::fromStdString(item.link);
  return QString::fromStdString(item.guid.value);
}

QString itemInfoLink(const rss::Item& item) {
  if (!item.guid.value.empty()) return QString::fromStdString(item.guid.value);
  if (!item.link.empty()) return QString::fromStdString(item.link);
  return QString::fromStdString(item.enclosure.url);
}

bool containsWord(const QString& text, const QString& word) {
  return text.contains(word, Qt::CaseInsensitive);
}

bool itemContains(const Item& item, const QString& word) {
  return containsWord(item.title, word) || containsWord(item.filename, word) ||
         containsWord(item.description, word);
}

bool hasEpisodeNumber(const QString& text) {
  static const QRegularExpression episodeNumber{
      R"((?:^|[\s._\-\[\(])(?:\d{1,4})(?:v\d+)?(?:[\s._\-\]\)]|$))"};
  return episodeNumber.match(text).hasMatch();
}

bool itemHasEpisodeNumber(const Item& item) {
  return hasEpisodeNumber(item.title) || hasEpisodeNumber(item.filename) ||
         hasEpisodeNumber(item.description);
}

std::optional<int> episodeNumber(const Item& item) {
  static const QRegularExpression number{R"(^\s*(\d{1,4}))"};
  const auto match = number.match(item.episode);
  if (!match.hasMatch()) return std::nullopt;

  bool ok = false;
  const auto value = match.captured(1).toInt(&ok);
  return ok ? std::optional<int>{value} : std::nullopt;
}

QString namespaceValue(const rss::Item& item, const QString& suffix) {
  for (const auto& [key, value] : item.namespace_elements) {
    const auto qkey = QString::fromStdString(key);
    if (qkey.endsWith(suffix, Qt::CaseInsensitive)) {
      return QString::fromStdString(value);
    }
  }
  return {};
}

QString sourceFor(const QString& channelLink, const QString& itemSource) {
  const auto value = !channelLink.isEmpty() ? channelLink : itemSource;
  const auto host = QUrl{value}.host().toCaseFolded();
  const auto lower = value.toCaseFolded();
  if (host.contains("anidex")) return "anidex";
  if (host.contains("animebytes")) return "animebytes";
  if (host.contains("animetosho")) return "animetosho";
  if (host.contains("minglong")) return "minglong";
  if (host.contains("nyaa.si")) return "nyaa.si";
  if (host.contains("nyaa")) return "nyaa";
  if (host.contains("subsplease")) return "subsplease";
  if (host.contains("tokyotosho")) return "tokyotosho";
  if (lower.contains("anidex")) return "anidex";
  if (lower.contains("animebytes")) return "animebytes";
  if (lower.contains("animetosho")) return "animetosho";
  if (lower.contains("minglong")) return "minglong";
  if (lower.contains("nyaa.si")) return "nyaa.si";
  if (lower.contains("nyaa")) return "nyaa";
  if (lower.contains("subsplease")) return "subsplease";
  if (lower.contains("tokyotosho")) return "tokyotosho";
  return {};
}

QString titleCapture(const QString& title, const QRegularExpression& expression) {
  const auto match = expression.match(title);
  return match.hasMatch() ? match.captured(1).trimmed() : QString{};
}

QString descriptionCapture(const QString& description, const QRegularExpression& expression) {
  const auto match = expression.match(description);
  return match.hasMatch() ? match.captured(1).trimmed() : QString{};
}

QString stripHtml(QString value) {
  value.replace(QRegularExpression{"</p>", QRegularExpression::CaseInsensitiveOption}, "\n");
  value.replace(QRegularExpression{"<br\\s*/?>", QRegularExpression::CaseInsensitiveOption}, "\n");
  value.remove(QRegularExpression{"<[^>]+>"});
  value.replace("&amp;", "&");
  value.replace("&lt;", "<");
  value.replace("&gt;", ">");
  value.replace("&quot;", "\"");
  value = value.trimmed();
  while (value.contains("\n\n")) value.replace("\n\n", "\n");
  value.replace("\n", " | ");
  while (value.contains("  ")) value.replace("  ", " ");
  return value;
}

QString parseMagnet(const Item& item) {
  if (item.link.startsWith("magnet:", Qt::CaseInsensitive)) return item.link;
  if (item.magnet_link.startsWith("magnet:", Qt::CaseInsensitive)) return item.magnet_link;

  const auto match = QRegularExpression{R"(href=["'](magnet:[^"']+)["'])",
                                        QRegularExpression::CaseInsensitiveOption}
                         .match(item.description);
  return match.hasMatch() ? match.captured(1) : QString{};
}

QString normalizeVideo(QString value) {
  value.replace("2160p", "2160p", Qt::CaseInsensitive);
  value.replace("1080p", "1080p", Qt::CaseInsensitive);
  value.replace("720p", "720p", Qt::CaseInsensitive);
  value.replace("480p", "480p", Qt::CaseInsensitive);
  return value.simplified();
}

Category categoryFor(const Item& item, const track::Episode& episode) {
  if (item.category.contains("batch", Qt::CaseInsensitive)) return Category::Batch;
  if (item.title.contains("batch", Qt::CaseInsensitive) ||
      item.title.contains("complete", Qt::CaseInsensitive)) {
    return Category::Batch;
  }
  if (!episode.element(anitomy::ElementKind::Episode, "").empty() &&
      item.filename.contains(QRegularExpression{R"(\d+\s*-\s*\d+)"})) {
    return Category::Batch;
  }
  const auto extension = QString::fromStdString(episode.element(anitomy::ElementKind::FileExtension));
  if (!extension.isEmpty()) {
    static const QStringList videoExtensions{"mkv", "mp4", "avi", "ogm", "webm", "m4v"};
    if (!videoExtensions.contains(extension.toCaseFolded())) return Category::Other;
  }
  return Category::Anime;
}

void mark(Item& item, ItemState state, const QString& filter) {
  if (!isDiscarded(item.state) && item.state != ItemState::Archived) {
    item.state = state;
  }
  item.matchedFilters.push_back(filter);
}

void applyNamedFilter(Item& item, const QString& name) {
  const auto lower = name.toCaseFolded();
  const auto* anime = anime::db.item(item.anime_id);
  const auto* entry = anime::db.entry(item.anime_id);

  if (lower == "select currently watching") {
    if (entry && entry->status == anime::list::Status::Watching) {
      mark(item, ItemState::Selected, name);
    }
    return;
  }

  if (lower == "select airing anime in plan to watch") {
    if (anime && entry && anime->status == anime::Status::Airing &&
        entry->status == anime::list::Status::PlanToWatch) {
      mark(item, ItemState::Selected, name);
    }
    return;
  }

  if (lower == "discard dropped") {
    if (entry && entry->status == anime::list::Status::Dropped) {
      mark(item, ItemState::Discarded, name);
    }
    return;
  }

  if (lower == "discard and deactivate not-in-list anime") {
    if (!entry || !anime) {
      mark(item, ItemState::DiscardedInactive, name);
    }
    return;
  }

  if (lower == "discard watched and available episodes") {
    const auto episode = episodeNumber(item);
    if (entry && episode && *episode <= entry->watched_episodes) {
      mark(item, ItemState::Discarded, name);
    }
    return;
  }

  if (lower == "discard batches") {
    if (item.torrent_category == Category::Batch || itemContains(item, "batch") ||
        itemContains(item, "complete")) {
      mark(item, ItemState::Discarded, name);
    }
    return;
  }

  if (lower == "prefer best resolution" || lower == "prefer high-resolution files") {
    if (item.state == ItemState::Selected &&
        (itemContains(item, "2160p") || itemContains(item, "1080p"))) {
      mark(item, ItemState::Preferred, name);
    }
    return;
  }

  if (lower == "ignore unknown episodes") {
    if (!itemHasEpisodeNumber(item)) mark(item, ItemState::Discarded, name);
    return;
  }

  if (lower.startsWith("discard:")) {
    const auto pattern = name.mid(QString{"discard:"}.size()).trimmed();
    if (!pattern.isEmpty() && itemContains(item, pattern)) {
      mark(item, ItemState::Discarded, name);
    }
    return;
  }

  if (lower.startsWith("select:")) {
    const auto pattern = name.mid(QString{"select:"}.size()).trimmed();
    if (!pattern.isEmpty() && itemContains(item, pattern)) {
      mark(item, ItemState::Selected, name);
    }
    return;
  }

  if (lower.startsWith("prefer:")) {
    const auto pattern = name.mid(QString{"prefer:"}.size()).trimmed();
    if (item.state == ItemState::Selected && !pattern.isEmpty() && itemContains(item, pattern)) {
      mark(item, ItemState::Preferred, name);
    }
  }
}

QString filterFieldValue(const Item& item, const QString& field) {
  const auto lower = field.toCaseFolded();
  if (lower == "title") return item.title;
  if (lower == "filename") return item.filename;
  if (lower == "description") return item.description;
  if (lower == "group") return item.group;
  if (lower == "video") return item.video;
  if (lower == "category") return categoryText(item.torrent_category);
  if (lower == "source") return item.source;
  return QStringList{item.title, item.filename, item.description, item.group, item.video,
                     item.category, item.source}
      .join(" ");
}

bool structuredFilterMatches(const Item& item, const QJsonObject& filter) {
  const auto value = filter.value("value").toString().trimmed();
  if (value.isEmpty()) return false;

  const auto candidate = filterFieldValue(item, filter.value("field").toString("any"));
  const auto match = filter.value("match").toString("contains").toCaseFolded();
  if (match == "equals") return candidate.compare(value, Qt::CaseInsensitive) == 0;
  if (match == "regex") {
    return QRegularExpression{value, QRegularExpression::CaseInsensitiveOption}.match(candidate).hasMatch();
  }
  return candidate.contains(value, Qt::CaseInsensitive);
}

void applyStructuredFilter(Item& item, const QJsonObject& filter) {
  if (!structuredFilterMatches(item, filter)) return;

  const auto name = filter.value("name").toString("Custom filter");
  const auto action = filter.value("action").toString("select").toCaseFolded();
  if (action == "prefer") {
    if (item.state == ItemState::Selected) mark(item, ItemState::Preferred, name);
  } else if (action == "discard") {
    mark(item, ItemState::Discarded, name);
  } else if (action == "discard_inactive") {
    mark(item, ItemState::DiscardedInactive, name);
  } else {
    mark(item, ItemState::Selected, name);
  }
}

int statePriority(ItemState state) {
  switch (state) {
    case ItemState::Selected:
      return 0;
    case ItemState::Preferred:
      return 1;
    case ItemState::Normal:
      return 2;
    case ItemState::Discarded:
    case ItemState::DiscardedInactive:
      return 3;
    case ItemState::Archived:
    case ItemState::DiscardedHidden:
      return 4;
  }
  return 2;
}

}  // namespace

Feed parseFeedDocument(const QByteArray& data) {
  const auto feed = rss::parseDocument(data);
  Feed result;
  result.title = QString::fromStdString(feed.channel.title);
  result.link = QString::fromStdString(feed.channel.link);
  result.items.reserve(feed.items.size());

  for (const auto& rssItem : feed.items) {
    Item item;
    item.title = QString::fromStdString(rssItem.title);
    item.link = itemLink(rssItem);
    item.info_link = itemInfoLink(rssItem);
    item.description = QString::fromStdString(rssItem.description);
    item.category = QString::fromStdString(rssItem.category.value);
    item.group = titleCapture(item.title, QRegularExpression{R"(^\s*\[([^\]]+)\])"});
    item.episode = titleCapture(item.title, QRegularExpression{R"((?:^|[\s._-])(\d{1,4}(?:v\d+)?)(?:[\s._-]|\[|$))"});
    item.video = titleCapture(item.title, QRegularExpression{R"((\d{3,4}p|4K|8K|HEVC|AV1|x264|x265))",
                                                             QRegularExpression::CaseInsensitiveOption});
    item.filename = item.title;
    item.size = QString::fromStdString(rssItem.enclosure.length);
    item.seeders = namespaceValue(rssItem, "seeders");
    item.leechers = namespaceValue(rssItem, "leechers");
    item.downloads = namespaceValue(rssItem, "downloads");
    item.published = QString::fromStdString(rssItem.pub_date);
    item.source = QString::fromStdString(rssItem.source.name);
    if (item.source.isEmpty()) item.source = result.link.isEmpty() ? result.title : result.link;
    const auto source = sourceFor(result.link, item.source);
    if (source == "tokyotosho") {
      const auto size = descriptionCapture(item.description, QRegularExpression{
                                                                 R"(Size:\s*([^<\|]+))",
                                                                 QRegularExpression::CaseInsensitiveOption});
      if (!size.isEmpty()) item.size = size;
      const auto comment = descriptionCapture(item.description, QRegularExpression{
                                                                    R"(Comment:\s*(.*))",
                                                                    QRegularExpression::CaseInsensitiveOption});
      if (!comment.isEmpty()) item.description = comment;
    } else if (source == "nyaa.si") {
      const auto nyaaSize = namespaceValue(rssItem, "size");
      if (!nyaaSize.isEmpty()) item.size = nyaaSize;
    } else if (source == "animetosho") {
      if (item.link.contains("/view/") && !rssItem.enclosure.url.empty()) {
        item.link = QString::fromStdString(rssItem.enclosure.url);
      }
      const auto size = descriptionCapture(item.description, QRegularExpression{
                                                                 R"(Total Size</strong>:\s*([^<]+))",
                                                                 QRegularExpression::CaseInsensitiveOption});
      if (!size.isEmpty()) item.size = size;
    } else if (source == "anidex" || source == "minglong" || source == "subsplease") {
      const auto size = descriptionCapture(item.description, QRegularExpression{
                                                                 R"(Size:\s*([^<\|]+))",
                                                                 QRegularExpression::CaseInsensitiveOption});
      if (!size.isEmpty()) item.size = size;
    }
    item.magnet_link = parseMagnet(item);
    item.description = stripHtml(item.description);
    result.items.push_back(std::move(item));
  }

  examineItems(result.items, result.link);
  applyFilters(result.items);
  sortItems(result.items);
  return result;
}

std::vector<Item> parseFeed(const QByteArray& data) {
  return parseFeedDocument(data).items;
}

void examineItems(std::vector<Item>& items, const QString&) {
  for (auto& item : items) {
    anitomy::Options options;
    options.parse_file_extension = true;
    auto episode = track::recognition::parse(item.title.toStdString(), options);
    item.anime_id = track::recognition::identify(episode);

    if (item.episode.isEmpty()) {
      item.episode = QString::fromStdString(episode.element(anitomy::ElementKind::Episode));
    }
    if (item.group.isEmpty()) {
      item.group = QString::fromStdString(episode.element(anitomy::ElementKind::ReleaseGroup));
    }
    if (item.video.isEmpty()) {
      item.video =
          QString::fromStdString(episode.element(anitomy::ElementKind::VideoResolution));
    }
    item.video = normalizeVideo(item.video);
    item.torrent_category = categoryFor(item, episode);

    if (const auto anime = anime::db.item(item.anime_id)) {
      item.title = QString::fromStdString(anime->titles.romaji);
    } else if (const auto animeTitle =
                   QString::fromStdString(episode.element(anitomy::ElementKind::Title));
               !animeTitle.isEmpty()) {
      item.title = animeTitle;
    } else {
      item.torrent_category = Category::Other;
    }
  }
}

void applyFilters(std::vector<Item>& items) {
  for (auto& item : items) {
    if (archiveContains(item.title)) {
      item.state = ItemState::Archived;
      item.matchedFilters.push_back("Archive");
      continue;
    }

    if (!taiga::settings.boolValue("rss.torrent.filters.enabled", true)) continue;

    for (const auto& value : configuredFilterSettings()) {
      const auto object = value.toObject();
      if (!object.value("enabled").toBool(true)) continue;
      if (object.contains("action") || object.contains("field") || object.contains("value")) {
        applyStructuredFilter(item, object);
      } else {
        applyNamedFilter(item, object.value("name").toString());
      }
    }
  }
}

void sortItems(std::vector<Item>& items) {
  std::stable_sort(items.begin(), items.end(), [](const Item& lhs, const Item& rhs) {
    if (lhs.torrent_category != rhs.torrent_category) {
      return static_cast<int>(lhs.torrent_category) < static_cast<int>(rhs.torrent_category);
    }
    if (statePriority(lhs.state) != statePriority(rhs.state)) {
      return statePriority(lhs.state) < statePriority(rhs.state);
    }
    const auto lhsDate = QDateTime::fromString(lhs.published, Qt::RFC2822Date);
    const auto rhsDate = QDateTime::fromString(rhs.published, Qt::RFC2822Date);
    if (lhsDate.isValid() && rhsDate.isValid() && lhsDate != rhsDate) {
      return rhsDate < lhsDate;
    }
    return false;
  });
}

QList<Item> selectedItems(const std::vector<Item>& items) {
  QList<Item> selected;
  for (const auto& item : items) {
    if (item.state == ItemState::Selected || item.state == ItemState::Preferred) {
      selected.push_back(item);
    }
  }
  return selected;
}

QList<Item> sortedDownloadQueue(const std::vector<Item>& items) {
  auto queue = selectedItems(items);
  const auto sortBy = taiga::settings.stringValue("rss.torrent.downloadSortBy", "episode_number");
  const auto descending =
      taiga::settings.stringValue("rss.torrent.downloadSortOrder", "ascending") == "descending";
  std::sort(queue.begin(), queue.end(), [sortBy, descending](const Item& lhs, const Item& rhs) {
    if (lhs.anime_id != rhs.anime_id) return lhs.anime_id < rhs.anime_id;
    bool okLhs = false;
    bool okRhs = false;
    if (sortBy == "release_date") {
      const auto lhsDate = QDateTime::fromString(lhs.published, Qt::RFC2822Date);
      const auto rhsDate = QDateTime::fromString(rhs.published, Qt::RFC2822Date);
      return descending ? rhsDate < lhsDate : lhsDate < rhsDate;
    }
    const auto lhsEpisode = lhs.episode.toInt(&okLhs);
    const auto rhsEpisode = rhs.episode.toInt(&okRhs);
    if (okLhs && okRhs) return descending ? rhsEpisode < lhsEpisode : lhsEpisode < rhsEpisode;
    return lhs.episode.localeAwareCompare(rhs.episode) < 0;
  });
  return queue;
}

std::optional<DownloadPlan> downloadPlan(const Item& item) {
  const auto magnet =
      !item.magnet_link.isEmpty() ? item.magnet_link
                                  : (item.link.startsWith("magnet:", Qt::CaseInsensitive) ? item.link
                                                                                          : QString{});
  if (!magnet.isEmpty() &&
      taiga::settings.boolValue("rss.torrent.useMagnet", false)) {
    return DownloadPlan{.item = item, .url = QUrl{magnet}, .save_path = {}, .magnet = true};
  }

  const auto url = QUrl{item.link};
  if (!url.isValid() || url.scheme().isEmpty()) return std::nullopt;

  auto path = taiga::settings.stringValue("rss.torrent.fileDownloadLocation");
  if (path.isEmpty()) {
    path = cachePath(item.source);
    QFileInfo info{path};
    path = info.absolutePath();
  }
  QDir{}.mkpath(path);
  auto fileName = item.filename.isEmpty() ? item.title : item.filename;
  fileName.replace(QRegularExpression{R"([\\/:*?"<>|])"}, "_");
  if (!fileName.endsWith(".torrent", Qt::CaseInsensitive)) fileName += ".torrent";

  return DownloadPlan{.item = item, .url = url, .save_path = QDir{path}.filePath(fileName)};
}

bool saveFeedCache(const Feed& feed, const QByteArray& data) {
  QFileInfo info{cachePath(feed.link)};
  QDir{}.mkpath(info.absolutePath());
  QFile file{info.absoluteFilePath()};
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
  return file.write(data) == data.size();
}

std::optional<Feed> loadFeedCache(const QString& source) {
  QFile file{cachePath(source)};
  if (!file.open(QIODevice::ReadOnly)) return std::nullopt;
  auto feed = parseFeedDocument(file.readAll());
  if (feed.link.isEmpty()) feed.link = source;
  return feed;
}

QString archivePath() {
  return u"%1/v1/rss/archive.xml"_s.arg(QString::fromStdString(taiga::get_data_path()));
}

QString cachePath(const QString& source) {
  return u"%1/v1/rss/%2/feed.xml"_s.arg(QString::fromStdString(taiga::get_data_path()),
                                        cacheKey(source));
}

bool archiveContains(const QString& title) {
  QFile file{archivePath()};
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return false;

  const auto key = archiveKey(title);
  QXmlStreamReader xml{&file};
  while (!xml.atEnd()) {
    xml.readNext();
    if (!xml.isStartElement() || xml.name() != u"item") continue;
    const auto value = xml.attributes().value("title").toString();
    if (archiveKey(value) == key) return true;
  }
  return false;
}

void archiveItems(const std::vector<Item>& items) {
  QMap<QString, Item> archived;

  QFile existing{archivePath()};
  if (existing.open(QIODevice::ReadOnly | QIODevice::Text)) {
    QXmlStreamReader xml{&existing};
    while (!xml.atEnd()) {
      xml.readNext();
      if (!xml.isStartElement() || xml.name() != u"item") continue;
      Item item;
      item.title = xml.attributes().value("title").toString();
      item.link = xml.attributes().value("link").toString();
      item.published = xml.attributes().value("published").toString();
      if (!item.title.isEmpty()) archived.insert(archiveKey(item.title), item);
    }
  }

  for (const auto& item : items) {
    if (!item.title.isEmpty()) archived.insert(archiveKey(item.title), item);
  }

  QFileInfo info{archivePath()};
  QDir{}.mkpath(info.absolutePath());
  QFile file{archivePath()};
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) return;

  QXmlStreamWriter xml{&file};
  xml.setAutoFormatting(true);
  xml.writeStartDocument();
  xml.writeStartElement("archive");
  const auto limit = taiga::settings.intValue("rss.torrent.filters.archiveMaxCount", 1000);
  int written = 0;
  for (auto it = archived.cbegin(); it != archived.cend() && (limit <= 0 || written < limit); ++it) {
    xml.writeEmptyElement("item");
    xml.writeAttribute("title", it->title);
    xml.writeAttribute("link", it->link);
    xml.writeAttribute("published", it->published);
    ++written;
  }
  xml.writeEndElement();
  xml.writeEndDocument();
}

int archiveCount() {
  QFile file{archivePath()};
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return 0;

  QXmlStreamReader xml{&file};
  int count = 0;
  while (!xml.atEnd()) {
    xml.readNext();
    if (xml.isStartElement() && xml.name() == u"item") ++count;
  }
  return count;
}

QString stateText(ItemState state) {
  switch (state) {
    case ItemState::Normal:
      return "New";
    case ItemState::Selected:
      return "Selected";
    case ItemState::Preferred:
      return "Preferred";
    case ItemState::Discarded:
      return "Discarded";
    case ItemState::DiscardedInactive:
      return "Discarded inactive";
    case ItemState::DiscardedHidden:
      return "Discarded hidden";
    case ItemState::Archived:
      return "Archived";
  }
  return "New";
}

QString categoryText(Category category) {
  switch (category) {
    case Category::Anime:
      return "Anime";
    case Category::Batch:
      return "Batch";
    case Category::Other:
      return "Other";
  }
  return "Anime";
}

bool isDiscarded(ItemState state) {
  return state == ItemState::Discarded || state == ItemState::DiscardedInactive ||
         state == ItemState::DiscardedHidden;
}

bool isHidden(ItemState state) {
  return state == ItemState::DiscardedHidden || state == ItemState::Archived;
}

}  // namespace track::torrent
