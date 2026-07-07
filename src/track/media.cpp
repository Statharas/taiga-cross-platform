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

#include "media.hpp"

#include <QFileInfo>
#include <QRegularExpression>

#include "base/file.hpp"
#include "media/anime_db.hpp"
#include "taiga/settings.hpp"
#include "track/episode.hpp"
#include "track/recognition.hpp"

namespace {

QString slug(QString value) {
  value = value.toLower();
  value.replace(QRegularExpression("[^a-z0-9]+"), ".");
  value.replace(QRegularExpression("^\\.|\\.$"), "");
  return value;
}

std::optional<track::Episode> episodeFromMediaInfo(const anisthesia::MediaInfo& mediaInfo) {
  auto episode = [&mediaInfo]() {
    if (mediaInfo.type == anisthesia::MediaInfoType::File) {
      const QFileInfo fileInfo{QString::fromStdString(mediaInfo.value)};
      return track::recognition::parseFileInfo(fileInfo);
    }

    return track::recognition::parse(mediaInfo.value);
  }();

  const auto animeId = track::recognition::identify(episode);
  if (!animeId) return std::nullopt;

  episode.setAnimeId(animeId);
  return episode;
}

}  // namespace

namespace track::media {

Detection::Detection(QObject* parent) : QObject(parent) {
  pollTimer_ = new QTimer(this);
  connect(pollTimer_, &QTimer::timeout, this, &Detection::poll);
}

const std::optional<Episode> Detection::getCurrentEpisode() const {
  return currentEpisode_;
}

const std::optional<Detection::media_t> Detection::getCurrentMedia() const {
  return currentMedia_;
}

const std::optional<Detection::player_t> Detection::getCurrentPlayer() const {
  return currentPlayer_;
}

bool Detection::init() {
  const auto file = base::readFile(":/players.anisthesia");

  if (file.isEmpty()) {
    return false;
  }

  if (!anisthesia::ParsePlayersData(file.toStdString(), players_)) {
    return false;
  }

  const auto interval = taiga::settings.mediaDetectionInterval();
  pollTimer_->start(interval);

  return true;
}

void Detection::poll() {
  if (players_.empty()) return;

  std::vector<player_t> players;
  for (const auto& player : players_) {
    const auto name = QString::fromStdString(player.name);
    if (player.type == anisthesia::PlayerType::WebBrowser) {
      if (!taiga::settings.boolValue("recognition.streaming.enabled", false)) continue;
      if (!taiga::settings.boolValue(QString("recognition.streaming.providers.%1").arg(slug(name)), true)) {
        continue;
      }
    } else {
      if (!taiga::settings.boolValue("recognition.mediaPlayers.enabled", true)) continue;
      if (!taiga::settings.boolValue(QString("recognition.mediaPlayers.%1").arg(slug(name)), true)) {
        continue;
      }
    }
    players.emplace_back(player);
  }
  if (players.empty()) return;

  static const auto media_proc = [](const anisthesia::MediaInfo&) {
    return true;  // Accept all media
  };

  std::vector<anisthesia::Result> results;
  if (!anisthesia::GetResults(players, media_proc, results)) {
    currentPlayer_.reset();
    currentMedia_.reset();
    if (currentEpisode_) {
      currentEpisode_.reset();
      emit currentEpisodeChanged(std::nullopt);
    }
    return;
  }

  currentPlayer_ = results.front().player;
  currentMedia_ = results.front().media.front();

  const auto episode = episodeFromMediaInfo(currentMedia_->information.front());
  if (!episode) return;

  if (!currentEpisode_ || currentEpisode_->animeId() != episode->animeId()) {
    currentEpisode_ = *episode;
    emit currentEpisodeChanged(episode);
  }
}

}  // namespace track::media
