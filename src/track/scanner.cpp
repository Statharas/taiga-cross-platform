/**
 * Taiga
 * Copyright (C) 2010-2025, Eren Okka
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "scanner.hpp"

#include <QDirIterator>
#include <QSet>
#include <optional>

#include "track/episode.hpp"
#include "track/recognition.hpp"
#include "media/anime_db.hpp"

namespace track {

std::optional<QString> findEpisode(const QString& path, const int anime_id,
                                   const int episode_number) {
  QDirIterator it{path, QDir::Files, QDirIterator::Subdirectories};

  while (it.hasNext()) {
    const auto info = it.nextFileInfo();

    if (!info.isFile()) continue;

    auto episode = recognition::parseFileInfo(info);

    if (QString::fromStdString(episode.element(anitomy::ElementKind::Episode)).toInt() !=
        episode_number) {
      continue;
    }

    if (track::recognition::identify(episode) != anime_id) continue;

    return info.filePath();
  }

  return std::nullopt;
}

std::optional<QString> findFolder(const QString& path, const int anime_id) {
  QDirIterator it{path, QDir::Dirs, QDirIterator::Subdirectories};

  while (it.hasNext()) {
    const auto info = it.nextFileInfo();

    if (!info.isDir()) continue;

    auto episode = recognition::parseFileInfo(info);

    if (track::recognition::identify(episode) != anime_id) continue;

    return info.filePath();
  }

  return std::nullopt;
}

ScanSummary scanAvailableEpisodes(const std::vector<std::string>& libraryFolders) {
  ScanSummary summary;
  QSet<int> animeIds;
  anime::db.clearAvailableEpisodes();

  for (const auto& folder : libraryFolders) {
    const auto root = QString::fromStdString(folder);
    if (root.isEmpty()) continue;
    if (!QDir{root}.exists()) continue;
    ++summary.folders;

    QDirIterator it{root, QDir::Files, QDirIterator::Subdirectories};
    while (it.hasNext()) {
      const auto info = it.nextFileInfo();
      if (!info.isFile()) continue;
      ++summary.files;

      auto episode = recognition::parseFileInfo(info);
      const auto animeId = track::recognition::identify(episode);
      if (animeId <= 0) continue;
      const auto episodeNumber =
          QString::fromStdString(episode.element(anitomy::ElementKind::Episode)).toInt();
      if (episodeNumber > 0) {
        anime::db.setAvailableEpisode(animeId, episodeNumber, info.absoluteFilePath());
      }

      ++summary.recognized;
      animeIds.insert(animeId);
    }
  }

  summary.anime = animeIds.size();
  return summary;
}

}  // namespace track
