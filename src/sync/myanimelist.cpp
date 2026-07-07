/**
 * Taiga
 * Copyright (C) 2010-2024, Eren Okka
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "myanimelist.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>
#include <memory>

#include "base/string.hpp"
#include "media/anime_db.hpp"
#include "media/anime_season.hpp"
#include "media/anime_season_db.hpp"
#include "sync/myanimelist_parsers.hpp"
#include "taiga/accounts.hpp"
#include "taiga/network.hpp"

namespace taiga_sync::myanimelist {

namespace {

constexpr int kPageLimit = 100;
constexpr int kSeasonPageLimit = 500;

QString fields() {
  return "id,title,main_picture,alternative_titles,start_date,end_date,synopsis,mean,"
         "popularity,num_episodes,status,media_type,average_episode_duration,rating,"
         "genres,studios,updated_at,list_status{status,score,num_episodes_watched,"
         "is_rewatching,updated_at,start_date,finish_date,num_times_rewatched,tags,comments}";
}

QNetworkRequest listRequest(const QString& username, int offset) {
  QUrl url{u"https://api.myanimelist.net/v2/users/%1/animelist"_s.arg(username)};
  QUrlQuery query;
  query.addQueryItem("limit", QString::number(kPageLimit));
  query.addQueryItem("offset", QString::number(offset));
  query.addQueryItem("nsfw", "true");
  query.addQueryItem("fields", fields());
  url.setQuery(query);

  QNetworkRequest request{url};
  request.setRawHeader("Authorization",
                       "Bearer " +
                           QByteArray::fromStdString(taiga::accounts.myanimelistAccessToken()));
  return request;
}

QString seasonSlug(const anime::SeasonName name) {
  switch (name) {
    case anime::SeasonName::Winter:
      return "winter";
    case anime::SeasonName::Spring:
      return "spring";
    case anime::SeasonName::Summer:
      return "summer";
    case anime::SeasonName::Fall:
      return "fall";
    default:
      return {};
  }
}

QNetworkRequest seasonRequest(const anime::Season season, int offset) {
  QUrl url{u"https://api.myanimelist.net/v2/anime/season/%1/%2"_s.arg(
      static_cast<int>(season.year)).arg(seasonSlug(season.name))};
  QUrlQuery query;
  query.addQueryItem("limit", QString::number(kSeasonPageLimit));
  query.addQueryItem("offset", QString::number(offset));
  query.addQueryItem("nsfw", "true");
  query.addQueryItem("fields", fields());
  url.setQuery(query);

  QNetworkRequest request{url};
  request.setRawHeader("Authorization",
                       "Bearer " +
                           QByteArray::fromStdString(taiga::accounts.myanimelistAccessToken()));
  return request;
}

int nextOffset(const QJsonObject& root) {
  const auto next = root.value("paging").toObject().value("next").toString();
  if (next.isEmpty()) return -1;
  bool ok = false;
  const auto value = QUrlQuery{QUrl{next}.query()}.queryItemValue("offset").toInt(&ok);
  return ok ? value : -1;
}

}  // namespace

Service* Service::instance() {
  static Service service;
  return &service;
}

void Service::fetchListEntries(std::function<void(bool, const QString&)> done) {
  if (taiga::accounts.myanimelistAccessToken().empty()) {
    if (done) done(false, "MyAnimeList access token is not configured.");
    return;
  }

  auto username = QString::fromStdString(taiga::accounts.myanimelistUsername()).trimmed();
  if (username.isEmpty()) username = "@me";

  fetchListEntriesPage(username, 0, true, 0, std::move(done));
}

void Service::fetchListEntriesPage(const QString& username, int offset, bool clearFirst,
                                   int updatedCount,
                                   std::function<void(bool, const QString&)> done) {
  auto* reply = taiga::network()->get(listRequest(username, offset));
  connect(reply, &QNetworkReply::finished, this,
          [this, reply, username, clearFirst, updatedCount, done = std::move(done)]() mutable {
            if (reply->error() != QNetworkReply::NoError) {
              const auto message = reply->errorString();
              reply->deleteLater();
              if (done) done(false, message);
              return;
            }

            const auto document = QJsonDocument::fromJson(reply->readAll());
            reply->deleteLater();
            if (!document.isObject()) {
              if (done) done(false, "Could not parse MyAnimeList anime list.");
              return;
            }

            if (clearFirst) anime::db.clearEntries();

            const auto root = document.object();
            auto count = updatedCount;
            for (const auto& value : root.value("data").toArray()) {
              const auto object = value.toObject();
              const auto node = object.value("node");
              const auto media = parseMedia(node);
              if (media) anime::db.updateItem(*media);
              if (media) {
                const auto entry = parseListEntry(object.value("list_status"), media->id);
                if (entry) {
                  anime::db.updateEntry(*entry);
                  ++count;
                }
              }
            }

            const auto next = nextOffset(root);
            if (next >= 0) {
              fetchListEntriesPage(username, next, false, count, std::move(done));
              return;
            }

            if (done) {
              done(true, QString{"Synchronized %1 MyAnimeList entries."}.arg(count));
            }
          });
}

void Service::fetchSeason(const anime::Season season,
                          std::function<void(bool, const QString&)> done) {
  if (taiga::accounts.myanimelistAccessToken().empty()) {
    if (done) done(false, "MyAnimeList access token is not configured.");
    return;
  }
  if (!season) {
    if (done) done(false, "No valid season was selected.");
    return;
  }

  fetchSeasonPage(season, 0, std::make_shared<QList<int>>(), std::move(done));
}

void Service::fetchSeasonPage(const anime::Season season, int offset,
                              const std::shared_ptr<QList<int>>& ids,
                              std::function<void(bool, const QString&)> done) {
  auto* reply = taiga::network()->get(seasonRequest(season, offset));
  connect(reply, &QNetworkReply::finished, this,
          [this, reply, season, ids, done = std::move(done)]() mutable {
            if (reply->error() != QNetworkReply::NoError) {
              const auto message = reply->errorString();
              reply->deleteLater();
              if (done) done(false, message);
              return;
            }

            const auto document = QJsonDocument::fromJson(reply->readAll());
            reply->deleteLater();
            if (!document.isObject()) {
              if (done) done(false, "Could not parse MyAnimeList season data.");
              return;
            }

            const auto root = document.object();
            for (const auto& value : root.value("data").toArray()) {
              const auto media = parseMedia(value.toObject().value("node"));
              if (!media) continue;
              anime::db.updateItem(*media);
              ids->push_back(media->id);
            }

            const auto next = nextOffset(root);
            if (next >= 0) {
              fetchSeasonPage(season, next, ids, std::move(done));
              return;
            }

            anime::season_db.set(season, *ids);
            if (done) {
              done(true, QString{"Fetched %1 MyAnimeList season entries."}.arg(ids->size()));
            }
          });
}

}  // namespace taiga_sync::myanimelist
