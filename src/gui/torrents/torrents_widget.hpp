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

#include <QWidget>

#include "track/torrent_feed.hpp"

class QLabel;
class QLineEdit;
class QNetworkReply;
class QPoint;
class QAction;
class QTableWidget;
class QTableWidgetItem;
class QTimer;

namespace gui {

class TorrentsWidget final : public QWidget {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(TorrentsWidget)

public:
  explicit TorrentsWidget(QWidget* parent = nullptr);
  ~TorrentsWidget() = default;

private:
  void archiveSelected();
  void archiveVisible();
  void fetch();
  void fetchFromCache();
  void discardSameAnime();
  void discardSameGroup();
  void handleItemChanged(QTableWidgetItem* item);
  void openMoreTorrents() const;
  void openTorrentInfo() const;
  void openSelected() const;
  void preferSameGroup();
  void populate();
  void resetAutoCheckTimer();
  void searchService() const;
  void showContextMenu(const QPoint& position);
  void showSettings();
  void setBusy(bool busy);
  void startDownload(const track::torrent::Item& item) const;
  void tickAutoCheck();
  const track::torrent::Item* currentItem() const;

  QLabel* statusLabel_ = nullptr;
  QLineEdit* filterEdit_ = nullptr;
  QAction* refreshAction_ = nullptr;
  QTableWidget* table_ = nullptr;
  QTimer* autoCheckTimer_ = nullptr;
  bool populating_ = false;
  int autoCheckRemaining_ = 0;
  int lastCheckedRow_ = -1;
  std::vector<track::torrent::Item> items_;
};

}  // namespace gui
