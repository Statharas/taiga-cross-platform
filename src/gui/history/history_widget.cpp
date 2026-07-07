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

#include "history_widget.hpp"

#include <QDesktopServices>
#include <QHeaderView>
#include <QLayout>
#include <QMenu>
#include <QMessageBox>
#include <QUrl>

#include "gui/main/main_window.hpp"
#include "gui/media/media_dialog.hpp"
#include "gui/models/history_model.hpp"
#include "gui/utils/theme.hpp"
#include "media/anime_db.hpp"
#include "media/anime_history.hpp"
#include "taiga/settings.hpp"

namespace gui {

HistoryWidget::HistoryWidget(QWidget* parent)
    : PageWidget{parent}, m_model(new HistoryModel(parent)), m_view(new QTreeView(parent)) {
  m_view->setObjectName("historyView");
  m_view->setFrameShape(QFrame::Shape::NoFrame);
  m_view->setModel(m_model);
  m_view->setAlternatingRowColors(true);
  m_view->setAllColumnsShowFocus(true);
  m_view->setContextMenuPolicy(Qt::CustomContextMenu);
  m_view->setRootIsDecorated(false);
  m_view->setUniformRowHeights(true);

  m_view->header()->setSectionsClickable(false);
  m_view->header()->setSectionsMovable(false);
  m_view->header()->setStretchLastSection(false);
  m_view->header()->setTextElideMode(Qt::ElideRight);
  m_view->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
  m_view->header()->setSectionResizeMode(HistoryModel::COLUMN_TITLE, QHeaderView::Stretch);
  m_view->header()->setSectionResizeMode(HistoryModel::COLUMN_DETAILS, QHeaderView::Stretch);

  m_view->sortByColumn(HistoryModel::COLUMN_MODIFIED, Qt::SortOrder::DescendingOrder);
  m_view->setSortingEnabled(true);

  layout()->addWidget(m_view);

  connect(m_view, &QWidget::customContextMenuRequested, this, &HistoryWidget::showContextMenu);
}

void HistoryWidget::showContextMenu() const {
  const auto index = m_view->currentIndex();

  if (!index.isValid()) return;

  const auto row = index.row();
  if (row < 0 || row >= anime::history.items().size()) return;

  const auto historyItem = anime::history.items().at(row);
  auto* menu = new QMenu(m_view);
  menu->setAttribute(Qt::WA_DeleteOnClose);

  if (const auto item = anime::db.item(historyItem.anime_id)) {
    menu->addAction(theme.getIcon("info"), tr("Details"), this, [this, item]() {
      const auto entry = anime::db.entry(item->id);
      MediaDialog::show(parentWidget(), MediaDialogPage::Details, *item,
                        entry ? std::optional<ListEntry>{*entry} : std::nullopt);
    });
  }

  menu->addAction(theme.getIcon("delete"), tr("Clear history"), this, [this]() {
    if (QMessageBox::question(parentWidget(), tr("Clear history"),
                              tr("Clear all history items?")) != QMessageBox::Yes) {
      return;
    }
    anime::history.clear();
    m_model->reset();
  });

  menu->popup(QCursor::pos());
}

}  // namespace gui
