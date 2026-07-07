/**
 * Taiga
 * Copyright (C) 2010-2024, Eren Okka
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "statistics_widget.hpp"

#include <QDirIterator>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QVBoxLayout>
#include <algorithm>

#include "gui/utils/format.hpp"
#include "media/anime_db.hpp"
#include "media/anime_list.hpp"
#include "taiga/path.hpp"

namespace gui {

namespace {

struct FileStats {
  int count = 0;
  qint64 size = 0;
};

FileStats fileStats(const QString& path, const QStringList& filters) {
  FileStats stats;
  QDirIterator it(path, filters, QDir::Files, QDirIterator::Subdirectories);
  while (it.hasNext()) {
    it.next();
    ++stats.count;
    stats.size += it.fileInfo().size();
  }
  return stats;
}

QString formatSize(qint64 bytes) {
  static constexpr std::array units{"B", "KiB", "MiB", "GiB"};
  auto size = static_cast<double>(bytes);
  int unit = 0;
  while (size >= 1024.0 && unit < static_cast<int>(units.size()) - 1) {
    size /= 1024.0;
    ++unit;
  }
  return unit == 0 ? QString("%1 %2").arg(bytes).arg(units[unit])
                   : QString("%1 %2").arg(size, 0, 'f', 1).arg(units[unit]);
}

QString formatWatchTime(int minutes) {
  const auto hours = minutes / 60;
  const auto days = hours / 24;
  if (days > 0) return QObject::tr("%1 day(s), %2 hour(s)").arg(days).arg(hours % 24);
  return QObject::tr("%1 hour(s)").arg(hours);
}

}  // namespace

StatisticsWidget::StatisticsWidget(QWidget* parent) : QWidget(parent) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(16, 16, 16, 16);
  layout->setSpacing(12);

  QMap<anime::list::Status, int> statusCounts;
  int totalEpisodes = 0;
  int watchedEpisodes = 0;
  int ratedEntries = 0;
  int scoreSum = 0;
  int watchedMinutes = 0;
  QMap<int, int> scoreCounts;

  for (const auto& entry : anime::db.entries()) {
    statusCounts[entry.status] += 1;
    watchedEpisodes += entry.watched_episodes;
    if (entry.score > 0) {
      ++ratedEntries;
      scoreSum += entry.score;
      scoreCounts[std::clamp(entry.score / 10, 1, 10)] += 1;
    }
    if (const auto anime = anime::db.item(entry.anime_id)) {
      if (anime->episode_count > 0) totalEpisodes += anime->episode_count;
      if (anime->episode_length > 0) watchedMinutes += entry.watched_episodes * anime->episode_length;
    }
  }

  auto* overview = new QGroupBox(tr("Anime list"), this);
  auto* overviewLayout = new QFormLayout(overview);
  overviewLayout->addRow(tr("Total entries"), new QLabel(QString::number(anime::db.entries().size()), overview));
  overviewLayout->addRow(tr("Watched episodes"), new QLabel(QString::number(watchedEpisodes), overview));
  overviewLayout->addRow(tr("Known episodes"), new QLabel(QString::number(totalEpisodes), overview));
  overviewLayout->addRow(tr("Life spent watching"), new QLabel(formatWatchTime(watchedMinutes), overview));
  overviewLayout->addRow(tr("Mean score"),
                         new QLabel(ratedEntries ? formatListScore(scoreSum / ratedEntries) : "-", overview));
  layout->addWidget(overview);

  auto* statuses = new QGroupBox(tr("Status"), this);
  auto* statusesLayout = new QFormLayout(statuses);
  for (const auto status : anime::list::kStatuses) {
    statusesLayout->addRow(formatListStatus(status),
                           new QLabel(QString::number(statusCounts[status]), statuses));
  }
  layout->addWidget(statuses);

  auto* scores = new QGroupBox(tr("Score distribution"), this);
  auto* scoresLayout = new QFormLayout(scores);
  const auto maxScoreCount = std::max(1, std::ranges::max(scoreCounts.values()));
  for (int score = 10; score >= 1; --score) {
    auto* bar = new QProgressBar(scores);
    bar->setRange(0, maxScoreCount);
    bar->setValue(scoreCounts[score]);
    bar->setFormat(QString::number(scoreCounts[score]));
    scoresLayout->addRow(QString::number(score), bar);
  }
  layout->addWidget(scores);

  const auto dataPath = QString::fromStdString(taiga::get_data_path());
  const auto imageStats = fileStats(dataPath + "/v1/db/image", {"*.jpg", "*.jpeg", "*.png", "*.webp"});
  const auto torrentStats = fileStats(dataPath + "/v1/rss", {"*.torrent"});
  auto* database = new QGroupBox(tr("Database"), this);
  auto* databaseLayout = new QFormLayout(database);
  databaseLayout->addRow(tr("Anime database"), new QLabel(QString::number(anime::db.items().size()), database));
  databaseLayout->addRow(tr("Poster images"),
                         new QLabel(tr("%1 (%2)").arg(imageStats.count).arg(formatSize(imageStats.size)), database));
  databaseLayout->addRow(tr("Torrent files"),
                         new QLabel(tr("%1 (%2)").arg(torrentStats.count).arg(formatSize(torrentStats.size)), database));
  layout->addWidget(database);
  layout->addStretch();
}

}  // namespace gui
