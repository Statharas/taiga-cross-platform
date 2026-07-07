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

#include "history_model.hpp"

#include <QApplication>
#include <QPalette>
#include <anitomy.hpp>
#include <anitomy/detail/keyword.hpp>  // don't try this at home
#include <ranges>
#include <algorithm>

#include "base/string.hpp"
#include "media/anime_db.hpp"
#include "media/anime_history.hpp"
#include "track/episode.hpp"
#include "track/recognition.hpp"

namespace gui {

HistoryModel::HistoryModel(QObject* parent) : QAbstractListModel(parent) {
  refreshRows();
}

int HistoryModel::rowCount(const QModelIndex&) const {
  return rows_.size();
}

int HistoryModel::columnCount(const QModelIndex&) const {
  return NUM_COLUMNS;
}

QVariant HistoryModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid()) return {};

  switch (role) {
    case Qt::DisplayRole: {
      const auto& historyItem = itemAt(index.row());
      const auto item = anime::db.item(historyItem.anime_id);
      switch (index.column()) {
        case COLUMN_TITLE:
          if (item) return QString::fromStdString(item->titles.romaji);
          return {};
        case COLUMN_DETAILS:
          return tr("Episode: %1").arg(historyItem.episode);
        case COLUMN_MODIFIED:
          return QString::fromStdString(historyItem.time);
      }
      break;
    }

    case Qt::TextAlignmentRole: {
      switch (index.column()) {
        case COLUMN_MODIFIED:
          return QVariant(Qt::AlignRight | Qt::AlignVCenter);
      }
      break;
    }
  }

  return {};
}

QVariant HistoryModel::headerData(int section, Qt::Orientation orientation, int role) const {
  switch (role) {
    case Qt::DisplayRole: {
      // clang-format off
      switch (section) {
        case COLUMN_TITLE: return tr("Title");
        case COLUMN_DETAILS: return tr("Details");
        case COLUMN_MODIFIED: return tr("Last modified");
      }
      // clang-format on
      break;
    }

    case Qt::TextAlignmentRole: {
      switch (section) {
        case COLUMN_TITLE:
        case COLUMN_DETAILS:
          return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        case COLUMN_MODIFIED:
          return QVariant(Qt::AlignRight | Qt::AlignVCenter);
      }
      break;
    }

    case Qt::InitialSortOrderRole: {
      switch (section) {
        case COLUMN_MODIFIED:
          return Qt::DescendingOrder;
        default:
          return Qt::AscendingOrder;
      }
      break;
    }
  }

  return QAbstractListModel::headerData(section, orientation, role);
}

void HistoryModel::reset() {
  beginResetModel();
  refreshRows();
  endResetModel();
}

void HistoryModel::sort(int column, Qt::SortOrder order) {
  emit layoutAboutToBeChanged();
  const auto descending = order == Qt::DescendingOrder;
  std::stable_sort(rows_.begin(), rows_.end(), [column, descending](int lhsRow, int rhsRow) {
    const auto& lhs = anime::history.items().at(lhsRow);
    const auto& rhs = anime::history.items().at(rhsRow);
    int comparison = 0;
    switch (column) {
      case COLUMN_TITLE: {
        const auto lhsAnime = anime::db.item(lhs.anime_id);
        const auto rhsAnime = anime::db.item(rhs.anime_id);
        const auto lhsTitle = lhsAnime ? QString::fromStdString(lhsAnime->titles.romaji) : QString{};
        const auto rhsTitle = rhsAnime ? QString::fromStdString(rhsAnime->titles.romaji) : QString{};
        comparison = QString::compare(lhsTitle, rhsTitle, Qt::CaseInsensitive);
        break;
      }
      case COLUMN_DETAILS:
        comparison = lhs.episode < rhs.episode ? -1 : (lhs.episode > rhs.episode ? 1 : 0);
        break;
      case COLUMN_MODIFIED:
      default:
        comparison = QString::fromStdString(lhs.time).compare(QString::fromStdString(rhs.time));
        break;
    }
    return descending ? comparison > 0 : comparison < 0;
  });
  emit layoutChanged();
}

anime::HistoryItem HistoryModel::itemAt(int row) const {
  return anime::history.items().at(sourceRow(row));
}

int HistoryModel::sourceRow(int row) const {
  if (row < 0 || row >= rows_.size()) return row;
  return rows_.at(row);
}

void HistoryModel::refreshRows() {
  rows_.clear();
  rows_.reserve(anime::history.items().size());
  for (int i = 0; i < anime::history.items().size(); ++i) rows_.push_back(i);
}

}  // namespace gui
