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
#include <QApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QShortcut>
#include <QSizePolicy>
#include <QTableWidget>
#include <QTimer>
#include <QToolBar>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <algorithm>
#include <optional>

#include "base/string.hpp"
#include "gui/settings/settings_dialog.hpp"
#include "gui/main/main_window.hpp"
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
  if (const auto anime = anime::db.item(item.anime_id)) {
    return QIcon(statusIconPath(anime->status));
  }
  return QIcon(statusIconPath(anime::Status::Unknown));
}

bool isGroupRow(const QTableWidgetItem* item) {
  return item && item->data(Qt::UserRole).toInt() < 0;
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
  refreshAction_ = toolbar->addAction(QIcon(":/icons/classic/16px/arrow-circle-315.png"),
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

  connect(refreshAction_, &QAction::triggered, this, &TorrentsWidget::fetch);
  connect(openAction, &QAction::triggered, this, &TorrentsWidget::openSelected);
  connect(discardAllAction, &QAction::triggered, this, &TorrentsWidget::archiveVisible);
  connect(settingsAction, &QAction::triggered, this, &TorrentsWidget::showSettings);
  connect(table_, &QTableWidget::itemDoubleClicked, this, [this]() { openSelected(); });
  connect(table_, &QTableWidget::itemChanged, this, &TorrentsWidget::handleItemChanged);
  connect(table_, &QWidget::customContextMenuRequested, this, &TorrentsWidget::showContextMenu);
  connect(new QShortcut(QKeySequence{Qt::Key_Return}, table_), &QShortcut::activated, this,
          &TorrentsWidget::openSelected);
  connect(new QShortcut(QKeySequence{Qt::Key_Enter}, table_), &QShortcut::activated, this,
          &TorrentsWidget::openSelected);

  autoCheckTimer_ = new QTimer(this);
  connect(autoCheckTimer_, &QTimer::timeout, this, &TorrentsWidget::tickAutoCheck);
  resetAutoCheckTimer();
  populate();
}

void TorrentsWidget::archiveSelected() {
  std::vector<track::torrent::Item> selected;
  for (int row = 0; row < table_->rowCount(); ++row) {
    if (isGroupRow(table_->item(row, 0))) continue;
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
    if (isGroupRow(table_->item(row, 0))) continue;
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
  if (QApplication::keyboardModifiers().testFlag(Qt::ControlModifier)) {
    fetchFromCache();
    return;
  }

  const auto source = taiga::settings.stringValue("rss.torrent.source",
                                                  "https://www.tokyotosho.info/rss.php?filter=1,11&zwnj=0");
  fetchUrl(QUrl{source});
}

void TorrentsWidget::fetchUrl(const QUrl& url) {
  if (!url.isValid() || url.scheme().isEmpty()) {
    statusLabel_->setText(tr("Torrent feed URL is invalid."));
    return;
  }

  setBusy(true);
  resetAutoCheckTimer();
  QNetworkRequest request{url};
  request.setHeaders(taiga::NetworkAccessManager::commonHeaders());
  request.setRawHeader("Accept", "application/rss+xml, */*");
  request.setTransferTimeout(std::chrono::seconds{30});
  auto* reply = taiga::network()->get(request);
  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    setBusy(false);
    if (reply->error() != QNetworkReply::NoError) {
      const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
      statusLabel_->setText(status > 0 ? tr("%1 returned an error (%2): %3")
                                             .arg(reply->url().host())
                                             .arg(status)
                                             .arg(reply->errorString())
                                       : reply->errorString());
      return;
    }

    const auto data = reply->readAll();
    auto feed = track::torrent::parseFeedDocument(data);
    if (feed.link.isEmpty()) feed.link = reply->url().toString();
    track::torrent::saveFeedCache(feed, data);
    items_ = std::move(feed.items);
    populate();
  });
}

void TorrentsWidget::submitSearch(const QString& text) {
  const auto query = text.trimmed();
  if (query.isEmpty()) {
    fetch();
    return;
  }

  auto urlTemplate = taiga::settings.stringValue("rss.torrent.search",
                                                 "https://nyaa.si/?page=rss&c=1_2&f=0&q=%title%");
  urlTemplate.replace("%title%", QString::fromUtf8(QUrl::toPercentEncoding(query)));
  fetchUrl(QUrl{urlTemplate});
}

void TorrentsWidget::discardSameAnime() {
  const auto* selected = currentItem();
  if (!selected) return;
  for (auto& item : items_) {
    if (selected->anime_id > 0 && item.anime_id == selected->anime_id) {
      item.state = track::torrent::ItemState::Discarded;
    }
  }
  populate();
}

void TorrentsWidget::discardSameGroup() {
  const auto* selected = currentItem();
  if (!selected || selected->group.isEmpty()) return;
  for (auto& item : items_) {
    if (item.group.compare(selected->group, Qt::CaseInsensitive) == 0) {
      item.state = track::torrent::ItemState::Discarded;
    }
  }
  populate();
}

void TorrentsWidget::fetchFromCache() {
  const auto source = taiga::settings.stringValue("rss.torrent.source",
                                                  "https://www.tokyotosho.info/rss.php?filter=1,11&zwnj=0");
  const auto feed = track::torrent::loadFeedCache(source);
  if (!feed) {
    statusLabel_->setText(tr("No cached torrent feed found."));
    return;
  }
  items_ = feed->items;
  populate();
  statusLabel_->setText(tr("Loaded cached torrent feed."));
}

void TorrentsWidget::handleItemChanged(QTableWidgetItem* item) {
  if (populating_ || !item || item->column() != 0 || isGroupRow(item)) return;

  const auto sourceRow = item->data(Qt::UserRole).toInt();
  if (sourceRow < 0 || sourceRow >= static_cast<int>(items_.size())) return;

  const bool checked = item->checkState() == Qt::Checked;
  if (QApplication::keyboardModifiers().testFlag(Qt::ShiftModifier) && lastCheckedRow_ > -1) {
    const int from = std::min(lastCheckedRow_, item->row());
    const int to = std::max(lastCheckedRow_, item->row());
    const auto category = items_.at(sourceRow).torrent_category;
    populating_ = true;
    for (int row = from; row <= to; ++row) {
      auto* rowItem = table_->item(row, 0);
      if (isGroupRow(rowItem)) continue;
      const auto rowSource = rowItem->data(Qt::UserRole).toInt();
      if (rowSource < 0 || rowSource >= static_cast<int>(items_.size())) continue;
      if (items_.at(rowSource).torrent_category != category) continue;
      rowItem->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
      items_[rowSource].state =
          checked ? track::torrent::ItemState::Selected : track::torrent::ItemState::Discarded;
    }
    populating_ = false;
  }

  items_[sourceRow].state =
      checked ? track::torrent::ItemState::Selected : track::torrent::ItemState::Discarded;
  if (checked) lastCheckedRow_ = item->row();
  const auto count = track::torrent::selectedItems(items_).size();
  statusLabel_->setText(count == 1 ? tr("Marked 1 torrent.")
                                   : tr("Marked %1 torrents.").arg(count));
}

void TorrentsWidget::openSelected() const {
  const auto queue = track::torrent::sortedDownloadQueue(items_);
  if (!queue.isEmpty()) {
    for (const auto& item : queue) startDownload(item);
    return;
  }

  const auto selected = table_->selectionModel()->selectedRows();
  for (const auto& index : selected) {
    const auto sourceRow = table_->item(index.row(), 0)->data(Qt::UserRole).toInt();
    if (sourceRow >= 0 && sourceRow < static_cast<int>(items_.size())) startDownload(items_.at(sourceRow));
  }
}

void TorrentsWidget::setFilterText(const QString& text) {
  const auto next = text.trimmed();
  if (filterText_ == next) return;
  filterText_ = next;
  populate();
}

void TorrentsWidget::preferSameGroup() {
  const auto* selected = currentItem();
  if (!selected || selected->group.isEmpty()) return;
  for (auto& item : items_) {
    if (selected->anime_id > 0 && item.anime_id != selected->anime_id) continue;
    if (item.group.compare(selected->group, Qt::CaseInsensitive) == 0) {
      item.state = track::torrent::ItemState::Preferred;
    } else if (selected->anime_id > 0) {
      item.state = track::torrent::ItemState::Discarded;
    }
  }
  populate();
}

void TorrentsWidget::populate() {
  populating_ = true;
  const auto filter = filterText_;
  table_->setRowCount(0);
  lastCheckedRow_ = -1;
  std::optional<track::torrent::Category> currentCategory;

  for (int i = 0; i < static_cast<int>(items_.size()); ++i) {
    const auto& item = items_.at(i);
    if (track::torrent::isHidden(item.state)) continue;
    if (!filter.isEmpty() && !item.title.contains(filter, Qt::CaseInsensitive)) continue;

    if (!currentCategory || *currentCategory != item.torrent_category) {
      currentCategory = item.torrent_category;
      const auto groupRow = table_->rowCount();
      table_->insertRow(groupRow);
      auto* groupItem = new QTableWidgetItem(track::torrent::categoryText(item.torrent_category));
      groupItem->setData(Qt::UserRole, -1);
      auto font = groupItem->font();
      font.setBold(true);
      groupItem->setFont(font);
      groupItem->setFlags(Qt::NoItemFlags);
      table_->setItem(groupRow, 0, groupItem);
      table_->setSpan(groupRow, 0, 1, table_->columnCount());
    }

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
                            .arg(items_.size())
                            .arg(track::torrent::archiveCount()));
  if (table_->rowCount() == 0) {
    statusLabel_->setText(tr("No new torrents found."));
  }
  populating_ = false;
}

void TorrentsWidget::resetAutoCheckTimer() {
  if (!autoCheckTimer_) return;
  if (!taiga::settings.boolValue("rss.torrent.autoCheck", true)) {
    autoCheckTimer_->stop();
    if (refreshAction_) refreshAction_->setText(tr("Check new torrents"));
    return;
  }
  autoCheckRemaining_ = std::max(1, taiga::settings.intValue("rss.torrent.checkInterval", 60)) * 60;
  autoCheckTimer_->start(1000);
}

void TorrentsWidget::setBusy(bool busy) {
  setEnabled(!busy);
  statusLabel_->setText(busy ? tr("Loading torrent feed...") : QString{});
}

void TorrentsWidget::showSettings() {
  SettingsDialog::show(this);
}

void TorrentsWidget::showContextMenu(const QPoint& position) {
  const auto* item = table_->item(table_->currentRow(), 0);
  if (!item || isGroupRow(item)) return;

  QMenu menu(this);
  menu.addAction(QIcon(":/icons/classic/16px/navigation-270-button.png"),
                 tr("Download marked torrents"), this, &TorrentsWidget::openSelected);
  menu.addAction(QIcon(":/icons/classic/16px/document-attribute.png"), tr("Torrent info"), this,
                 &TorrentsWidget::openTorrentInfo);
  menu.addAction(QIcon(":/icons/classic/16px/magnifier-left.png"), tr("More torrents"), this,
                 &TorrentsWidget::openMoreTorrents);
  menu.addAction(QIcon(":/icons/classic/16px/magnifier-left.png"), tr("Search service"), this,
                 &TorrentsWidget::searchService);
  menu.addSeparator();
  auto* filters = menu.addMenu(tr("Filters"));
  filters->addAction(tr("Prefer this fansub group"), this, &TorrentsWidget::preferSameGroup);
  filters->addAction(tr("Discard this anime"), this, &TorrentsWidget::discardSameAnime);
  filters->addAction(tr("Discard this fansub group"), this, &TorrentsWidget::discardSameGroup);
  menu.addSeparator();
  menu.addAction(QIcon(":/icons/classic/16px/cross.png"), tr("Discard marked torrents"), this,
                 &TorrentsWidget::archiveSelected);
  menu.addSeparator();
  menu.addAction(QIcon(":/icons/classic/16px/arrow-circle-315.png"), tr("Check new torrents"),
                 this, &TorrentsWidget::fetch);
  menu.addAction(QIcon(":/icons/classic/16px/gear.png"), tr("Settings"), this,
                 &TorrentsWidget::showSettings);
  menu.exec(table_->viewport()->mapToGlobal(position));
}

void TorrentsWidget::openTorrentInfo() const {
  const auto row = table_->currentRow();
  if (row < 0 || isGroupRow(table_->item(row, 0))) return;
  const auto sourceRow = table_->item(row, 0)->data(Qt::UserRole).toInt();
  if (sourceRow < 0 || sourceRow >= static_cast<int>(items_.size())) return;
  const auto& item = items_.at(sourceRow);
  const auto url = !item.info_link.isEmpty() ? item.info_link : item.link;
  if (!url.isEmpty()) QDesktopServices::openUrl(QUrl{url});
}

void TorrentsWidget::openMoreTorrents() const {
  const auto row = table_->currentRow();
  if (row < 0 || isGroupRow(table_->item(row, 0))) return;
  const auto sourceRow = table_->item(row, 0)->data(Qt::UserRole).toInt();
  if (sourceRow < 0 || sourceRow >= static_cast<int>(items_.size())) return;
  const auto& item = items_.at(sourceRow);
  auto urlTemplate = taiga::settings.stringValue("rss.torrent.search",
                                                 "https://nyaa.si/?page=rss&c=1_2&f=0&q=%title%");
  urlTemplate.replace("%title%", QUrl::toPercentEncoding(item.title));
  QDesktopServices::openUrl(QUrl{urlTemplate});
}

void TorrentsWidget::searchService() const {
  const auto row = table_->currentRow();
  if (row < 0 || isGroupRow(table_->item(row, 0))) return;
  const auto sourceRow = table_->item(row, 0)->data(Qt::UserRole).toInt();
  if (sourceRow < 0 || sourceRow >= static_cast<int>(items_.size())) return;
  mainWindow()->navigateTo(MainWindowPage::Search);
  mainWindow()->searchBox()->setText(items_.at(sourceRow).title);
}

void TorrentsWidget::startDownload(const track::torrent::Item& item) const {
  const auto plan = track::torrent::downloadPlan(item);
  if (!plan) return;

  if (plan->magnet) {
    QDesktopServices::openUrl(plan->url);
    track::torrent::archiveItems({item});
    return;
  }

  QNetworkRequest request{plan->url};
  request.setHeaders(taiga::NetworkAccessManager::commonHeaders());
  request.setRawHeader("Accept", "application/x-bittorrent, */*");
  request.setTransferTimeout(std::chrono::seconds{30});
  auto* reply = taiga::network()->get(request);
  const auto path = plan->save_path;
  connect(reply, &QNetworkReply::finished, this, [reply, path, item]() {
    if (reply->error() != QNetworkReply::NoError) return;
    QFileInfo info{path};
    QDir{}.mkpath(info.absolutePath());
    QFile file{path};
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;
    file.write(reply->readAll());
    file.close();
    track::torrent::archiveItems({item});
    if (taiga::settings.boolValue("rss.torrent.openApp", true)) {
      QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    }
  });
}

void TorrentsWidget::tickAutoCheck() {
  if (autoCheckRemaining_ <= 0) {
    fetch();
    return;
  }
  --autoCheckRemaining_;
  if (refreshAction_) {
    const auto minutes = autoCheckRemaining_ / 60;
    const auto seconds = autoCheckRemaining_ % 60;
    refreshAction_->setText(tr("Check new torrents (%1:%2)")
                                .arg(minutes, 2, 10, QLatin1Char('0'))
                                .arg(seconds, 2, 10, QLatin1Char('0')));
  }
}

const track::torrent::Item* TorrentsWidget::currentItem() const {
  const auto row = table_->currentRow();
  if (row < 0 || isGroupRow(table_->item(row, 0))) return nullptr;
  const auto sourceRow = table_->item(row, 0)->data(Qt::UserRole).toInt();
  if (sourceRow < 0 || sourceRow >= static_cast<int>(items_.size())) return nullptr;
  return &items_.at(sourceRow);
}

}  // namespace gui
