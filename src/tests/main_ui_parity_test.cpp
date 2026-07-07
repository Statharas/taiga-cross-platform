#include <QApplication>
#include <QFile>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QStackedWidget>
#include <QTableWidget>
#include <QToolBar>
#include <QTreeWidget>

#include <cstdlib>
#include <iostream>

#include "gui/main/main_window.hpp"
#include "gui/main/now_playing_page_widget.hpp"
#include "gui/models/anime_list_model.hpp"
#include "gui/main/navigation_widget.hpp"
#include "gui/torrents/torrents_widget.hpp"

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(EXIT_FAILURE);
  }
}

QStringList menuTitles(const QMenuBar* menuBar) {
  QStringList titles;
  for (const auto* action : menuBar->actions()) {
    if (const auto* menu = action->menu(); action->isVisible() && menu) {
      titles.push_back(menu->title().remove('&'));
    }
  }
  return titles;
}

QStringList topLevelItems(const QTreeWidget& tree) {
  QStringList items;
  for (int i = 0; i < tree.topLevelItemCount(); ++i) {
    const auto* item = tree.topLevelItem(i);
    if (item->text(0).isEmpty()) continue;
    items.push_back(item->text(0));
  }
  return items;
}

}  // namespace

int main(int argc, char* argv[]) {
  qputenv("QT_QPA_PLATFORM", "offscreen");

  QApplication app(argc, argv);
  QCoreApplication::setApplicationName("taiga-main-ui-parity-test");
  QCoreApplication::setOrganizationName("taiga");

  gui::MainWindow window;
  const auto* menuBar = window.findChild<QMenuBar*>();
  const auto* stack = window.findChild<QStackedWidget*>();

  require(menuBar != nullptr, "Main menu bar was not created");
  require(stack != nullptr, "Main stacked widget was not created");
  require(stack->count() == 9, "Main page count changed");
  require(menuTitles(menuBar) == QStringList({"File", "Services", "Tools", "View", "Help"}),
          "Main menu order no longer matches the v1 shell");

  gui::NavigationWidget navigation(nullptr);
  require(topLevelItems(navigation) ==
              QStringList({"Now Playing", "Anime List", "History", "Statistics", "Search",
                           "Seasons", "Torrents"}),
          "Sidebar page list no longer matches the v1 shell");

  for (const auto& icon : {
           "account_circle", "arrow_back", "arrow_forward", "bar_chart", "cloud_download",
           "create_new_folder", "delete", "edit", "empty", "export_notes", "favorite",
           "folder", "grid_view", "help", "history", "home", "info", "list_alt", "lists",
           "logout", "menu", "more_horiz", "open_in_new", "pageview", "play_arrow",
           "rss_feed", "search", "settings", "share", "shuffle", "skip_next", "sort",
           "sync",
       }) {
    const auto path = QString(":/icons/%1.svg").arg(icon);
    const auto message = QString("Bundled UI icon missing: %1").arg(path).toStdString();
    require(QFile::exists(path), message.c_str());
  }

  for (const auto& icon : {
           "16px/calendar-month", "16px/category", "16px/chart", "16px/clock",
           "16px/cross", "16px/document-attribute", "16px/document-export",
           "16px/document-import", "16px/feed", "16px/film", "16px/magnifier-left",
           "16px/navigation-270-button", "16px/sort-quantity-descending",
           "16px/square-small-blue", "16px/square-small-gray", "16px/square-small-green",
           "16px/square-small-red", "16px/ui-scroll-pane-detail",
           "24px/application-export", "24px/application-sidebar-list",
           "24px/arrow-circle-double-135", "24px/feed", "24px/folder-open", "24px/gear",
           "24px/globe", "24px/inbox-document", "24px/megaphone",
       }) {
    const auto path = QString(":/icons/classic/%1.png").arg(icon);
    const auto message = QString("Bundled classic UI icon missing: %1").arg(path).toStdString();
    require(QFile::exists(path), message.c_str());
  }

  gui::AnimeListModel animeListModel(nullptr);
  require(animeListModel.columnCount() == gui::AnimeListModel::NUM_COLUMNS,
          "Anime list model column count is inconsistent");
  require(animeListModel.headerData(gui::AnimeListModel::COLUMN_STATUS, Qt::Horizontal,
                                    Qt::DisplayRole)
              .toString()
              .isEmpty(),
          "Anime list v1 status icon column should have a blank header");
  require(animeListModel.headerData(gui::AnimeListModel::COLUMN_TITLE, Qt::Horizontal,
                                    Qt::DisplayRole)
              .toString() == "Title",
          "Anime list title column shifted away from v1 order");

  gui::TorrentsWidget torrents;
  const auto* torrentTable = torrents.findChild<QTableWidget*>();
  const auto* torrentToolbar = torrents.findChild<QToolBar*>();
  require(torrents.findChild<QLineEdit*>() == nullptr,
          "Torrent view should use the shared shell search box, not a local search box");
  require(torrentToolbar != nullptr, "Torrent toolbar missing");
  require(torrentToolbar->actions().size() >= 6, "Torrent toolbar lost v1 action/separator shape");
  require(torrentTable != nullptr && torrentTable->columnCount() == 11,
          "Torrent table lost the v1 column inventory");
  require(!torrentTable->showGrid(), "Torrent table should render as a v1 row list, not a grid");
  require(!torrentTable->verticalHeader()->isVisible(),
          "Torrent table should not show row numbers");
  require(torrentTable->alternatingRowColors(), "Torrent table should keep v1-style row bands");
  require(torrentTable->horizontalHeaderItem(0)->text() == "Anime title",
          "Torrent first column should remain Anime title");
  require(torrentTable->horizontalHeaderItem(10)->text() == "Release date",
          "Torrent release-date column missing");

  gui::NowPlayingPageWidget nowPlaying;
  nowPlaying.show();
  app.processEvents();
  bool staleHeaderVisible = false;
  for (const auto* label : nowPlaying.findChildren<QLabel*>()) {
    if ((label->text() == "Alternative titles" || label->text() == "Details" ||
         label->text() == "Synopsis") &&
        label->isVisible()) {
      staleHeaderVisible = true;
    }
  }
  require(!staleHeaderVisible, "Now Playing idle state should not show orphan detail headers");

  return EXIT_SUCCESS;
}
