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
#include <QString>
#include <QStringList>
#include <vector>

namespace track::torrent {

enum class ItemState {
  Normal,
  Selected,
  Preferred,
  Discarded,
  Archived,
};

struct Item {
  QString title;
  QString link;
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
  ItemState state = ItemState::Normal;
  QStringList matchedFilters;
};

std::vector<Item> parseFeed(const QByteArray& data);
void applyFilters(std::vector<Item>& items);
bool archiveContains(const QString& title);
void archiveItems(const std::vector<Item>& items);
int archiveCount();
QString archivePath();
QString stateText(ItemState state);

}  // namespace track::torrent
