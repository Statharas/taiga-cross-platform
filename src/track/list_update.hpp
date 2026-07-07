/**
 * Taiga
 * Copyright (C) 2010-2025, Eren Okka
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include <QCoreApplication>
#include <QObject>
#include <QTimer>
#include <optional>

#include "media/anime_list.hpp"
#include "track/episode.hpp"

namespace track::list_update {

struct PreparedUpdate {
  ListEntry entry;
  QString message;
};

std::optional<PreparedUpdate> prepareUpdate(const Episode& episode);

class Manager final : public QObject {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(Manager)

public:
  explicit Manager(QObject* parent);

  void init();
  int remainingSeconds() const;
  QString statusText() const;

signals:
  void countdownChanged(int seconds) const;
  void statusChanged(QString status) const;
  void updateFinished(bool ok, QString message) const;

private:
  void onCurrentEpisodeChanged(std::optional<Episode> episode);
  void tick();
  void updateNow();
  void setStatus(QString status);

  QTimer* timer_ = nullptr;
  std::optional<Episode> pendingEpisode_;
  int remainingSeconds_ = 0;
  QString statusText_;
  bool updating_ = false;
};

inline Manager* manager() {
  static auto manager = new Manager(qApp);
  return manager;
}

}  // namespace track::list_update
