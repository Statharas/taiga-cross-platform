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

#include <QList>
#include <QWidget>
#include <optional>

#include "media/anime.hpp"
#include "track/episode.hpp"

class QLabel;

namespace gui {

class NowPlayingPageWidget final : public QWidget {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(NowPlayingPageWidget)

public:
  explicit NowPlayingPageWidget(QWidget* parent = nullptr);
  ~NowPlayingPageWidget() = default;

private:
  void reset();
  void setPlaying(const track::Episode& episode);
  void refresh();
  void refreshPoster();

  QLabel* m_posterLabel = nullptr;
  QLabel* m_titleLabel = nullptr;
  QLabel* m_actionLabel = nullptr;
  QLabel* m_alternativeTitlesLabel = nullptr;
  QLabel* m_detailsLabel = nullptr;
  QLabel* m_synopsisLabel = nullptr;
  QLabel* m_emptyLabel = nullptr;
  QList<QWidget*> m_episodeWidgets;

  std::optional<Anime> m_anime;
  std::optional<track::Episode> m_episode;
};

}  // namespace gui
