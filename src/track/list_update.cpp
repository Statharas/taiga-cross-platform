/**
 * Taiga
 * Copyright (C) 2010-2025, Eren Okka
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "list_update.hpp"

#include <QDate>
#include <QDateTime>

#include "media/anime.hpp"
#include "media/anime_db.hpp"
#include "sync/service.hpp"
#include "taiga/settings.hpp"
#include "track/media.hpp"

namespace {

FuzzyDate fuzzyDateFromQDate(const QDate& date) {
  return FuzzyDate{
      std::chrono::year{date.year()} / std::chrono::month{static_cast<unsigned>(date.month())} /
      std::chrono::day{static_cast<unsigned>(date.day())}};
}

std::optional<int> episodeNumber(const track::Episode& episode) {
  bool ok = false;
  const auto value = QString::fromStdString(episode.element(anitomy::ElementKind::Episode)).toInt(&ok);
  if (!ok || value <= 0) return std::nullopt;
  return value;
}

}  // namespace

namespace track::list_update {

std::optional<PreparedUpdate> prepareUpdate(const Episode& episode) {
  const auto anime = anime::db.item(episode.animeId());
  if (!anime) return PreparedUpdate{{}, QObject::tr("Detected anime is not in the local database.")};

  const auto number = episodeNumber(episode);
  if (!number) return PreparedUpdate{{}, QObject::tr("Detected episode number is invalid.")};

  if (anime->episode_count > 0 && *number > anime->episode_count &&
      !taiga::settings.boolValue("account.update.outOfRange", false)) {
    return PreparedUpdate{{}, QObject::tr("Detected episode is outside the anime episode range.")};
  }

  ListEntry entry = anime::db.entry(anime->id) ? *anime::db.entry(anime->id) : ListEntry{};
  entry.anime_id = anime->id;

  if (entry.status == anime::list::Status::Completed && !entry.rewatching) {
    return PreparedUpdate{{}, QObject::tr("List entry is already completed.")};
  }

  if (*number <= entry.watched_episodes) {
    return PreparedUpdate{{}, QObject::tr("List entry is already up to date.")};
  }

  entry.watched_episodes = *number;
  entry.last_updated = QDateTime::currentSecsSinceEpoch();

  const auto today = fuzzyDateFromQDate(QDate::currentDate());
  if (*number == 1 && !entry.date_started) entry.date_started = today;
  if (anime->episode_count > 0 && *number == anime->episode_count && !entry.date_completed) {
    entry.date_completed = today;
  }

  if (anime->episode_count > 0 && *number == anime->episode_count) {
    entry.status = anime::list::Status::Completed;
  } else if (!entry.rewatching) {
    entry.status = anime::list::Status::Watching;
  }

  return PreparedUpdate{entry, QObject::tr("Updating list entry.")};
}

Manager::Manager(QObject* parent) : QObject(parent) {
  timer_ = new QTimer(this);
  timer_->setInterval(1000);
  connect(timer_, &QTimer::timeout, this, &Manager::tick);
}

void Manager::init() {
  connect(track::media::detection(), &track::media::Detection::currentEpisodeChanged, this,
          &Manager::onCurrentEpisodeChanged);
  if (const auto episode = track::media::detection()->getCurrentEpisode()) {
    onCurrentEpisodeChanged(episode);
  }
}

int Manager::remainingSeconds() const {
  return remainingSeconds_;
}

QString Manager::statusText() const {
  return statusText_;
}

void Manager::onCurrentEpisodeChanged(std::optional<Episode> episode) {
  timer_->stop();
  pendingEpisode_ = std::move(episode);
  updating_ = false;

  if (!pendingEpisode_) {
    remainingSeconds_ = 0;
    setStatus({});
    emit countdownChanged(remainingSeconds_);
    return;
  }

  if (!taiga::settings.boolValue("program.general.enableSync", true)) {
    remainingSeconds_ = 0;
    setStatus(tr("List update disabled."));
    emit countdownChanged(remainingSeconds_);
    return;
  }

  remainingSeconds_ = std::max(0, taiga::settings.intValue("account.update.delay", 120));
  emit countdownChanged(remainingSeconds_);
  if (remainingSeconds_ == 0) {
    updateNow();
  } else {
    setStatus({});
    timer_->start();
  }
}

void Manager::tick() {
  if (remainingSeconds_ > 0) --remainingSeconds_;
  emit countdownChanged(remainingSeconds_);
  if (remainingSeconds_ <= 0) updateNow();
}

void Manager::updateNow() {
  if (updating_ || !pendingEpisode_) return;
  timer_->stop();
  updating_ = true;

  const auto prepared = prepareUpdate(*pendingEpisode_);
  if (!prepared || prepared->entry.anime_id == anime::list::kUnknownId) {
    updating_ = false;
    setStatus(prepared ? prepared->message : tr("List update skipped."));
    emit updateFinished(false, statusText_);
    return;
  }

  setStatus(tr("Updating list..."));
  taiga_sync::updateListEntry(prepared->entry, [this](bool ok, const QString& message) {
    updating_ = false;
    setStatus(message);
    emit updateFinished(ok, message);
  });
}

void Manager::setStatus(QString status) {
  if (statusText_ == status) return;
  statusText_ = std::move(status);
  emit statusChanged(statusText_);
}

}  // namespace track::list_update
