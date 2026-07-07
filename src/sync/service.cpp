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

#include "service.hpp"

#include <QMap>
#include <utility>

#include "sync/anilist.hpp"
#include "sync/anilist_utils.hpp"
#include "sync/kitsu.hpp"
#include "sync/kitsu_utils.hpp"
#include "sync/myanimelist.hpp"
#include "sync/myanimelist_utils.hpp"
#include "media/anime_season.hpp"
#include "taiga/network.hpp"
#include "taiga/settings.hpp"

namespace taiga_sync {

Service::Service() : QObject{nullptr}, manager_{taiga::network()} {
  api_.setCommonHeaders(taiga::NetworkAccessManager::commonHeaders());
}

ServiceId currentServiceId() {
  const auto slug = QString::fromStdString(taiga::settings.service());
  return serviceIdFromSlug(slug);
}

ServiceId serviceIdFromSlug(const QString& slug) {
  static const QMap<QString, ServiceId> services{
      {"myanimelist", ServiceId::MyAnimeList},
      {"kitsu", ServiceId::Kitsu},
      {"anilist", ServiceId::AniList},
  };
  return services.value(slug, ServiceId::Unknown);
}

QString serviceName(const ServiceId serviceId) {
  // clang-format off
  switch (serviceId) {
    case ServiceId::MyAnimeList: return "MyAnimeList";
    case ServiceId::Kitsu: return "Kitsu";
    case ServiceId::AniList: return "AniList";  
    case ServiceId::Unknown: break;
  }
  // clang-format on
  return "Taiga";
}

QString serviceSlug(const ServiceId serviceId) {
  // clang-format off
  switch (serviceId) {
    case ServiceId::MyAnimeList: return "myanimelist";
    case ServiceId::Kitsu: return "kitsu";
    case ServiceId::AniList: return "anilist";
    case ServiceId::Unknown: break;
  }
  // clang-format on
  return "taiga";
}

void fetchAnime(const int id) {
  switch (currentServiceId()) {
    case ServiceId::MyAnimeList:
      break;
    case ServiceId::Kitsu:
      break;
    case ServiceId::AniList:
      anilist::Service::instance()->fetchAnime(id);
      break;
    case ServiceId::Unknown:
      break;
  }
}

void fetchSeason(const anime::Season season, std::function<void(bool, const QString&)> done) {
  switch (currentServiceId()) {
    case ServiceId::MyAnimeList:
      myanimelist::Service::instance()->fetchSeason(season, std::move(done));
      break;
    case ServiceId::Kitsu:
      kitsu::Service::instance()->fetchSeason(season, std::move(done));
      break;
    case ServiceId::AniList:
      anilist::Service::instance()->fetchSeason(season, std::move(done));
      break;
    case ServiceId::Unknown:
      if (done) done(false, "No active metadata service is configured.");
      break;
  }
}

void searchTitle(const QString& query, std::function<void(bool, const QString&)> done) {
  const auto trimmed = query.trimmed();
  if (trimmed.isEmpty()) {
    if (done) done(false, "Search query is empty.");
    return;
  }

  switch (currentServiceId()) {
    case ServiceId::AniList:
      anilist::Service::instance()->search(trimmed, std::move(done));
      break;
    case ServiceId::MyAnimeList:
      myanimelist::Service::instance()->search(trimmed, std::move(done));
      break;
    case ServiceId::Kitsu:
      kitsu::Service::instance()->search(trimmed, std::move(done));
      break;
    case ServiceId::Unknown:
      if (done) done(false, "No active metadata service is configured.");
      break;
  }
}

void synchronize(std::function<void(bool, const QString&)> done) {
  switch (currentServiceId()) {
    case ServiceId::MyAnimeList:
      myanimelist::Service::instance()->fetchListEntries(std::move(done));
      break;
    case ServiceId::Kitsu:
      kitsu::Service::instance()->fetchListEntries(std::move(done));
      break;
    case ServiceId::AniList:
      anilist::Service::instance()->fetchListEntries(std::move(done));
      break;
    case ServiceId::Unknown:
      if (done) done(false, "No active metadata service is configured.");
      break;
  }
}

QString animePageUrl(const int id) {
  switch (currentServiceId()) {
    case ServiceId::MyAnimeList:
      return QString::fromStdString(myanimelist::animePageUrl(id));
    case ServiceId::Kitsu:
      return QString::fromStdString(kitsu::animePageUrl(id));
    case ServiceId::AniList:
      return QString::fromStdString(anilist::animePageUrl(id));
    case ServiceId::Unknown:
      break;
  }
  return {};
}

}  // namespace taiga_sync
