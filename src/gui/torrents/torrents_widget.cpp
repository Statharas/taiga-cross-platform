/**
 * Taiga
 * Copyright (C) 2010-2024, Eren Okka
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "torrents_widget.hpp"

#include <QAction>
#include <QDesktopServices>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QSizePolicy>
#include <QTableWidget>
#include <QToolBar>
#include <QUrl>
#include <QVBoxLayout>

#include "base/string.hpp"
#include "gui/settings/settings_dialog.hpp"
#include "media/anime_db.hpp"
#include "taiga/network.hpp"
#include "taiga/settings.hpp"

namespace gui {

namespace {

QString formatSize(const QString& bytesText) {
  bool ok = false;
  auto bytes = bytesText.toLongLong(&ok);
  if (!ok || bytes <= 0) return {};

  static constexpr std::array units{"B", "KiB", "MiB", "GiB"};
  auto size = static_cast<double>(bytes);
  int unit = 0;
  while (size >= 1024.0 && unit < static_cast<int>(units.size()) - 1) {
    size /= 1024.0;
    ++unit;
  }
  return unit == 0 ? QString("%1 %2").arg(bytes).arg(units[unit])
                   : QString("%1 %2").arg(size, 0, 'f', 1).arg(units[unit]);
}

QString statusIconPath(anime::Status status) {
  switch (status) {
    case anime::Status::Airing:
      return ":/icons/classic/16px/square-small-green.png";
    case anime::Status::FinishedAiring:
      return ":/icons/classic/16px/square-small-blue.png";
    case anime::Status::NotYetAired:
      return ":/icons/classic/16px/square-small-red.png";
    case anime::Status::Unknown:
      break;
  }
  return ":/icons/classic/16px/square-small-gray.png";
}

QIcon torrentIcon(const track::torrent::Item& item) {
  for (const auto& anime : anime::db.items()) {
    const auto title = QString::fromStdString(anime.titles.romaji);
    if (!title.isEmpty() && item.title.contains(title, Qt::CaseInsensitive)) {
      return QIcon(statusIconPath(anime.status));
    }
  }
  return QIcon(statusIconPath(anime::Status::Unknown));
}

}  // namespace

TorrentsWidget::TorrentsWidget(QWidget* parent) : QWidget(parent) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(8, 4, 8, 4);
  layout->setSpacing(4);

  auto* toolbar = new QToolBar(this);
  toolbar->setIconSize(QSize{16, 16});
  toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  toolbar->setMovable(false);
  toolbar->setFloatable(false);
  auto* refreshAction =
      toolbar->addAction(QIcon(":/icons/classic/16px/arrow-circle-315.png"),
                         tr("Check new torrents"));
  toolbar->addSeparator();
  auto* openAction =
      toolbar->addAction(QIcon(":/icons/classic/16px/navigation-270-button.png"),
                         tr("Download marked torrents"));
  auto* discardAllAction =
      toolbar->addAction(QIcon(":/icons/classic/16px/cross.png"), tr("Discard all"));
  toolbar->addSeparator();
  auto* settingsAction =
      toolbar->addAction(QIcon(":/icons/classic/16px/gear.png"), tr("Settings"));
  auto* spacer = new QWidget(toolbar);
  spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  toolbar->addWidget(spacer);
  filterEdit_ = new QLineEdit(toolbar);
  filterEdit_->setPlaceholderText(tr("Search for torrents"));
  filterEdit_->setMaximumWidth(240);
  toolbar->addWidget(filterEdit_);
  layout->addWidget(toolbar);

  table_ = new QTableWidget(this);
  table_->setColumnCount(11);
  table_->setHorizontalHeaderLabels({tr("Anime title"), tr("Episode"), tr("Group"), tr("Size"),
                                     tr("Video"), tr("S"), tr("L"), tr("D"),
                                     tr("Description"), tr("Filename"), tr("Release date")});
  table_->setSelectionBehavior(QAbstractItemView::SelectRows);
  table_->setSelectionMode(QAbstractItemView::ExtendedSelection);
  table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  table_->setContextMenuPolicy(Qt::CustomContextMenu);
  table_->setSortingEnabled(true);
  table_->horizontalHeader()->setStretchLastSection(false);
  table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
  table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
  table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
  table_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
  table_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
  table_->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
  table_->horizontalHeader()->setSectionResizeMode(7, QHeaderView::ResizeToContents);
  table_->horizontalHeader()->setSectionResizeMode(8, QHeaderView::Stretch);
  table_->horizontalHeader()->setSectionResizeMode(9, QHeaderView::Stretch);
  table_->horizontalHeader()->setSectionResizeMode(10, QHeaderView::ResizeToContents);
  layout->addWidget(table_, 1);

  statusLabel_ = new QLabel(this);
  layout->addWidget(statusLabel_);

  connect(refreshAction, &QAction::triggered, this, &TorrentsWidget::fetch);
  connect(openAction, &QAction::triggered, this, &TorrentsWidget::openSelected);
  connect(discardAllAction, &QAction::triggered, this, &TorrentsWidget::archiveVisible);
  connect(settingsAction, &QAction::triggered, this, &TorrentsWidget::showSettings);
  connect(filterEdit_, &QLineEdit::textChanged, this, &TorrentsWidget::populate);
  connect(table_, &QTableWidget::itemDoubleClicked, this, [this]() { openSelected(); });
  connect(table_, &QWidget::customContextMenuRequested, this, &TorrentsWidget::showContextMenu);

  populate();
}

void TorrentsWidget::archiveSelected() {
  std::vector<track::torrent::Item> selected;
  for (int row = 0; row < table_->rowCount(); ++row) {
    if (table_->item(row, 0)->checkState() != Qt::Checked &&
        !table_->selectionModel()->isRowSelected(row, {})) {
      continue;
    }
    const auto sourceRow = table_->item(row, 0)->data(Qt::UserRole).toInt();
    if (sourceRow >= 0 && sourceRow < static_cast<int>(items_.size())) {
      selected.push_back(items_.at(sourceRow));
    }
  }
  track::torrent::archiveItems(selected);
  track::torrent::applyFilters(items_);
  populate();
}

void TorrentsWidget::archiveVisible() {
  std::vector<track::torrent::Item> visible;
  for (int row = 0; row < table_->rowCount(); ++row) {
    const auto sourceRow = table_->item(row, 0)->data(Qt::UserRole).toInt();
    if (sourceRow >= 0 && sourceRow < static_cast<int>(items_.size())) {
      visible.push_back(items_.at(sourceRow));
    }
  }
  track::torrent::archiveItems(visible);
  track::torrent::applyFilters(items_);
  populate();
}

void TorrentsWidget::fetch() {
  const auto source = taiga::settings.stringValue("rss.torrent.source",
                                                  "https://www.tokyotosho.info/rss.php?filter=1,11&zwnj=0");
  const QUrl url{source};
  if (!url.isValid() || url.scheme().isEmpty()) {
    statusLabel_->setText(tr("Torrent feed URL is invalid."));
    return;
  }

  setBusy(true);
  QNetworkRequest request{url};
  request.setHeaders(taiga::NetworkAccessManager::commonHeaders());
  request.setRawHeader("Accept", "application/rss+xml, */*");
  request.setTransferTimeout(std::chrono::seconds{30});
  auto* reply = taiga::network()->get(request);
  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    setBusy(false);
    if (reply->error() != QNetworkReply::NoError) {
      statusLabel_->setText(reply->errorString());
      return;
    }

    items_ = track::torrent::parseFeed(reply->readAll());
    populate();
  });
}

void TorrentsWidget::openSelected() const {
  for (int row = 0; row < table_->rowCount(); ++row) {
    if (table_->item(row, 0)->checkState() != Qt::Checked &&
        !table_->selectionModel()->isRowSelected(row, {})) {
      continue;
    }
    const auto sourceRow = table_->item(row, 0)->data(Qt::UserRole).toInt();
    if (sourceRow >= 0 && sourceRow < static_cast<int>(items_.size())) {
      const auto link = items_.at(sourceRow).link;
      if (!link.isEmpty()) QDesktopServices::openUrl(QUrl{link});
    }
  }
}

void TorrentsWidget::populate() {
  const auto filter = filterEdit_ ? filterEdit_->text().trimmed() : QString{};
  table_->setRowCount(0);

  for (int i = 0; i < static_cast<int>(items_.size()); ++i) {
    const auto& item = items_.at(i);
    if (!filter.isEmpty() && !item.title.contains(filter, Qt::CaseInsensitive)) continue;

    const auto row = table_->rowCount();
    table_->insertRow(row);
    auto* title = new QTableWidgetItem(item.title);
    title->setIcon(torrentIcon(item));
    title->setData(Qt::UserRole, i);
    title->setCheckState(item.state == track::torrent::ItemState::Selected ||
                                 item.state == track::torrent::ItemState::Preferred
                             ? Qt::Checked
                             : Qt::Unchecked);
    title->setToolTip(track::torrent::stateText(item.state) + "\n" + item.matchedFilters.join(", "));
    table_->setItem(row, 0, title);
    table_->setItem(row, 1, new QTableWidgetItem(item.episode));
    table_->setItem(row, 2, new QTableWidgetItem(item.group));
    table_->setItem(row, 3, new QTableWidgetItem(formatSize(item.size)));
    table_->setItem(row, 4, new QTableWidgetItem(item.video));
    table_->setItem(row, 5, new QTableWidgetItem(item.seeders));
    table_->setItem(row, 6, new QTableWidgetItem(item.leechers));
    table_->setItem(row, 7, new QTableWidgetItem(item.downloads));
    table_->setItem(row, 8, new QTableWidgetItem(item.description));
    table_->setItem(row, 9, new QTableWidgetItem(item.filename));
    table_->setItem(row, 10, new QTableWidgetItem(item.published));
  }

  statusLabel_->setText(tr("%1 torrent(s), %2 archived")
                            .arg(table_->rowCount())
                            .arg(track::torrent::archiveCount()));
  if (table_->rowCount() == 0) {
    statusLabel_->setText(tr("No new torrents found."));
  }
}

void TorrentsWidget::setBusy(bool busy) {
  setEnabled(!busy);
  statusLabel_->setText(busy ? tr("Loading torrent feed...") : QString{});
}

void TorrentsWidget::showSettings() {
  SettingsDialog::show(this);
}

void TorrentsWidget::showContextMenu(const QPoint& position) {
  QMenu menu(this);
  menu.addAction(QIcon(":/icons/classic/16px/navigation-270-button.png"),
                 tr("Download marked torrents"), this, &TorrentsWidget::openSelected);
  menu.addAction(QIcon(":/icons/classic/16px/cross.png"), tr("Discard marked torrents"), this,
                 &TorrentsWidget::archiveSelected);
  menu.addSeparator();
  menu.addAction(QIcon(":/icons/classic/16px/arrow-circle-315.png"), tr("Check new torrents"),
                 this, &TorrentsWidget::fetch);
  menu.addAction(QIcon(":/icons/classic/16px/gear.png"), tr("Settings"), this,
                 &TorrentsWidget::showSettings);
  menu.exec(table_->viewport()->mapToGlobal(position));
}

}  // namespace gui
