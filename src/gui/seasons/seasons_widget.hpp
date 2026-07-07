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

#include <QComboBox>
#include <QLabel>
#include <QPair>
#include <QVector>
#include <QScrollArea>
#include <QTableWidget>
#include <QWidget>

#include "media/anime.hpp"
#include "media/anime_season.hpp"

namespace gui {

class SeasonsWidget final : public QWidget {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(SeasonsWidget)

public:
  explicit SeasonsWidget(QWidget* parent = nullptr);
  ~SeasonsWidget() = default;

private:
  enum class GroupBy { Type, Season };
  enum class SortBy { Title, Score, Popularity };
  enum class ViewAs { Details, Table };

  void initControls();
  void scheduleUpdate();
  void updateView();
  void updateTable(const QVector<QPair<const Anime*, anime::Season>>& rows);
  void updateDetails(const QVector<QPair<const Anime*, anime::Season>>& rows);
  QWidget* createDetailsCard(const Anime& anime, const anime::Season& season);
  void showMediaMenu(int animeId) const;
  void showTableContextMenu(const QPoint& position) const;

  QComboBox* m_yearCombo = nullptr;
  QComboBox* m_seasonCombo = nullptr;
  QComboBox* m_groupCombo = nullptr;
  QComboBox* m_sortCombo = nullptr;
  QComboBox* m_viewCombo = nullptr;
  QScrollArea* m_scrollArea = nullptr;
  QWidget* m_detailsWidget = nullptr;
  QTableWidget* m_table = nullptr;
  QLabel* m_statusLabel = nullptr;
  bool m_refreshingPosters = false;
  bool m_updateQueued = false;
};

}  // namespace gui
