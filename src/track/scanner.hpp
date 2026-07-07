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

#pragma once

#include <QString>
#include <vector>
#include <optional>

namespace track {

struct ScanSummary {
  int folders = 0;
  int files = 0;
  int recognized = 0;
  int anime = 0;
};

std::optional<QString> findEpisode(const QString& path, const int anime_id,
                                   const int episode_number);
std::optional<QString> findFolder(const QString& path, const int anime_id);
ScanSummary scanAvailableEpisodes(const std::vector<std::string>& libraryFolders);

}  // namespace track
