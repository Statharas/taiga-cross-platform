/**
 * Taiga
 * Copyright (C) 2010-2024, Eren Okka
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "kitsu.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>
#include <memory>

#include "media/anime_season.hpp"
#include "media/anime_season_db.hpp"
#include "media/anime_db.hpp"
#include "sync/kitsu_parsers.hpp"
#include "sync/kitsu_ratings.hpp"
#include "taiga/accounts.hpp"
#include "taiga/network.hpp"
#include "taiga/settings.hpp"

namespace taiga_sync::kitsu {

namespace {

constexpr int kPageLimit = 100;

QByteArray bearerHeader() {
  return "Bearer " + QByteArray::fromStdString(taiga::accounts.kitsuAccessToken());
}

QNetworkRequest apiRequest(QUrl url) {
  QNetworkRequest request{url};
  request.setRawHeader("Accept", "application/vnd.api+json");
  if (!taiga::accounts.kitsuAccessToken().empty()) {
    request.setRawHeader("Authorization", bearerHeader());
  }
  return request;
}

QUrl listUrl(const QString& userId, int offset) {
  QUrl url{"https://kitsu.app/api/edge/library-entries"};
  QUrlQuery query;
  query.addQueryItem("filter[user_id]", userId);
  query.addQueryItem("filter[kind]", "anime");
  query.addQueryItem("include", "anime");
  query.addQueryItem("page[limit]", QString::number(kPageLimit));
  query.addQueryItem("page[offset]", QString::number(offset));
  query.addQueryItem("fields[libraryEntries]",
                     "status,progress,reconsumeCount,reconsuming,notes,private,ratingTwenty,"
                     "startedAt,finishedAt,updatedAt,anime");
  query.addQueryItem("fields[anime]",
                     "canonicalTitle,titles,subtype,status,synopsis,startDate,endDate,"
                     "episodeCount,episodeLength,averageRating,popularityRank,ageRating,posterImage");
  url.setQuery(query);
  return url;
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

QUrl seasonUrl(const anime::Season season, int offset) {
  QUrl url{"https://kitsu.app/api/edge/anime"};
  QUrlQuery query;
  query.addQueryItem("filter[season]", seasonSlug(season.name));
  query.addQueryItem("filter[season_year]", QString::number(static_cast<int>(season.year)));
  query.addQueryItem("page[limit]", QString::number(kPageLimit));
  query.addQueryItem("page[offset]", QString::number(offset));
  query.addQueryItem("sort", "-user_count");
  query.addQueryItem("fields[anime]",
                     "canonicalTitle,titles,subtype,status,synopsis,startDate,endDate,"
                     "episodeCount,episodeLength,averageRating,popularityRank,ageRating,posterImage");
  url.setQuery(query);
  return url;
}

QUrl searchUrl(const QString& queryText) {
  QUrl url{"https://kitsu.app/api/edge/anime"};
  QUrlQuery query;
  query.addQueryItem("filter[text]", queryText);
  query.addQueryItem("page[limit]", "50");
  query.addQueryItem("fields[anime]",
                     "canonicalTitle,titles,subtype,status,synopsis,startDate,endDate,"
                     "episodeCount,episodeLength,averageRating,popularityRank,ageRating,posterImage");
  url.setQuery(query);
  return url;
}

QUrl updateListEntryUrl(const anime::list::Entry& entry) {
  QUrl url{entry.id == anime::list::kUnknownId
               ? "https://kitsu.app/api/edge/library-entries"
               : QString{"https://kitsu.app/api/edge/library-entries/%1"}.arg(entry.id)};
  QUrlQuery query;
  query.addQueryItem("fields[libraryEntries]",
                     "status,progress,reconsumeCount,reconsuming,notes,private,ratingTwenty,"
                     "startedAt,finishedAt,updatedAt,anime");
  url.setQuery(query);
  return url;
}

QString dateString(const FuzzyDate& date) {
  return date ? QString::fromStdString(date.to_string()) : QString{};
}

QJsonObject resourceIdentifier(const QString& type, const QString& id) {
  return {
      {"type", type},
      {"id", id},
  };
}

QByteArray updateListEntryBody(const anime::list::Entry& entry, const QString& userId) {
  QJsonObject attributes{
      {"status", fromListStatus(entry.status)},
      {"progress", entry.watched_episodes},
      {"private", entry.is_private},
      {"reconsumeCount", entry.rewatched_times},
      {"reconsuming", entry.rewatching},
      {"notes", QString::fromStdString(entry.notes)},
  };
  if (entry.score > 0) {
    attributes["ratingTwenty"] = fromListScore(entry.score);
  } else {
    attributes["ratingTwenty"] = QJsonValue::Null;
  }
  if (entry.date_started) attributes["startedAt"] = dateString(entry.date_started);
  if (entry.date_completed) attributes["finishedAt"] = dateString(entry.date_completed);

  QJsonObject relationships{
      {"anime", QJsonObject{{"data", resourceIdentifier("anime", QString::number(entry.anime_id))}}},
  };
  if (!userId.isEmpty()) {
    relationships["user"] = QJsonObject{{"data", resourceIdentifier("users", userId)}};
  }

  QJsonObject data{
      {"type", "libraryEntries"},
      {"attributes", attributes},
      {"relationships", relationships},
  };
  if (entry.id != anime::list::kUnknownId) data["id"] = QString::number(entry.id);

  return QJsonDocument{QJsonObject{{"data", data}}}.toJson(QJsonDocument::Compact);
}

int nextOffset(const QJsonObject& root) {
  const auto next = root.value("links").toObject().value("next").toString();
  if (next.isEmpty()) return -1;
  bool ok = false;
  const auto value = QUrlQuery{QUrl{next}.query()}.queryItemValue("page[offset]").toInt(&ok);
  return ok ? value : -1;
}

int animeIdForEntry(const QJsonObject& entry) {
  return entry.value("relationships")
      .toObject()
      .value("anime")
      .toObject()
      .value("data")
      .toObject()
      .value("id")
      .toString()
      .toInt();
}

}  // namespace

Service* Service::instance() {
  static Service service;
  return &service;
}

void Service::search(const QString& query, std::function<void(bool, const QString&)> done) {
  auto* reply = taiga::network()->get(apiRequest(searchUrl(query)));
  connect(reply, &QNetworkReply::finished, this, [reply, done = std::move(done)]() mutable {
    if (reply->error() != QNetworkReply::NoError) {
      const auto message = reply->errorString();
      reply->deleteLater();
      if (done) done(false, message);
      return;
    }

    const auto document = QJsonDocument::fromJson(reply->readAll());
    reply->deleteLater();
    if (!document.isObject()) {
      if (done) done(false, "Could not parse Kitsu search results.");
      return;
    }

    int updated = 0;
    for (const auto& value : document.object().value("data").toArray()) {
      const auto media = parseMedia(value.toObject());
      if (!media) continue;
      anime::db.updateItem(*media);
      ++updated;
    }

    if (done) done(true, QString{"Found %1 Kitsu search result(s)."}.arg(updated));
  });
}

void Service::fetchListEntries(std::function<void(bool, const QString&)> done) {
  if (taiga::accounts.kitsuAccessToken().empty()) {
    if (done) done(false, "Kitsu access token is not configured.");
    return;
  }

  const auto userId = QString::fromStdString(taiga::accounts.kitsuUserId()).trimmed();
  if (!userId.isEmpty()) {
    fetchListEntriesPage(userId, 0, true, 0, std::move(done));
    return;
  }

  fetchAuthenticatedUser(
      [this, done = std::move(done)](bool ok, const QString& resolvedUserId,
                                     const QString& message) mutable {
        if (!ok) {
          if (done) done(false, message);
          return;
        }
        fetchListEntriesPage(resolvedUserId, 0, true, 0, std::move(done));
      });
}

void Service::fetchAuthenticatedUser(
    std::function<void(bool, const QString&, const QString&)> done) {
  QUrl url{"https://kitsu.app/api/edge/users"};
  QUrlQuery query;
  query.addQueryItem("filter[self]", "true");
  url.setQuery(query);

  auto* reply = taiga::network()->get(apiRequest(url));
  connect(reply, &QNetworkReply::finished, this, [reply, done = std::move(done)]() mutable {
    if (reply->error() != QNetworkReply::NoError) {
      const auto message = reply->errorString();
      reply->deleteLater();
      done(false, {}, message);
      return;
    }

    const auto document = QJsonDocument::fromJson(reply->readAll());
    reply->deleteLater();
    const auto users = document.object().value("data").toArray();
    const auto user = users.isEmpty() ? QJsonObject{} : users.first().toObject();
    const auto userId = user.value("id").toString();
    if (userId.isEmpty()) {
      done(false, {}, "Could not read the Kitsu user id.");
      return;
    }

    const auto attributes = user.value("attributes").toObject();
    const auto slug = attributes.value("slug").toString();
    const auto name = attributes.value("name").toString();
    taiga::accounts.setKitsuUserId(userId.toStdString());
    if (!slug.isEmpty()) taiga::accounts.setKitsuUsername(slug.toStdString());
    if (!name.isEmpty()) taiga::settings.setStringValue("account.kitsu.displayName", name);
    done(true, userId, {});
  });
}

void Service::fetchListEntriesPage(const QString& userId, int offset, bool clearFirst,
                                   int updatedCount,
                                   std::function<void(bool, const QString&)> done) {
  auto* reply = taiga::network()->get(apiRequest(listUrl(userId, offset)));
  connect(reply, &QNetworkReply::finished, this,
          [this, reply, userId, clearFirst, updatedCount, done = std::move(done)]() mutable {
            if (reply->error() != QNetworkReply::NoError) {
              const auto message = reply->errorString();
              reply->deleteLater();
              if (done) done(false, message);
              return;
            }

            const auto document = QJsonDocument::fromJson(reply->readAll());
            reply->deleteLater();
            if (!document.isObject()) {
              if (done) done(false, "Could not parse Kitsu anime list.");
              return;
            }

            if (clearFirst) anime::db.clearEntries();

            const auto root = document.object();
            QMap<int, Anime> mediaById;
            for (const auto& value : root.value("included").toArray()) {
              const auto object = value.toObject();
              if (object.value("type").toString() != "anime") continue;
              if (const auto media = parseMedia(object)) {
                anime::db.updateItem(*media);
                mediaById.insert(media->id, *media);
              }
            }

            auto count = updatedCount;
            for (const auto& value : root.value("data").toArray()) {
              const auto object = value.toObject();
              const auto animeId = animeIdForEntry(object);
              if (const auto entry = parseListEntry(object, animeId)) {
                anime::db.updateEntry(*entry);
                ++count;
              }
            }

            const auto next = nextOffset(root);
            if (next >= 0) {
              fetchListEntriesPage(userId, next, false, count, std::move(done));
              return;
            }

            if (done) done(true, QString{"Synchronized %1 Kitsu entries."}.arg(count));
          });
}

void Service::fetchSeason(const anime::Season season,
                          std::function<void(bool, const QString&)> done) {
  if (!season) {
    if (done) done(false, "No valid season was selected.");
    return;
  }

  fetchSeasonPage(season, 0, std::make_shared<QList<int>>(), std::move(done));
}

void Service::fetchSeasonPage(const anime::Season season, int offset,
                              const std::shared_ptr<QList<int>>& ids,
                              std::function<void(bool, const QString&)> done) {
  auto* reply = taiga::network()->get(apiRequest(seasonUrl(season, offset)));
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
              if (done) done(false, "Could not parse Kitsu season data.");
              return;
            }

            const auto root = document.object();
            for (const auto& value : root.value("data").toArray()) {
              const auto media = parseMedia(value.toObject());
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
            if (done) done(true, QString{"Fetched %1 Kitsu season entries."}.arg(ids->size()));
          });
}

void Service::updateListEntry(const anime::list::Entry& entry,
                              std::function<void(bool, const QString&)> done) {
  if (taiga::accounts.kitsuAccessToken().empty()) {
    if (done) done(false, "Kitsu access token is not configured.");
    return;
  }
  if (entry.anime_id == anime::list::kUnknownId) {
    if (done) done(false, "Could not update an unknown Kitsu entry.");
    return;
  }

  auto userId = QString::fromStdString(taiga::accounts.kitsuUserId()).trimmed();
  if (userId.isEmpty() && entry.id == anime::list::kUnknownId) {
    fetchAuthenticatedUser([this, entry, done = std::move(done)](
                               bool ok, const QString& resolvedUserId,
                               const QString& message) mutable {
      if (!ok) {
        if (done) done(false, message);
        return;
      }
      updateListEntryWithUser(entry, resolvedUserId, std::move(done));
    });
    return;
  }

  updateListEntryWithUser(entry, userId, std::move(done));
}

void Service::updateListEntryWithUser(const anime::list::Entry& entry, const QString& userId,
                                      std::function<void(bool, const QString&)> done) {
  auto request = apiRequest(updateListEntryUrl(entry));
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/vnd.api+json");
  const auto verb = entry.id == anime::list::kUnknownId ? "POST" : "PATCH";
  auto* reply =
      taiga::network()->sendCustomRequest(request, verb, updateListEntryBody(entry, userId));
  connect(reply, &QNetworkReply::finished, this, [reply, entry, done = std::move(done)]() mutable {
    if (reply->error() != QNetworkReply::NoError) {
      const auto message = reply->errorString();
      reply->deleteLater();
      if (done) done(false, message);
      return;
    }

    const auto document = QJsonDocument::fromJson(reply->readAll());
    reply->deleteLater();
    if (!document.isObject()) {
      if (done) done(false, "Could not parse saved Kitsu entry.");
      return;
    }

    const auto root = document.object();
    if (const auto savedEntry = parseListEntry(root.value("data").toObject(), entry.anime_id)) {
      anime::db.updateEntry(*savedEntry);
      if (done) done(true, "Saved Kitsu entry.");
      return;
    }

    anime::db.updateEntry(entry);
    if (done) done(true, "Saved Kitsu entry.");
  });
}

}  // namespace taiga_sync::kitsu
