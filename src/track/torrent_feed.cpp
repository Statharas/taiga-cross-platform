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

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QRegularExpression>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "base/rss.hpp"
#include "base/string.hpp"
#include "taiga/path.hpp"
#include "taiga/settings.hpp"

namespace track::torrent {

namespace {

QJsonArray filterSettings() {
  const auto value = taiga::settings.stringValue("rss.torrent.filters.itemsJson");
  const auto document = QJsonDocument::fromJson(value.toUtf8());
  if (document.isArray()) return document.array();

  return {
      QJsonObject{{"name", "Discard batches"}, {"enabled", true}},
      QJsonObject{{"name", "Prefer fansub groups"}, {"enabled", true}},
      QJsonObject{{"name", "Prefer best resolution"}, {"enabled", true}},
      QJsonObject{{"name", "Ignore unknown episodes"}, {"enabled", true}},
  };
}

QString archiveKey(const QString& title) {
  return title.simplified().toCaseFolded();
}

QString itemLink(const rss::Item& item) {
  if (!item.enclosure.url.empty()) return QString::fromStdString(item.enclosure.url);
  if (!item.link.empty()) return QString::fromStdString(item.link);
  return QString::fromStdString(item.guid.value);
}

bool containsWord(const QString& text, const QString& word) {
  return text.contains(word, Qt::CaseInsensitive);
}

bool hasEpisodeNumber(const QString& text) {
  static const QRegularExpression episodeNumber{
      R"((?:^|[\s._\-\[\(])(?:\d{1,4})(?:v\d+)?(?:[\s._\-\]\)]|$))"};
  return episodeNumber.match(text).hasMatch();
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

QString titleCapture(const QString& title, const QRegularExpression& expression) {
  const auto match = expression.match(title);
  return match.hasMatch() ? match.captured(1).trimmed() : QString{};
}

void mark(Item& item, ItemState state, const QString& filter) {
  if (item.state != ItemState::Discarded && item.state != ItemState::Archived) {
    item.state = state;
  }
  item.matchedFilters.push_back(filter);
}

void applyNamedFilter(Item& item, const QString& name) {
  const auto lower = name.toCaseFolded();

  if (lower == "discard batches") {
    if (containsWord(item.title, "batch") || containsWord(item.title, "complete")) {
      mark(item, ItemState::Discarded, name);
    }
    return;
  }

  if (lower == "prefer best resolution") {
    if (containsWord(item.title, "2160p") || containsWord(item.title, "1080p")) {
      mark(item, ItemState::Preferred, name);
    }
    return;
  }

  if (lower == "ignore unknown episodes") {
    if (!hasEpisodeNumber(item.title)) mark(item, ItemState::Discarded, name);
    return;
  }

  if (lower.startsWith("discard:")) {
    const auto pattern = name.mid(QString{"discard:"}.size()).trimmed();
    if (!pattern.isEmpty() && containsWord(item.title, pattern)) {
      mark(item, ItemState::Discarded, name);
    }
    return;
  }

  if (lower.startsWith("select:")) {
    const auto pattern = name.mid(QString{"select:"}.size()).trimmed();
    if (!pattern.isEmpty() && containsWord(item.title, pattern)) {
      mark(item, ItemState::Selected, name);
    }
    return;
  }

  if (lower.startsWith("prefer:")) {
    const auto pattern = name.mid(QString{"prefer:"}.size()).trimmed();
    if (!pattern.isEmpty() && containsWord(item.title, pattern)) {
      mark(item, ItemState::Preferred, name);
    }
  }
}

}  // namespace

std::vector<Item> parseFeed(const QByteArray& data) {
  const auto feed = rss::parseDocument(data);
  std::vector<Item> items;
  items.reserve(feed.items.size());

  for (const auto& rssItem : feed.items) {
    Item item;
    item.title = QString::fromStdString(rssItem.title);
    item.link = itemLink(rssItem);
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
    if (item.source.isEmpty()) item.source = QString::fromStdString(feed.channel.title);
    items.push_back(std::move(item));
  }

  applyFilters(items);
  return items;
}

void applyFilters(std::vector<Item>& items) {
  for (auto& item : items) {
    if (archiveContains(item.title)) {
      item.state = ItemState::Archived;
      item.matchedFilters.push_back("Archive");
      continue;
    }

    if (!taiga::settings.boolValue("rss.torrent.filters.enabled", true)) continue;

    for (const auto& value : filterSettings()) {
      const auto object = value.toObject();
      if (!object.value("enabled").toBool(true)) continue;
      applyNamedFilter(item, object.value("name").toString());
    }
  }
}

QString archivePath() {
  return u"%1/v1/rss/archive.xml"_s.arg(QString::fromStdString(taiga::get_data_path()));
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
    case ItemState::Archived:
      return "Archived";
  }
  return "New";
}

}  // namespace track::torrent
