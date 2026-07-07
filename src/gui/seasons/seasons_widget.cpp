/**
 * Taiga
 * Copyright (C) 2010-2024, Eren Okka
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "seasons_widget.hpp"

#include <QDate>
#include <QFrame>
#include <QGridLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QTableWidget>
#include <QToolButton>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>

#include "gui/media/media_menu.hpp"
#include "gui/utils/format.hpp"
#include "gui/utils/image_provider.hpp"
#include "gui/utils/theme.hpp"
#include "media/anime_db.hpp"
#include "media/anime_season_db.hpp"
#include "sync/service.hpp"

namespace gui {

namespace {

QString joinStrings(const std::vector<std::string>& values) {
  QStringList strings;
  for (const auto& value : values) {
    if (!value.empty()) strings.push_back(QString::fromStdString(value));
  }
  return strings.join(", ");
}

QWidget* labelWithIcon(const QIcon& icon, const QString& text, QWidget* parent) {
  auto* widget = new QWidget(parent);
  auto* layout = new QHBoxLayout(widget);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(3);

  auto* iconLabel = new QLabel(widget);
  iconLabel->setPixmap(icon.pixmap(QSize{16, 16}));
  layout->addWidget(iconLabel);

  auto* textLabel = new QLabel(text, widget);
  layout->addWidget(textLabel);

  return widget;
}

}  // namespace

SeasonsWidget::SeasonsWidget(QWidget* parent) : QWidget(parent) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(8, 4, 8, 4);
  layout->setSpacing(4);

  auto* controls = new QHBoxLayout();
  controls->setSpacing(6);
  layout->addLayout(controls);

  auto* seasonLabel = new QLabel(this);
  seasonLabel->setPixmap(
      theme.getIcon("classic/16px/calendar-month", "png", false).pixmap(QSize{16, 16}));
  controls->addWidget(seasonLabel);

  m_seasonCombo = new QComboBox(this);
  m_seasonCombo->setMinimumWidth(150);
  controls->addWidget(m_seasonCombo);

  m_yearCombo = new QComboBox(this);
  m_yearCombo->setMinimumWidth(82);
  controls->addWidget(m_yearCombo);

  auto* refreshButton = new QToolButton(this);
  refreshButton->setIcon(theme.getIcon("sync"));
  refreshButton->setText(tr("Refresh data"));
  refreshButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  controls->addWidget(refreshButton);

  controls->addWidget(
      labelWithIcon(theme.getIcon("classic/16px/category", "png", false), tr("Group by:"), this));
  m_groupCombo = new QComboBox(this);
  controls->addWidget(m_groupCombo);

  controls->addWidget(labelWithIcon(theme.getIcon("classic/16px/sort-quantity-descending", "png",
                                                  false),
                                    tr("Sort by:"), this));
  m_sortCombo = new QComboBox(this);
  controls->addWidget(m_sortCombo);

  controls->addWidget(labelWithIcon(theme.getIcon("classic/16px/ui-scroll-pane-detail", "png",
                                                  false),
                                    tr("View:"), this));
  m_viewCombo = new QComboBox(this);
  controls->addWidget(m_viewCombo);
  controls->addStretch();

  m_scrollArea = new QScrollArea(this);
  m_scrollArea->setWidgetResizable(true);
  m_scrollArea->setFrameShape(QFrame::NoFrame);
  m_detailsWidget = new QWidget(m_scrollArea);
  m_scrollArea->setWidget(m_detailsWidget);
  layout->addWidget(m_scrollArea, 1);

  m_table = new QTableWidget(this);
  m_table->setColumnCount(6);
  m_table->setHorizontalHeaderLabels(
      {tr("Title"), tr("Type"), tr("Season"), tr("Episodes"), tr("Score"), tr("Status")});
  m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_table->setContextMenuPolicy(Qt::CustomContextMenu);
  m_table->setSortingEnabled(true);
  m_table->setAlternatingRowColors(true);
  m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
  for (int column = 1; column < m_table->columnCount(); ++column) {
    m_table->horizontalHeader()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
  }
  layout->addWidget(m_table, 1);

  m_statusLabel = new QLabel(this);
  layout->addWidget(m_statusLabel);

  initControls();
  updateView();

  connect(&imageProvider, &ImageProvider::posterChanged, this, [this](int) {
    if (!m_refreshingPosters) scheduleUpdate();
  });
  connect(m_table, &QWidget::customContextMenuRequested, this,
          &SeasonsWidget::showTableContextMenu);
  connect(refreshButton, &QToolButton::clicked, this, [this, refreshButton]() {
    const auto selectedYear = m_yearCombo->currentData().toInt();
    const auto selectedSeason =
        static_cast<anime::SeasonName>(m_seasonCombo->currentData().toInt());
    const auto season = anime::Season{selectedSeason, std::chrono::year{selectedYear}};
    if (!season) return;

    refreshButton->setEnabled(false);
    m_statusLabel->setText(
        tr("%1: Retrieving %2 anime season...")
            .arg(taiga_sync::serviceName(taiga_sync::currentServiceId()))
            .arg(formatSeason(season)));
    taiga_sync::fetchSeason(season, [this, refreshButton, season](bool ok, const QString& message) {
      refreshButton->setEnabled(true);
      updateView();
      if (!message.isEmpty()) {
        m_statusLabel->setText(message);
      } else {
        m_statusLabel->setText(ok ? tr("Season refresh complete.") : tr("Season refresh failed."));
      }
    });
  });
}

void SeasonsWidget::initControls() {
  QSet<int> years;
  for (const auto& item : anime::db.items()) {
    if (const auto season = anime::Season{item.date_started}) {
      years.insert(static_cast<int>(season.year));
    }
  }

  const int currentYear = QDate::currentDate().year();
  if (years.isEmpty()) years.insert(currentYear);

  auto sortedYears = QList<int>(years.begin(), years.end());
  std::ranges::sort(sortedYears, std::greater{});
  for (const auto year : sortedYears) {
    m_yearCombo->addItem(QString::number(year), year);
  }
  if (const int index = m_yearCombo->findData(currentYear); index >= 0) m_yearCombo->setCurrentIndex(index);

  for (const auto season : {anime::SeasonName::Winter, anime::SeasonName::Spring,
                            anime::SeasonName::Summer, anime::SeasonName::Fall}) {
    m_seasonCombo->addItem(formatSeasonName(season), static_cast<int>(season));
  }
  const auto currentSeason = anime::Season{base::Date{std::chrono::year{currentYear},
                                                      std::chrono::month{static_cast<unsigned>(
                                                          QDate::currentDate().month())},
                                                      std::chrono::day{1}}};
  if (const int index = m_seasonCombo->findData(static_cast<int>(currentSeason.name)); index >= 0) {
    m_seasonCombo->setCurrentIndex(index);
  }

  m_groupCombo->addItem(tr("Type"), static_cast<int>(GroupBy::Type));
  m_groupCombo->addItem(tr("Season"), static_cast<int>(GroupBy::Season));

  m_sortCombo->addItem(tr("Score"), static_cast<int>(SortBy::Score));
  m_sortCombo->addItem(tr("Title"), static_cast<int>(SortBy::Title));
  m_sortCombo->addItem(tr("Popularity"), static_cast<int>(SortBy::Popularity));

  m_viewCombo->addItem(tr("Details"), static_cast<int>(ViewAs::Details));
  m_viewCombo->addItem(tr("Table"), static_cast<int>(ViewAs::Table));

  for (const auto* combo : {m_yearCombo, m_seasonCombo, m_groupCombo, m_sortCombo, m_viewCombo}) {
    connect(combo, &QComboBox::currentIndexChanged, this, &SeasonsWidget::updateView);
  }
}

void SeasonsWidget::scheduleUpdate() {
  if (m_updateQueued) return;

  m_updateQueued = true;
  QTimer::singleShot(0, this, [this]() {
    m_updateQueued = false;
    updateView();
  });
}

void SeasonsWidget::updateView() {
  QVector<QPair<const Anime*, anime::Season>> rows;
  const auto selectedYear = m_yearCombo->currentData().toInt();
  const auto selectedSeason =
      static_cast<anime::SeasonName>(m_seasonCombo->currentData().toInt());

  const auto requestedSeason = anime::Season{selectedSeason, std::chrono::year{selectedYear}};

  if (requestedSeason && anime::season_db.matches(requestedSeason) &&
      !anime::season_db.items.isEmpty()) {
    for (const int id : anime::season_db.items) {
      if (const auto* item = anime::db.item(id)) rows.push_back({item, anime::Season{item->date_started}});
    }
  } else {
    for (const auto& item : anime::db.items()) {
      const auto season = anime::Season{item.date_started};
      if (!season) continue;
      if (selectedYear > 0 && static_cast<int>(season.year) != selectedYear) continue;
      if (selectedSeason != anime::SeasonName::Unknown && season.name != selectedSeason) continue;
      rows.push_back({&item, season});
    }
  }

  const auto sortBy = static_cast<SortBy>(m_sortCombo->currentData().toInt());
  std::ranges::sort(rows, [sortBy](const auto& left, const auto& right) {
    switch (sortBy) {
      case SortBy::Score:
        return left.first->score > right.first->score;
      case SortBy::Popularity:
        return left.first->popularity_rank < right.first->popularity_rank;
      case SortBy::Title:
      default:
        return left.first->titles.romaji < right.first->titles.romaji;
    }
  });

  const auto viewAs = static_cast<ViewAs>(m_viewCombo->currentData().toInt());
  m_scrollArea->setVisible(viewAs == ViewAs::Details);
  m_table->setVisible(viewAs == ViewAs::Table);

  if (viewAs == ViewAs::Details) {
    updateDetails(rows);
  } else {
    updateTable(rows);
  }

  m_statusLabel->setText(requestedSeason ? formatSeason(requestedSeason) : tr("Season list"));
}

void SeasonsWidget::updateTable(const QVector<QPair<const Anime*, anime::Season>>& rows) {
  m_table->setSortingEnabled(false);
  m_table->setRowCount(0);

  for (const auto& row : rows) {
    const auto* anime = row.first;
    const auto tableRow = m_table->rowCount();
    m_table->insertRow(tableRow);
    auto* title = new QTableWidgetItem(QString::fromStdString(anime->titles.romaji));
    title->setData(Qt::UserRole, anime->id);
    m_table->setItem(tableRow, 0, title);
    m_table->setItem(tableRow, 1, new QTableWidgetItem(formatType(anime->type)));
    m_table->setItem(tableRow, 2, new QTableWidgetItem(formatSeason(row.second)));
    m_table->setItem(tableRow, 3, new QTableWidgetItem(formatNumber(anime->episode_count)));
    m_table->setItem(tableRow, 4, new QTableWidgetItem(formatScore(anime->score)));
    m_table->setItem(tableRow, 5, new QTableWidgetItem(formatStatus(anime->status)));
  }

  m_table->setSortingEnabled(true);
}

void SeasonsWidget::updateDetails(const QVector<QPair<const Anime*, anime::Season>>& rows) {
  if (auto* oldWidget = m_scrollArea->takeWidget()) oldWidget->deleteLater();

  m_detailsWidget = new QWidget(m_scrollArea);
  auto* layout = new QVBoxLayout(m_detailsWidget);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(6);

  const auto groupBy = static_cast<GroupBy>(m_groupCombo->currentData().toInt());
  QString currentGroup;
  QGridLayout* grid = nullptr;
  int cardIndex = 0;

  for (const auto& row : rows) {
    const auto group = groupBy == GroupBy::Season ? formatSeason(row.second) : formatType(row.first->type);
    if (group != currentGroup) {
      currentGroup = group;
      auto* title = new QLabel(currentGroup, m_detailsWidget);
      title->setStyleSheet("color: #0645ad; padding: 2px 0px;");
      layout->addWidget(title);
      grid = new QGridLayout();
      grid->setHorizontalSpacing(10);
      grid->setVerticalSpacing(10);
      layout->addLayout(grid);
      cardIndex = 0;
    }

    auto* card = createDetailsCard(*row.first, row.second);
    grid->addWidget(card, cardIndex / 2, cardIndex % 2);
    ++cardIndex;
  }

  layout->addStretch();
  m_scrollArea->setWidget(m_detailsWidget);
}

QWidget* SeasonsWidget::createDetailsCard(const Anime& anime, const anime::Season& season) {
  auto* card = new QFrame(m_detailsWidget);
  card->setObjectName("seasonDetailsCard");
  card->setFrameShape(QFrame::StyledPanel);
  card->setMinimumHeight(184);
  card->setContextMenuPolicy(Qt::CustomContextMenu);
  card->setStyleSheet(
      "QFrame#seasonDetailsCard:hover { background-color: #eef7ff; border-color: #7aa7d9; }");
  connect(card, &QWidget::customContextMenuRequested, this,
          [this, id = anime.id](const QPoint&) { showMediaMenu(id); });

  auto* layout = new QHBoxLayout(card);
  layout->setContentsMargins(6, 6, 6, 6);
  layout->setSpacing(8);

  auto* poster = new QLabel(card);
  poster->setFixedSize(118, 174);
  poster->setAlignment(Qt::AlignCenter);
  poster->setFrameShape(QFrame::StyledPanel);
  if (const auto* pixmap = imageProvider.loadPoster(anime.id); pixmap && !pixmap->isNull()) {
    poster->setPixmap(pixmap->scaled(poster->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
  } else {
    imageProvider.fetchPoster(anime.id);
  }
  layout->addWidget(poster);

  auto* content = new QWidget(card);
  auto* contentLayout = new QVBoxLayout(content);
  contentLayout->setContentsMargins(0, 0, 0, 0);
  contentLayout->setSpacing(3);
  layout->addWidget(content, 1);

  auto* title = new QLabel(QString::fromStdString(anime.titles.romaji), content);
  title->setStyleSheet("background-color: #e0f4e5; font-weight: 600; padding: 3px;");
  title->setWordWrap(true);
  contentLayout->addWidget(title);

  const QString details =
      QString("<b>Aired:</b> %1<br><b>Episodes:</b> %2<br><b>Genres:</b> %3<br>"
              "<b>Producers:</b> %4<br><b>Score:</b> %5<br><b>Popularity:</b> #%6")
          .arg(formatFuzzyDateRange(anime.date_started, anime.date_finished))
          .arg(formatNumber(anime.episode_count, "Unknown"))
          .arg(joinStrings(anime.genres).toHtmlEscaped())
          .arg(joinStrings(anime.producers).toHtmlEscaped())
          .arg(formatScore(anime.score))
          .arg(formatNumber(anime.popularity_rank));
  auto* detailsLabel = new QLabel(details, content);
  detailsLabel->setWordWrap(true);
  detailsLabel->setTextFormat(Qt::RichText);
  contentLayout->addWidget(detailsLabel);

  auto* synopsis = new QLabel(QString::fromStdString(anime.synopsis).toHtmlEscaped(), content);
  synopsis->setWordWrap(true);
  synopsis->setMaximumHeight(44);
  contentLayout->addWidget(synopsis);

  contentLayout->addStretch();
  card->setToolTip(formatSeason(season));
  return card;
}

void SeasonsWidget::showMediaMenu(int animeId) const {
  const auto* item = anime::db.item(animeId);
  if (!item) return;

  QMap<int, ListEntry> entries;
  if (const auto* entry = anime::db.entry(animeId)) entries.insert(animeId, *entry);

  auto* menu = new MediaMenu(const_cast<SeasonsWidget*>(this), QList<Anime>{*item}, entries, nullptr);
  menu->popup();
}

void SeasonsWidget::showTableContextMenu(const QPoint& position) const {
  const auto* item = m_table->item(m_table->rowAt(position.y()), 0);
  if (!item) return;

  const auto animeId = item->data(Qt::UserRole).toInt();
  showMediaMenu(animeId);
}

}  // namespace gui
