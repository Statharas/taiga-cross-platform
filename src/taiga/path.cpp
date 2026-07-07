/**
 * Taiga
 * Copyright (C) 2010-2024, Eren Okka
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

#include "path.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>
#include <format>

#include "taiga/config.h"

namespace taiga {

// Returns current path in portable mode, AppData location otherwise
std::string get_data_path() {
  const auto overridePath = qEnvironmentVariable("TAIGA_DATA_PATH");
  if (!overridePath.isEmpty()) {
    QDir{}.mkpath(overridePath);
    return overridePath.toStdString();
  }
#ifdef TAIGA_PORTABLE
  return std::format("{}/data", QCoreApplication::applicationDirPath().toStdString());
#else
  auto location = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  if (location.isEmpty()) {
    location = QDir::home().filePath(".local/share/taiga");
  }
  QDir{}.mkpath(location);
  return std::format("{}/data", location.toStdString());
#endif
}

}  // namespace taiga
