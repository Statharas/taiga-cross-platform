#include <QLabel>
#include <QLineEdit>
#include <QAction>
#include <QStatusBar>
#include <QStackedWidget>
#include <QTreeView>
#include <QHeaderView>
#include <QTest>
#include <QTemporaryDir>

#include <cstdlib>
#include <iostream>

#include "gui/main/main_window.hpp"
#include "gui/torrents/torrents_widget.hpp"
#include "gui/utils/theme.hpp"
#include "taiga/application.hpp"
#include "taiga/settings.hpp"

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(EXIT_FAILURE);
  }
}

bool hasLabelText(const QWidget& widget, const QString& text) {
  for (const auto* label : widget.findChildren<QLabel*>()) {
    if (label->text() == text) return true;
  }
  return false;
}

}  // namespace

int main(int argc, char* argv[]) {
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QTemporaryDir dataDir;
  require(dataDir.isValid(), "Could not create temporary data directory");
  qputenv("TAIGA_DATA_PATH", dataDir.path().toUtf8());

  taiga::Application app(argc, argv);
  QCoreApplication::setApplicationName("taiga-ui-interaction-driver-test");
  QCoreApplication::setOrganizationName("taiga");

  taiga::settings.setService("myanimelist");
  taiga::settings.setStringValue("rss.torrent.search", "%title%");
  gui::theme.initStyle();

  gui::MainWindow window;
  app.setMainWindowForTest(&window);
  window.init();
  window.show();
  app.processEvents();

  auto* searchBox = window.searchBox();
  require(searchBox != nullptr, "Main search box missing");
  auto* stack = window.findChild<QStackedWidget*>();
  auto* backAction = window.findChild<QAction*>("actionBack");
  auto* forwardAction = window.findChild<QAction*>("actionForward");
  require(stack != nullptr && backAction != nullptr && forwardAction != nullptr,
          "Page history controls missing");

  window.navigateTo(gui::MainWindowPage::Search);
  app.processEvents();
  window.navigateTo(gui::MainWindowPage::List);
  app.processEvents();
  require(backAction->isEnabled(), "Back action did not enable after navigation");
  backAction->trigger();
  app.processEvents();
  require(stack->currentIndex() == static_cast<int>(gui::MainWindowPage::Search),
          "Back action did not restore previous page");
  require(forwardAction->isEnabled(), "Forward action did not enable after going back");
  forwardAction->trigger();
  app.processEvents();
  require(stack->currentIndex() == static_cast<int>(gui::MainWindowPage::List),
          "Forward action did not restore next page");
  auto* animeList = window.findChild<QTreeView*>("animeList");
  require(animeList != nullptr && animeList->header()->contextMenuPolicy() == Qt::CustomContextMenu,
          "Anime list header should expose the v1 column context menu");
  window.navigateTo(gui::MainWindowPage::Search);
  app.processEvents();
  require(searchBox->placeholderText() == "Search MyAnimeList for anime",
          "Search page did not switch shared search box to service search mode");
  searchBox->setFocus();
  searchBox->clear();
  QTest::keyClicks(searchBox, "Nisekoi");
  QMetaObject::invokeMethod(searchBox, "returnPressed", Qt::DirectConnection);
  app.processEvents();
  require(window.statusBar()->currentMessage() == "MyAnimeList access token is not configured.",
          "Enter on Search page did not submit through service search mode");

  window.navigateTo(gui::MainWindowPage::Torrents);
  app.processEvents();
  auto* torrents = window.findChild<gui::TorrentsWidget*>();
  require(torrents != nullptr, "Torrents page was not created");
  require(torrents->findChild<QLineEdit*>() == nullptr,
          "Torrents page should not have a local search field");
  require(searchBox->placeholderText() == "Search for torrents",
          "Torrents page did not switch shared search box to feed search mode");
  searchBox->setFocus();
  searchBox->clear();
  require(torrents->filterText().isEmpty(), "Torrent filter should start empty in feed-search mode");
  QTest::keyClicks(searchBox, "Nisekoi");
  app.processEvents();
  require(torrents->filterText().isEmpty(),
          "Typing in torrent search should not live-filter the current torrent table");
  QMetaObject::invokeMethod(searchBox, "returnPressed", Qt::DirectConnection);
  app.processEvents();
  require(hasLabelText(*torrents, "Torrent feed URL is invalid."),
          "Enter on Torrents page did not submit through torrent feed search mode");

  return EXIT_SUCCESS;
}
