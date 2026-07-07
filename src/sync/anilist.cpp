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

#include "anilist.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRestReply>
#include <memory>
#include <ranges>

#include "base/file.hpp"
#include "base/log.hpp"
#include "base/string.hpp"
#include "media/anime_db.hpp"
#include "media/anime_season.hpp"
#include "media/anime_season_db.hpp"
#include "sync/anilist_parsers.hpp"
#include "sync/anilist_utils.hpp"
#include "taiga/accounts.hpp"

// AniList API documentation:
// https://docs.anilist.co/

namespace taiga_sync::anilist {

Service::Service() : taiga_sync::Service{} {
  api_.setBaseUrl(QUrl{"https://graphql.anilist.co"});
  applyBearerToken();
}

Service* Service::instance() {
  static Service service;
  return &service;
}

////////////////////////////////////////////////////////////////////////////////

void Service::authenticateUser(std::function<void(bool, const QString&)> done) {
  if (!applyBearerToken()) {
    if (done) done(false, "AniList token is not configured.");
    return;
  }

  const QJsonDocument data{QJsonObject{
      {"query", gql("Viewer")},
  }};

  const auto callback = [this, done = std::move(done)](QRestReply& reply) {
    if (isError(reply)) {
      handleError(reply);
      if (done) done(false, reply.errorString());
      return;
    }

    const auto viewer = reply.readJson().and_then([](const QJsonDocument& json) {
      return std::make_optional(json["data"]["Viewer"].toObject());
    });

    if (!viewer) {
      handleError(reply, "Could not parse user object.");
      if (done) done(false, "Could not parse AniList user object.");
      return;
    }

    taiga::accounts.setAnilistUsername((*viewer)["name"].toString().toStdString());
    // @TODO: Set rating system setting using viewer["mediaListOptions"]["scoreFormat"]

    if (done) done(true, {});
  };

  manager_.post(api_.createRequest(), data, this, callback);
}

void Service::fetchAnime(const int id) {
  const QJsonDocument data{{
      {"query", gql("Media")},
      {"variables", QJsonObject{{"id", id}}},
  }};

  const auto callback = [this](QRestReply& reply) {
    if (isError(reply)) {
      handleError(reply);
      return;
    }

    const auto item = reply.readJson().and_then(
        [](const QJsonDocument& json) { return parseMedia(json["data"]["Media"]); });

    if (!item) {
      handleError(reply, "Could not parse media object.");
      return;
    }

    anime::db.updateItem(*item);
  };

  manager_.post(api_.createRequest(), data, this, callback);
}

void Service::fetchSeason(const anime::Season season, std::function<void(bool, const QString&)> done) {
  constexpr int kPageSize = 50;

  const auto ids = std::make_shared<QList<int>>();
  const auto fetchPage = std::make_shared<std::function<void(int)>>();

  *fetchPage = [this, season, done = std::move(done), ids, fetchPage, kPageSize](int page) mutable {
    const QJsonDocument data{QJsonObject{
        {"query", gql("MediaSearch")},
        {"variables",
         QJsonObject{
             {"season", fromSeasonName(season.name)},
             {"seasonYear", static_cast<int>(season.year)},
             {"page", page},
             {"perPage", kPageSize},
         }},
    }};

    const auto callback =
        [this, season, done, ids, fetchPage](QRestReply& reply) mutable {
          if (isError(reply)) {
            handleError(reply);
            if (done) done(false, reply.errorString());
            return;
          }

          const auto json = reply.readJson();
          if (!json) {
            handleError(reply, "Could not parse season results.");
            if (done) done(false, "Could not parse AniList season data.");
            return;
          }

          const auto pageObject = (*json)["data"]["Page"].toObject();
          for (const auto& value : pageObject["media"].toArray()) {
            if (const auto item = parseMedia(value)) {
              anime::db.updateItem(*item);
              ids->push_back(item->id);
            }
          }

          const auto pageInfo = pageObject["pageInfo"].toObject();
          if (pageInfo["hasNextPage"].toBool()) {
            (*fetchPage)(pageInfo["currentPage"].toInt() + 1);
            return;
          }

          anime::season_db.set(season, *ids);
          if (done) done(true, QString{"Fetched %1 AniList season entries."}.arg(ids->size()));
        };

    manager_.post(api_.createRequest(), data, this, callback);
  };

  (*fetchPage)(1);
}

void Service::search(const QString& query, std::function<void(bool, const QString&)> done) {
  const QJsonDocument data{{
      {"query", gql("MediaSearch")},
      {"variables", QJsonObject{{"query", query}}},
  }};

  const auto callback = [this, done = std::move(done)](QRestReply& reply) {
    if (isError(reply)) {
      handleError(reply);
      if (done) done(false, reply.errorString());
      return;
    }

    const auto items = reply.readJson().and_then([](const QJsonDocument& json) {
      const auto value = json["data"]["Page"]["media"];
      if (!value.isArray()) return std::optional<QList<std::optional<Anime>>>{};
      QList<std::optional<Anime>> items;
      const auto array = value.toArray();
      items.reserve(array.size());
      for (const auto& item : array) {
        items.emplace_back(parseMedia(item));
      }
      return std::make_optional(items);
    });

    if (!items) {
      handleError(reply, "Could not parse search results.");
      if (done) done(false, "Could not parse AniList search results.");
      return;
    }

    int updated = 0;
    for (const auto& item : *items) {
      if (item) {
        anime::db.updateItem(*item);
        ++updated;
      }
    }
    if (done) done(true, QString{"Found %1 AniList search result(s)."}.arg(updated));
  };

  manager_.post(api_.createRequest(), data, this, callback);
}

void Service::fetchListEntries(std::function<void(bool, const QString&)> done) {
  applyBearerToken();

  const auto username = QString::fromStdString(taiga::accounts.anilistUsername()).trimmed();
  if (username.isEmpty()) {
    authenticateUser([this, done = std::move(done)](bool ok, const QString& message) mutable {
      if (!ok) {
        if (done) done(false, message);
        return;
      }
      fetchListEntries(std::move(done));
    });
    return;
  }

  const QJsonDocument data{{
      {"query", gql("MediaListCollection")},
      {"variables", QJsonObject{{"userName", username}}},
  }};

  const auto callback = [this, done = std::move(done)](QRestReply& reply) {
    if (isError(reply)) {
      handleError(reply);
      if (done) done(false, reply.errorString());
      return;
    }

    const auto json = reply.readJson();
    if (!json) {
      handleError(reply, "Could not parse anime list.");
      if (done) done(false, "Could not parse AniList anime list.");
      return;
    }

    const auto lists = (*json)["data"]["MediaListCollection"]["lists"].toArray();
    if (lists.isEmpty()) {
      anime::db.clearEntries();
      if (done) done(true, "AniList returned an empty anime list.");
      return;
    }

    int updated = 0;
    anime::db.clearEntries();
    for (const auto& list : lists) {
      for (const auto& value : list.toObject()["entries"].toArray()) {
        const auto entryObject = value.toObject();
        if (const auto media = parseMedia(entryObject["media"])) {
          anime::db.updateItem(*media);
        }
        if (const auto entry = parseMediaListEntry(entryObject)) {
          anime::db.updateEntry(*entry);
          ++updated;
        }
      }
    }

    if (done) done(true, QString{"Synchronized %1 AniList entries."}.arg(updated));
  };

  manager_.post(api_.createRequest(), data, this, callback);
}

void Service::addListEntry() {
  updateListEntry();
}

void Service::deleteListEntry(const int id) {
  const auto listEntry = anime::db.entry(id);

  if (!listEntry) return;

  const QJsonDocument data{{
      {"query", gql("DeleteMediaListEntry")},
      {"variables", QJsonObject{{"id", static_cast<qint64>(listEntry->id)}}},
  }};

  const auto callback = [this, id](QRestReply& reply) {
    if (isError(reply) && reply.httpStatus() != 404) {
      handleError(reply);
      return;
    }

    anime::db.deleteEntry(id);
  };

  manager_.post(api_.createRequest(), data, this, callback);
}

void Service::updateListEntry() {
  // @TODO
}

////////////////////////////////////////////////////////////////////////////////

QString Service::gql(const QString& name) const {
  auto query = base::readFile(u":/gql/anilist/%1.gql"_s.arg(name));
  if (name.endsWith("Fields")) return query;
  query.replace("{mediaFields}", gql("MediaFields").trimmed());
  query.replace("{mediaListFields}", gql("MediaListFields").trimmed());
  return query;
}

bool Service::isError(const QRestReply& reply) const {
  return !reply.isHttpStatusSuccess() || reply.hasError();
  // @TODO: Check DDoS protection
}

bool Service::applyBearerToken() {
  const auto token = taiga::accounts.anilistToken();
  if (token.empty()) return false;
  api_.setBearerToken(QByteArray::fromStdString(token));
  return true;
}

void Service::handleError(const QRestReply& reply, const QString& message) const {
  if (reply.hasError()) LOGE("{}", reply.errorString().toStdString());
  if (!message.isEmpty()) LOGE("{}", message.toStdString());
  // @TODO: Parse body for "errors" array
  // @TODO: Emit signal
}

}  // namespace taiga_sync::anilist
