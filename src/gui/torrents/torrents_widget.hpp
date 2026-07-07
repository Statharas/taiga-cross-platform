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
class QPoint;
class QTableWidget;

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
  void openSelected() const;
  void populate();
  void showContextMenu(const QPoint& position);
  void showSettings();
  void setBusy(bool busy);

  QLabel* statusLabel_ = nullptr;
  QLineEdit* filterEdit_ = nullptr;
  QTableWidget* table_ = nullptr;
  std::vector<track::torrent::Item> items_;
};

}  // namespace gui
