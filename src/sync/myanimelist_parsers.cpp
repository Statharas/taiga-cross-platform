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

#include "myanimelist_parsers.hpp"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QMap>

#include "base/chrono.hpp"
#include "base/string.hpp"
#include "media/anime.hpp"
#include "media/anime_list.hpp"

namespace taiga_sync::myanimelist {

namespace {

std::vector<std::string> stringList(const QJsonArray& values, const QString& key = {}) {
  std::vector<std::string> list;
  list.reserve(values.size());
  for (const auto& value : values) {
    const auto text = key.isEmpty() ? value.toString() : value.toObject().value(key).toString();
    if (!text.isEmpty()) list.push_back(text.toStdString());
  }
  return list;
}

QString imageUrl(const QJsonObject& value) {
  const auto picture = value.value("main_picture").toObject();
  const auto large = picture.value("large").toString();
  return large.isEmpty() ? picture.value("medium").toString() : large;
}

std::string commentsAndTags(const QJsonObject& value) {
  QStringList notes;
  const auto comments = value.value("comments").toString().trimmed();
  if (!comments.isEmpty()) notes.push_back(comments);
  const auto tags = value.value("tags").toArray();
  QStringList tagValues;
  for (const auto& tag : tags) {
    const auto text = tag.toString().trimmed();
    if (!text.isEmpty()) tagValues.push_back(text);
  }
  if (!tagValues.isEmpty()) notes.push_back("Tags: " + tagValues.join(", "));
  return notes.join("\n").toStdString();
}

}  // namespace

std::optional<Anime> parseMedia(const QJsonValue& value) {
  const auto object = value.toObject();
  const auto id = object.value("id").toInt();
  if (id == anime::kUnknownId) return {};

  Anime anime;
  anime.id = id;
  anime.titles.romaji = object.value("title").toString().toStdString();
  const auto alternativeTitles = object.value("alternative_titles").toObject();
  anime.titles.english = alternativeTitles.value("en").toString().toStdString();
  anime.titles.japanese = alternativeTitles.value("ja").toString().toStdString();
  anime.titles.synonyms = stringList(alternativeTitles.value("synonyms").toArray());
  anime.date_started = parseFuzzyDate(object.value("start_date").toString());
  anime.date_finished = parseFuzzyDate(object.value("end_date").toString());
  anime.synopsis = object.value("synopsis").toString().toStdString();
  anime.score = static_cast<float>(object.value("mean").toDouble());
  anime.popularity_rank = object.value("popularity").toInt();
  const auto episodeCount = object.value("num_episodes");
  anime.episode_count =
      episodeCount.isDouble() ? episodeCount.toInt() : anime::kUnknownEpisodeCount;
  anime.status = parseStatus(object.value("status").toString());
  anime.type = parseType(object.value("media_type").toString());
  anime.episode_length = parseEpisodeLength(object.value("average_episode_duration").toInt());
  anime.age_rating = parseAgeRating(object.value("rating").toString());
  anime.image_url = imageUrl(object).toStdString();
  anime.genres = stringList(object.value("genres").toArray(), "name");
  anime.studios = stringList(object.value("studios").toArray(), "name");
  anime.last_modified = parseListLastUpdated(object.value("updated_at").toString());
  return anime;
}

anime::AgeRating parseAgeRating(const QString& value) {
  using anime::AgeRating;
  // clang-format off
  static const QMap<QString, AgeRating> table{
      {"g", AgeRating::G},
      {"pg", AgeRating::PG},
      {"pg_13", AgeRating::PG13},
      {"r", AgeRating::R17},
      {"r+", AgeRating::R17},
      {"rx", AgeRating::R18},
  };
  // clang-format on
  return table.value(value.toLower(), AgeRating::Unknown);
}

FuzzyDate parseFuzzyDate(const QString& value) {
  // YYYY-MM-DD
  if (value.size() >= 10) return FuzzyDate(value.toStdString());
  // YYYY-MM
  if (value.size() == 7) return FuzzyDate(u"%1-00"_s.arg(value).toStdString());
  // YYYY
  if (value.size() == 4) return FuzzyDate(u"%1-00-00"_s.arg(value).toStdString());
  return FuzzyDate{};
}

int parseEpisodeLength(int value) {
  const auto seconds = std::chrono::seconds{value};
  return std::chrono::duration_cast<std::chrono::minutes>(seconds).count();
}

anime::Status parseStatus(const QString& value) {
  using anime::Status;
  static const QMap<QString, Status> table{
      {"currently_airing", Status::Airing},
      {"finished_airing", Status::FinishedAiring},
      {"not_yet_aired", Status::NotYetAired},
  };
  return table.value(value.toLower(), Status::Unknown);
}

anime::Type parseType(const QString& value) {
  using anime::Type;
  // clang-format off
  static const QMap<QString, Type> table{
    {"unknown", Type::Unknown},
    {"tv", Type::Tv},
    {"ova", Type::Ova},
    {"movie", Type::Movie},
    {"special", Type::Special},
    {"ona", Type::Ona},
    {"music", Type::Music},
    {"cm", Type::Special},
    {"pv", Type::Special},
    {"tv_special", Type::Special},
  };
  // clang-format on
  return table.value(value.toLower(), Type::Unknown);
}

std::time_t parseListLastUpdated(const QString& value) {
  return QDateTime::fromString(value, Qt::DateFormat::ISODate).toSecsSinceEpoch();
}

int parseListScore(int value) {
  return value * (anime::list::kScoreMax / 10);
}

anime::list::Status parseListStatus(const QString& value) {
  using anime::list::Status;
  static const QMap<QString, Status> table{
      {"watching", Status::Watching},
      {"completed", Status::Completed},
      {"on_hold", Status::OnHold},
      {"dropped", Status::Dropped},
      {"plan_to_watch", Status::PlanToWatch},
  };
  return table.value(value.toLower(), Status::NotInList);
}

std::optional<ListEntry> parseListEntry(const QJsonValue& value, int animeId) {
  const auto object = value.toObject();
  if (animeId == anime::kUnknownId) return {};

  ListEntry entry;
  entry.id = animeId;
  entry.anime_id = animeId;
  entry.status = parseListStatus(object.value("status").toString());
  entry.score = parseListScore(object.value("score").toInt());
  entry.watched_episodes = object.value("num_episodes_watched").toInt();
  entry.rewatching = object.value("is_rewatching").toBool();
  entry.rewatched_times = object.value("num_times_rewatched").toInt();
  entry.date_started = parseFuzzyDate(object.value("start_date").toString());
  entry.date_completed = parseFuzzyDate(object.value("finish_date").toString());
  entry.last_updated = parseListLastUpdated(object.value("updated_at").toString());
  entry.notes = commentsAndTags(object);
  return entry;
}

}  // namespace taiga_sync::myanimelist
