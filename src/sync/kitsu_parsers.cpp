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

#include "kitsu_parsers.hpp"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>

#include "base/chrono.hpp"
#include "media/anime.hpp"
#include "media/anime_list.hpp"
#include "sync/kitsu_ratings.hpp"
#include "sync/service.hpp"

namespace taiga_sync::kitsu {

namespace {

std::string titleValue(const QJsonObject& titles, const QString& key) {
  return titles.value(key).toString().toStdString();
}

QString posterUrl(const QJsonObject& attributes) {
  const auto poster = attributes.value("posterImage").toObject();
  for (const auto& key : {"original", "large", "medium", "small"}) {
    const auto value = poster.value(key).toString();
    if (!value.isEmpty()) return value;
  }
  return {};
}

}  // namespace

std::optional<Anime> parseMedia(const QJsonObject& object) {
  const auto id = object.value("id").toString().toInt();
  if (id == anime::kUnknownId) return {};

  const auto attributes = object.value("attributes").toObject();
  const auto titles = attributes.value("titles").toObject();

  Anime anime;
  anime.id = id;
  anime.titles.romaji = attributes.value("canonicalTitle").toString().toStdString();
  anime.titles.english = titleValue(titles, "en");
  anime.titles.japanese = titleValue(titles, "ja_jp");
  if (anime.titles.romaji.empty()) anime.titles.romaji = titleValue(titles, "en_jp");
  anime.type = parseType(attributes.value("subtype").toString());
  anime.status = parseStatus(attributes.value("status").toString());
  anime.synopsis = attributes.value("synopsis").toString().toStdString();
  anime.date_started = FuzzyDate{attributes.value("startDate").toString().toStdString()};
  anime.date_finished = FuzzyDate{attributes.value("endDate").toString().toStdString()};
  const auto episodeCount = attributes.value("episodeCount");
  anime.episode_count =
      episodeCount.isDouble() ? episodeCount.toInt() : anime::kUnknownEpisodeCount;
  anime.episode_length = attributes.value("episodeLength").toInt(anime::kUnknownEpisodeLength);
  anime.score = static_cast<float>(parseScore(attributes.value("averageRating").toString()));
  anime.popularity_rank = attributes.value("popularityRank").toInt();
  anime.age_rating = parseAgeRating(attributes.value("ageRating").toString());
  anime.image_url = posterUrl(attributes).toStdString();
  return anime;
}

anime::AgeRating parseAgeRating(const QString& value) {
  static const QMap<QString, anime::AgeRating> table{
      {"G", anime::AgeRating::G},
      {"PG", anime::AgeRating::PG},
      {"R", anime::AgeRating::R17},
      {"R18", anime::AgeRating::R18},
  };
  return table.value(value.toUpper(), anime::AgeRating::Unknown);
}

double parseScore(const QString& value) {
  return value.toDouble() / 10.0;
}

double fromScore(const double value) {
  return value * 10.0;
}

anime::Status parseStatus(const QString& value) {
  // clang-format off
  static const QMap<QString, anime::Status> table{
      {"current", anime::Status::Airing},
      {"finished", anime::Status::FinishedAiring},
      {"tba", anime::Status::NotYetAired},
      {"unreleased", anime::Status::NotYetAired},
      {"upcoming", anime::Status::NotYetAired},
  };
  // clang-format on
  return table.value(value, anime::Status::Unknown);
}

anime::Type parseType(const QString& value) {
  // clang-format off
  static const QMap<QString, anime::Type> table{
      {"TV", anime::Type::Tv},
      {"special", anime::Type::Special},
      {"OVA", anime::Type::Ova},
      {"ONA", anime::Type::Ona},
      {"movie", anime::Type::Movie},
      {"music", anime::Type::Music},
  };
  // clang-format on
  return table.value(value, anime::Type::Unknown);
}

QString parseListDate(const QString& value) {
  return value.size() >= 10 ? value.first(10) : QString{};
}

QString fromListDate(const QString& value) {
  return value + "T00:00:00.000Z";
}

std::time_t parseListLastUpdated(const QString& value) {
  return QDateTime::fromString(value, Qt::DateFormat::ISODate).toSecsSinceEpoch();
}

anime::list::Status parseListStatus(const QString& value) {
  // clang-format off
  static const QMap<QString, anime::list::Status> table{
      {"current", anime::list::Status::Watching},
      {"planned", anime::list::Status::PlanToWatch},
      {"completed", anime::list::Status::Completed},
      {"on_hold", anime::list::Status::OnHold},
      {"dropped", anime::list::Status::Dropped},
  };
  // clang-format on
  return table.value(value, anime::list::Status::NotInList);
}

QString fromListStatus(const anime::list::Status value) {
  // clang-format off
  switch (value) {
    case anime::list::Status::Watching: return "current";
    case anime::list::Status::Completed: return "completed";
    case anime::list::Status::OnHold: return "on_hold";
    case anime::list::Status::Dropped: return "dropped";
    case anime::list::Status::PlanToWatch: return "planned";
  }
  // clang-format on
  return "";
}

std::optional<ListEntry> parseListEntry(const QJsonObject& object, int animeId) {
  if (animeId == anime::kUnknownId) return {};

  const auto attributes = object.value("attributes").toObject();
  ListEntry entry;
  entry.id = object.value("id").toString().toLongLong();
  entry.anime_id = animeId;
  entry.status = parseListStatus(attributes.value("status").toString());
  entry.watched_episodes = attributes.value("progress").toInt();
  entry.score = parseListScore(attributes.value("ratingTwenty").toInt());
  entry.is_private = attributes.value("private").toBool();
  entry.rewatched_times = attributes.value("reconsumeCount").toInt();
  entry.rewatching = attributes.value("reconsuming").toBool();
  entry.date_started = FuzzyDate{parseListDate(attributes.value("startedAt").toString()).toStdString()};
  entry.date_completed = FuzzyDate{parseListDate(attributes.value("finishedAt").toString()).toStdString()};
  entry.last_updated = parseListLastUpdated(attributes.value("updatedAt").toString());
  entry.notes = attributes.value("notes").toString().toStdString();
  return entry;
}

}  // namespace taiga_sync::kitsu
