/**
 * Taiga
 * Copyright (C) 2010-2024, Eren Okka
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <optional>
#include <vector>

namespace track::torrent {

enum class Category {
  Anime,
  Batch,
  Other,
};

enum class ItemState {
  Normal,
  Selected,
  Preferred,
  Discarded,
  DiscardedInactive,
  DiscardedHidden,
  Archived,
};

struct Item {
  int anime_id = 0;
  QString title;
  QString link;
  QString info_link;
  QString magnet_link;
  QString description;
  QString category;
  QString episode;
  QString filename;
  QString group;
  QString leechers;
  QString seeders;
  QString size;
  QString downloads;
  QString published;
  QString source;
  QString video;
  Category torrent_category = Category::Anime;
  ItemState state = ItemState::Normal;
  QStringList matchedFilters;
};

struct Feed {
  QString title;
  QString link;
  std::vector<Item> items;
};

struct DownloadPlan {
  Item item;
  QUrl url;
  QString save_path;
  bool magnet = false;
};

Feed parseFeedDocument(const QByteArray& data);
std::vector<Item> parseFeed(const QByteArray& data);
void applyFilters(std::vector<Item>& items);
void examineItems(std::vector<Item>& items, const QString& channelLink = {});
void sortItems(std::vector<Item>& items);
QList<Item> selectedItems(const std::vector<Item>& items);
QList<Item> sortedDownloadQueue(const std::vector<Item>& items);
std::optional<DownloadPlan> downloadPlan(const Item& item);
bool saveFeedCache(const Feed& feed, const QByteArray& data);
std::optional<Feed> loadFeedCache(const QString& source);
bool archiveContains(const QString& title);
void archiveItems(const std::vector<Item>& items);
int archiveCount();
QString archivePath();
QString cachePath(const QString& source);
QString categoryText(Category category);
QString stateText(ItemState state);
bool isDiscarded(ItemState state);
bool isHidden(ItemState state);

}  // namespace track::torrent
