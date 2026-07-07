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

#include "settings.hpp"

#include <QFile>
#include <QJsonArray>
#include <ranges>

#include "base/string.hpp"
#include "compat/settings.hpp"
#include "sync/service.hpp"
#include "taiga/accounts.hpp"
#include "taiga/path.hpp"
#include "taiga/version.hpp"

namespace taiga {

void Settings::init() const {
  const auto appVersion = taiga::version().to_string();

  // v1 to v2
  if (!QFile::exists(fileName())) {
    compat::v1::readSettings(std::format("{}/v1/settings.xml", get_data_path()), *this, accounts);
    setValue("meta.version", appVersion);
    return;
  }

  // v2.x
  const auto fileVersion = value("meta.version").toString().toStdString();
  if (fileVersion != appVersion) {
    setValue("meta.version", appVersion);
  }
}

QString Settings::fileName() const {
  return u"%1/settings.json"_s.arg(QString::fromStdString(get_data_path()));
}

////////////////////////////////////////////////////////////////////////////////

Qt::ColorScheme Settings::appColorScheme() const {
  return value("app.colorScheme", static_cast<int>(Qt::ColorScheme::Unknown))
      .value<Qt::ColorScheme>();
}

bool Settings::boolValue(QAnyStringView key, bool defaultValue) const {
  return value(key, defaultValue).toBool();
}

int Settings::intValue(QAnyStringView key, int defaultValue) const {
  return value(key, defaultValue).toInt();
}

std::string Settings::service() const {
  return value("v1.service", taiga_sync::serviceSlug(taiga_sync::ServiceId::AniList))
      .toString()
      .toStdString();
}

QString Settings::stringValue(QAnyStringView key, const QString& defaultValue) const {
  return value(key, defaultValue).toString();
}

std::vector<std::string> Settings::libraryFolders() const {
  const auto variants = value("library.folders").toJsonArray().toVariantList();
  std::vector<std::string> folders;
  folders.reserve(variants.size());
  for (const auto& variant : variants) {
    folders.emplace_back(variant.toString().toStdString());
  }
  return folders;
}

std::chrono::milliseconds Settings::mediaDetectionInterval() const {
  const auto interval = value("track.detection.interval", 3000).toInt();
  return std::chrono::milliseconds{interval};
}

////////////////////////////////////////////////////////////////////////////////

void Settings::setAppColorScheme(const Qt::ColorScheme scheme) const {
  setValue("app.colorScheme", static_cast<int>(scheme));
}

void Settings::setBoolValue(QAnyStringView key, bool value) const {
  setValue(key, value);
}

void Settings::setIntValue(QAnyStringView key, int value) const {
  setValue(key, value);
}

void Settings::setService(const std::string& service) const {
  setValue("v1.service", service);
}

void Settings::setStringValue(QAnyStringView key, const QString& value) const {
  setValue(key, value);
}

void Settings::setLibraryFolders(std::vector<std::string> folders) const {
  QStringList list;
  list.reserve(static_cast<qsizetype>(folders.size()));
  for (const auto& folder : folders) {
    list.emplace_back(QString::fromStdString(folder));
  }
  setValue("library.folders", QJsonArray::fromStringList(list));
}

void Settings::setMediaDetectionInterval(const std::chrono::milliseconds interval) const {
  setValue("track.detection.interval", static_cast<qlonglong>(interval.count()));
}

}  // namespace taiga
