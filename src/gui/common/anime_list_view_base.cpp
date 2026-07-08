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

#include "anime_list_view_base.hpp"

#include <QLineEdit>
#include <QListView>
#include <QDesktopServices>
#include <QDateTime>
#include <QMessageBox>
#include <QPushButton>
#include <QStatusBar>
#include <QTreeView>
#include <QUrl>

#include "gui/main/main_window.hpp"
#include "gui/main/navigation_item_delegate.hpp"
#include "gui/main/navigation_widget.hpp"
#include "gui/main/now_playing_widget.hpp"
#include "gui/media/media_dialog.hpp"
#include "gui/media/media_menu.hpp"
#include "gui/models/anime_list_model.hpp"
#include "gui/models/anime_list_proxy_model.hpp"
#include "gui/utils/format.hpp"
#include "media/anime.hpp"
#include "media/anime_db.hpp"
#include "media/anime_list.hpp"
#include "sync/service.hpp"
#include "taiga/settings.hpp"
#include "track/play.hpp"
#include "track/scanner.hpp"

namespace gui {

ListViewBase::ListViewBase(QWidget* parent, QAbstractItemView* view, AnimeListModel* model,
                           AnimeListProxyModel* proxyModel)
    : QObject(parent), m_view(view), m_model(model), m_proxyModel(proxyModel) {
  m_view->setContextMenuPolicy(Qt::CustomContextMenu);
  m_view->setSelectionMode(QAbstractItemView::SelectionMode::ExtendedSelection);

  m_proxyModel->setSourceModel(m_model);
  m_view->setModel(m_proxyModel);

  connect(mainWindow()->searchBox(), &QLineEdit::textChanged, this, &ListViewBase::filterByText);

  connect(m_view, &QAbstractItemView::doubleClicked, this,
          &ListViewBase::executeConfiguredDoubleClickAction);

  connect(m_view, &QWidget::customContextMenuRequested, this, &ListViewBase::showMediaMenu);

  connect(m_view->selectionModel(), &QItemSelectionModel::selectionChanged, this,
          &ListViewBase::updateSelectionStatus);
}

void ListViewBase::executeConfiguredDoubleClickAction(const QModelIndex& index) {
  executeAction(taiga::settings.intValue("program.list.doubleClickAction", 4), index);
}

void ListViewBase::executeConfiguredMiddleClickAction(const QModelIndex& index) {
  executeAction(taiga::settings.intValue("program.list.middleClickAction", 3), index);
}

void ListViewBase::executeAction(int action, const QModelIndex& index) {
  const auto mappedIndex = m_proxyModel->mapToSource(index);
  const auto anime = m_model->getAnime(mappedIndex);
  if (!anime) return;
  const auto entry = m_model->getListEntry(mappedIndex);

  switch (action) {
    case 0:
      return;
    case 1:
      MediaDialog::show(mainWindow(), MediaDialogPage::List, *anime,
                        entry ? std::optional<ListEntry>{*entry} : std::nullopt);
      return;
    case 2: {
      for (const auto& path : taiga::settings.libraryFolders()) {
        const auto folder = track::findFolder(QString::fromStdString(path), anime->id);
        if (folder) {
          QDesktopServices::openUrl(QUrl::fromLocalFile(*folder));
          return;
        }
      }
      QMessageBox::information(mainWindow(), tr("Open Folder"),
                               tr("Could not find folder for %1.")
                                   .arg(QString::fromStdString(anime->titles.romaji)));
      return;
    }
    case 3:
      track::playNextEpisode(anime->id);
      return;
    case 4:
      MediaDialog::show(mainWindow(), MediaDialogPage::Details, *anime,
                        entry ? std::optional<ListEntry>{*entry} : std::nullopt);
      return;
    case 5:
      QDesktopServices::openUrl(QUrl{taiga_sync::animePageUrl(anime->id)});
      return;
    default:
      return;
  }
}

void ListViewBase::filterByText(const QString& text) {
  m_proxyModel->setTextFilter(text);
}

void ListViewBase::playNextEpisode(const QModelIndex& index) {
  executeAction(3, index);
}

void ListViewBase::removeSelectedEntries() {
  const auto indexes = selectedIndexes();
  if (indexes.isEmpty()) return;

  QMessageBox msgBox(mainWindow());
  msgBox.setIcon(QMessageBox::Icon::Question);
  msgBox.setText(tr("Do you want to remove selected items from your list?"));
  QAbstractButton* removeButton = msgBox.addButton(tr("Remove"), QMessageBox::DestructiveRole);
  msgBox.addButton(QMessageBox::Cancel);
  msgBox.setDefaultButton(QMessageBox::Cancel);
  msgBox.exec();
  if (msgBox.clickedButton() != removeButton) return;

  for (const auto& selectedIndex : indexes) {
    const auto index = m_proxyModel->mapToSource(selectedIndex);
    if (const auto anime = m_model->getAnime(index)) {
      auto entry = m_model->getListEntry(index) ? *m_model->getListEntry(index) : ListEntry{};
      entry.id = anime->id;
      entry.anime_id = anime->id;
      entry.status = anime::list::Status::NotInList;
      entry.last_updated = QDateTime::currentSecsSinceEpoch();
      mainWindow()->statusBar()->showMessage(tr("Updating list..."));
      taiga_sync::updateListEntry(entry, [](bool, const QString& message) {
        mainWindow()->statusBar()->showMessage(message, 5000);
      });
    }
  }
}

void ListViewBase::showMediaDialog(const QModelIndex& index) {
  const auto mappedIndex = m_proxyModel->mapToSource(index);
  const auto anime = m_model->getAnime(mappedIndex);
  if (!anime) return;
  const auto entry = m_model->getListEntry(mappedIndex);
  MediaDialog::show(mainWindow(), MediaDialogPage::Details, *anime,
                    entry ? std::optional<ListEntry>{*entry} : std::nullopt);
}

void ListViewBase::showMediaMenu() {
  const auto indexes = selectedIndexes();
  if (indexes.isEmpty()) return;

  QList<Anime> items;
  QMap<int, ListEntry> entries;

  for (auto selectedIndex : indexes) {
    const auto index = m_proxyModel->mapToSource(selectedIndex);
    if (const auto item = m_model->getAnime(index)) {
      items.push_back(*item);
      if (const auto entry = m_model->getListEntry(index)) {
        entries[item->id] = *entry;
      }
    }
  }

  auto* menu = new MediaMenu(m_view, items, entries, m_view->selectionModel());
  menu->popup();
}

void ListViewBase::updateSelectionStatus(const QItemSelection&, const QItemSelection&) {
  const auto n_selected = selectedIndexes().size();

  if (!n_selected) {
    mainWindow()->statusBar()->clearMessage();
    return;
  }

  int n_episodes = 0;
  int n_score = 0;
  double total_score = 0.0;
  double average_score = 0.0;

  for (const auto index : selectedIndexes()) {
    const auto anime = m_model->getAnime(m_proxyModel->mapToSource(index));
    if (!anime) continue;
    if (anime->episode_count > 0) n_episodes += anime->episode_count;
    if (anime->score) {
      ++n_score;
      total_score += anime->score;
    }
  }

  if (n_score) average_score = total_score / n_score;

  const QStringList parts{
      tr("%n item(s) selected", nullptr, n_selected),
      tr("%n episode(s)", nullptr, n_episodes),
      tr("%1 average").arg(formatScore(average_score)),
  };

  mainWindow()->statusBar()->showMessage(parts.join(" · "));
}

QModelIndexList ListViewBase::selectedIndexes() {
  const auto model = m_view->selectionModel();
  return model->selectedRows().size() ? model->selectedRows() : model->selectedIndexes();
}

}  // namespace gui
