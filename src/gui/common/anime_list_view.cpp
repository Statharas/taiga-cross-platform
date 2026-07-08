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

#include "anime_list_view.hpp"

#include <QHeaderView>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QStatusBar>

#include "gui/common/anime_list_item_delegate.hpp"
#include "gui/main/main_window.hpp"
#include "gui/common/anime_list_view_base.hpp"
#include "gui/models/anime_list_model.hpp"
#include "gui/models/anime_list_proxy_model.hpp"
#include "gui/utils/painters.hpp"
#include "media/anime.hpp"
#include "media/anime_db.hpp"
#include "media/anime_list.hpp"
#include "sync/service.hpp"
#include "track/play.hpp"

namespace gui {

ListView::ListView(QWidget* parent, AnimeListModel* model, AnimeListProxyModel* proxyModel)
    : m_base(new ListViewBase(parent, this, model, proxyModel)) {
  setObjectName("animeList");

  setFrameShape(QFrame::Shape::NoFrame);

  setAlternatingRowColors(true);
  setItemDelegate(new ListItemDelegate(this));
  setMouseTracking(true);
  viewport()->setMouseTracking(true);

  setAllColumnsShowFocus(true);
  setExpandsOnDoubleClick(false);
  setItemsExpandable(false);
  setRootIsDecorated(false);
  setUniformRowHeights(true);

  header()->setFirstSectionMovable(true);
  header()->setStretchLastSection(false);
  header()->setTextElideMode(Qt::ElideRight);
  header()->hideSection(AnimeListModel::COLUMN_DURATION);
  header()->hideSection(AnimeListModel::COLUMN_REWATCHES);
  header()->hideSection(AnimeListModel::COLUMN_AVERAGE);
  header()->hideSection(AnimeListModel::COLUMN_STARTED);
  header()->hideSection(AnimeListModel::COLUMN_COMPLETED);
  header()->hideSection(AnimeListModel::COLUMN_NOTES);
  header()->resizeSection(AnimeListModel::COLUMN_STATUS, 22);
  header()->resizeSection(AnimeListModel::COLUMN_TITLE, 295);
  header()->resizeSection(AnimeListModel::COLUMN_PROGRESS, 150);
  header()->resizeSection(AnimeListModel::COLUMN_DURATION, 75);
  header()->resizeSection(AnimeListModel::COLUMN_SCORE, 75);
  header()->resizeSection(AnimeListModel::COLUMN_AVERAGE, 75);
  header()->resizeSection(AnimeListModel::COLUMN_TYPE, 75);
  header()->resizeSection(AnimeListModel::COLUMN_LAST_UPDATED, 110);
  header()->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(header(), &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
    QMenu menu(this);
    for (int column = 0; column < AnimeListModel::NUM_COLUMNS; ++column) {
      auto text = this->model()->headerData(column, Qt::Horizontal, Qt::DisplayRole).toString();
      if (text.isEmpty()) text = tr("Status");
      auto* action = menu.addAction(text, this, [this, column]() {
        setColumnHidden(column, !isColumnHidden(column));
      });
      action->setCheckable(true);
      action->setChecked(!isColumnHidden(column));
      action->setEnabled(column != AnimeListModel::COLUMN_TITLE);
    }
    menu.exec(header()->mapToGlobal(pos));
  });

  // `sortByColumn` needs to be called before `setSortingEnabled`.
  // Otherwise the sort column is set to `0`.
  sortByColumn(proxyModel->sortColumn(), proxyModel->sortOrder());
  setSortingEnabled(true);

  connect(this, &QAbstractItemView::clicked, this,
          qOverload<const QModelIndex&>(&QAbstractItemView::edit));
}

void ListView::leaveEvent(QEvent* event) {
  unsetCursor();
  QTreeView::leaveEvent(event);
}

void ListView::mouseMoveEvent(QMouseEvent* event) {
  const auto button = progressButtonAt(event->pos());
  if (button == ProgressButton::None) {
    unsetCursor();
    setToolTip({});
  } else {
    setCursor(Qt::PointingHandCursor);
    setToolTip(button == ProgressButton::Increment ? tr("+1 episode") : tr("-1 episode"));
  }
  QTreeView::mouseMoveEvent(event);
}

void ListView::keyPressEvent(QKeyEvent* event) {
  if (event->key() == Qt::Key::Key_Return || event->key() == Qt::Key::Key_Enter) {
    const auto indexes = selectionModel()->selectedRows();
    for (const auto& index : indexes) {
      m_base->executeConfiguredDoubleClickAction(index);
    }
    return;
  }
  if (event->key() == Qt::Key::Key_Delete) {
    m_base->removeSelectedEntries();
    return;
  }

  QTreeView::keyPressEvent(event);
}

void ListView::mousePressEvent(QMouseEvent* event) {
  if (event->button() == Qt::MouseButton::LeftButton) {
    const auto button = progressButtonAt(event->pos());
    if (button != ProgressButton::None) {
      const auto index = indexAt(event->pos());
      if (updateProgressAt(index, button == ProgressButton::Increment ? 1 : -1)) {
        setCurrentIndex(index);
        event->accept();
        return;
      }
    }
  }

  if (event->button() == Qt::MouseButton::MiddleButton) {
    const QModelIndex index = indexAt(event->pos());
    if (index.isValid()) {
      setCurrentIndex(index);
      m_base->executeConfiguredMiddleClickAction(index);
      return;
    }
  }

  QTreeView::mousePressEvent(event);
}

ListView::ProgressButton ListView::progressButtonAt(const QPoint& pos) const {
  const auto index = indexAt(pos);
  if (!index.isValid() || index.column() != AnimeListModel::COLUMN_PROGRESS) {
    return ProgressButton::None;
  }

  const auto anime = index.data(static_cast<int>(AnimeListItemDataRole::Anime)).value<const Anime*>();
  const auto entry =
      index.data(static_cast<int>(AnimeListItemDataRole::ListEntry)).value<const ListEntry*>();
  if (!anime || !entry) return ProgressButton::None;
  if (entry->status == anime::list::Status::Dropped) return ProgressButton::None;
  if (entry->status == anime::list::Status::Completed && !entry->rewatching) {
    return ProgressButton::None;
  }

  auto rect = visualRect(index);
  rect.adjust(2, 2, -2, -2);

  if (entry->watched_episodes > 0 && progressButtonRect(rect, false).contains(pos)) {
    return ProgressButton::Decrement;
  }

  const auto canIncrement = anime->episode_count <= 0 || entry->watched_episodes < anime->episode_count;
  if (canIncrement && progressButtonRect(rect, true).contains(pos)) {
    return ProgressButton::Increment;
  }

  return ProgressButton::None;
}

bool ListView::updateProgressAt(const QModelIndex& index, int delta) {
  if (!index.isValid()) return false;

  const auto anime = index.data(static_cast<int>(AnimeListItemDataRole::Anime)).value<const Anime*>();
  const auto existing =
      index.data(static_cast<int>(AnimeListItemDataRole::ListEntry)).value<const ListEntry*>();
  if (!anime || !existing) return false;
  if (existing->status == anime::list::Status::Dropped) return false;
  if (existing->status == anime::list::Status::Completed && !existing->rewatching) return false;

  auto entry = *existing;
  const auto maximum = anime->episode_count > 0 ? anime->episode_count : anime::kMaxEpisodeCount;
  entry.watched_episodes = std::clamp(entry.watched_episodes + delta, 0, maximum);
  if (entry.watched_episodes == existing->watched_episodes) return false;

  entry.last_updated = QDateTime::currentSecsSinceEpoch();
  if (anime->episode_count > 0 && entry.watched_episodes == anime->episode_count) {
    entry.status = anime::list::Status::Completed;
  } else if (!entry.rewatching && entry.status == anime::list::Status::NotInList) {
    entry.status = anime::list::Status::Watching;
  }

  mainWindow()->statusBar()->showMessage(tr("Updating list..."));
  taiga_sync::updateListEntry(entry, [entry](bool ok, const QString& message) {
    if (ok) anime::db.updateEntry(entry);
    mainWindow()->statusBar()->showMessage(message, 5000);
  });
  return true;
}

void ListView::paintEvent(QPaintEvent* event) {
  if (model() && model()->rowCount() == 0) {
    paintEmptyListText(this, tr("No items found."));
  }

  QTreeView::paintEvent(event);
}

}  // namespace gui
