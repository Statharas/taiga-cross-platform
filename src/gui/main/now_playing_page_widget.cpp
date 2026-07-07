/**
 * Taiga
 * Copyright (C) 2010-2024, Eren Okka
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "now_playing_page_widget.hpp"

#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QPixmap>
#include <QVBoxLayout>

#include "gui/utils/format.hpp"
#include "gui/utils/image_provider.hpp"
#include "media/anime_db.hpp"
#include "media/anime_season.hpp"
#include "track/media.hpp"

namespace gui {

namespace {

QString joinStrings(const std::vector<std::string>& values) {
  QStringList strings;
  for (const auto& value : values) {
    if (!value.empty()) strings.push_back(QString::fromStdString(value));
  }
  return strings.join(", ");
}

QFrame* separator(QWidget* parent) {
  auto* line = new QFrame(parent);
  line->setFrameShape(QFrame::HLine);
  line->setFrameShadow(QFrame::Sunken);
  return line;
}

QLabel* sectionTitle(const QString& text, QWidget* parent) {
  auto* label = new QLabel(text, parent);
  auto font = label->font();
  font.setBold(true);
  label->setFont(font);
  return label;
}

}  // namespace

NowPlayingPageWidget::NowPlayingPageWidget(QWidget* parent) : QWidget(parent) {
  auto* rootLayout = new QHBoxLayout(this);
  rootLayout->setContentsMargins(12, 12, 12, 12);
  rootLayout->setSpacing(14);

  m_posterLabel = new QLabel(this);
  m_posterLabel->setFixedSize(150, 230);
  m_posterLabel->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
  m_posterLabel->setFrameShape(QFrame::StyledPanel);
  m_posterLabel->setScaledContents(false);
  rootLayout->addWidget(m_posterLabel, 0, Qt::AlignTop);

  auto* content = new QWidget(this);
  auto* contentLayout = new QVBoxLayout(content);
  contentLayout->setContentsMargins(0, 0, 0, 0);
  contentLayout->setSpacing(6);
  rootLayout->addWidget(content, 1);

  m_titleLabel = new QLabel(content);
  auto titleFont = m_titleLabel->font();
  titleFont.setPointSize(titleFont.pointSize() + 2);
  m_titleLabel->setFont(titleFont);
  m_titleLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
  m_titleLabel->setOpenExternalLinks(false);
  contentLayout->addWidget(m_titleLabel);

  m_actionLabel = new QLabel(content);
  m_actionLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
  contentLayout->addWidget(m_actionLabel);

  contentLayout->addSpacing(14);
  contentLayout->addWidget(sectionTitle(tr("Alternative titles"), content));
  contentLayout->addWidget(separator(content));
  m_alternativeTitlesLabel = new QLabel(content);
  m_alternativeTitlesLabel->setWordWrap(true);
  contentLayout->addWidget(m_alternativeTitlesLabel);

  contentLayout->addSpacing(4);
  contentLayout->addWidget(sectionTitle(tr("Details"), content));
  contentLayout->addWidget(separator(content));
  m_detailsLabel = new QLabel(content);
  m_detailsLabel->setTextFormat(Qt::RichText);
  contentLayout->addWidget(m_detailsLabel);

  contentLayout->addSpacing(4);
  contentLayout->addWidget(sectionTitle(tr("Synopsis"), content));
  contentLayout->addWidget(separator(content));
  m_synopsisLabel = new QLabel(content);
  m_synopsisLabel->setWordWrap(true);
  contentLayout->addWidget(m_synopsisLabel);

  m_emptyLabel = new QLabel(tr("No media is currently detected."), content);
  m_emptyLabel->setAlignment(Qt::AlignCenter);
  m_emptyLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  contentLayout->addWidget(m_emptyLabel, 1);

  connect(track::media::detection(), &track::media::Detection::currentEpisodeChanged, this,
          [this](std::optional<track::Episode> episode) {
            if (episode) {
              setPlaying(*episode);
            } else {
              reset();
            }
          });
  connect(&imageProvider, &ImageProvider::posterChanged, this, [this](int id) {
    if (m_anime && m_anime->id == id) refreshPoster();
  });

  if (const auto episode = track::media::detection()->getCurrentEpisode()) {
    setPlaying(*episode);
  } else {
    reset();
  }
}

void NowPlayingPageWidget::reset() {
  m_anime.reset();
  m_episode.reset();
  refresh();
}

void NowPlayingPageWidget::setPlaying(const track::Episode& episode) {
  m_episode = episode;
  if (const auto anime = anime::db.item(episode.animeId())) {
    m_anime = *anime;
    imageProvider.fetchPoster(anime->id);
  } else {
    m_anime.reset();
  }
  refresh();
}

void NowPlayingPageWidget::refresh() {
  const bool hasEpisode = m_episode.has_value();
  m_emptyLabel->setVisible(!hasEpisode);
  m_posterLabel->setVisible(hasEpisode);
  m_titleLabel->setVisible(hasEpisode);
  m_actionLabel->setVisible(hasEpisode);
  m_alternativeTitlesLabel->setVisible(hasEpisode);
  m_detailsLabel->setVisible(hasEpisode);
  m_synopsisLabel->setVisible(hasEpisode);

  if (!hasEpisode) {
    m_titleLabel->clear();
    m_actionLabel->clear();
    m_alternativeTitlesLabel->clear();
    m_detailsLabel->clear();
    m_synopsisLabel->clear();
    m_posterLabel->clear();
    return;
  }

  const QString title = m_anime ? QString::fromStdString(m_anime->titles.romaji)
                                : QString::fromStdString(
                                      m_episode->element(anitomy::ElementKind::Title, "Unknown"));
  const QString episode = QString::fromStdString(m_episode->element(anitomy::ElementKind::Episode, "?"));

  m_titleLabel->setText(QString("<a href=\"#\" style=\"color: #0645ad; text-decoration: none;\">%1</a>")
                            .arg(title.toHtmlEscaped()));
  m_actionLabel->setText(
      tr("Now playing: Episode %1<br><a href=\"#\" style=\"color: #0645ad;\">Edit</a> · "
         "<a href=\"#\" style=\"color: #0645ad;\">Share</a> · "
         "<a href=\"#\" style=\"color: #0645ad;\">Watch next episode</a>")
          .arg(episode));

  if (!m_anime) {
    m_alternativeTitlesLabel->setText("-");
    m_detailsLabel->setText("-");
    m_synopsisLabel->setText(tr("No anime database entry is linked to the detected media."));
    refreshPoster();
    return;
  }

  QStringList titles;
  if (!m_anime->titles.english.empty()) titles.push_back(QString::fromStdString(m_anime->titles.english));
  if (!m_anime->titles.japanese.empty()) titles.push_back(QString::fromStdString(m_anime->titles.japanese));
  for (const auto& synonym : m_anime->titles.synonyms) {
    if (!synonym.empty()) titles.push_back(QString::fromStdString(synonym));
  }
  m_alternativeTitlesLabel->setText(titles.isEmpty() ? "-" : titles.join("<br>"));

  const auto season = anime::Season{m_anime->date_started};
  const QString aired = formatFuzzyDateRange(m_anime->date_started, m_anime->date_finished);
  const QString details =
      QString("<table>"
      "<tr><td><b>Type:</b></td><td>%1</td></tr>"
      "<tr><td><b>Episodes:</b></td><td>%2</td></tr>"
      "<tr><td><b>Status:</b></td><td>%3</td></tr>"
      "<tr><td><b>Season:</b></td><td>%4</td></tr>"
      "<tr><td><b>Aired:</b></td><td>%5</td></tr>"
      "<tr><td><b>Genres:</b></td><td>%6</td></tr>"
      "<tr><td><b>Producers:</b></td><td>%7</td></tr>"
      "<tr><td><b>Score:</b></td><td>%8</td></tr>"
      "</table>").arg(formatType(m_anime->type))
          .arg(formatNumber(m_anime->episode_count, "Unknown"))
          .arg(formatStatus(m_anime->status))
          .arg(season ? formatSeason(season) : "-")
          .arg(aired)
          .arg(joinStrings(m_anime->genres).toHtmlEscaped())
          .arg(joinStrings(m_anime->producers).toHtmlEscaped())
          .arg(formatScore(m_anime->score));
  m_detailsLabel->setText(details);
  m_synopsisLabel->setText(QString::fromStdString(m_anime->synopsis).toHtmlEscaped());

  refreshPoster();
}

void NowPlayingPageWidget::refreshPoster() {
  if (!m_anime) {
    m_posterLabel->clear();
    return;
  }

  const auto* pixmap = imageProvider.loadPoster(m_anime->id);
  if (!pixmap || pixmap->isNull()) {
    imageProvider.fetchPoster(m_anime->id);
    m_posterLabel->clear();
    return;
  }

  m_posterLabel->setPixmap(
      pixmap->scaled(m_posterLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

}  // namespace gui
