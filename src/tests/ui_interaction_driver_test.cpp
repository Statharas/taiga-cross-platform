#include <QLabel>
#include <QLineEdit>
#include <QStatusBar>
#include <QTest>

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

  window.navigateTo(gui::MainWindowPage::Search);
  app.processEvents();
  require(searchBox->placeholderText() == "Search MyAnimeList for anime",
          "Search page did not switch shared search box to service search mode");
  searchBox->setFocus();
  searchBox->clear();
  QTest::keyClicks(searchBox, "Nisekoi");
  QMetaObject::invokeMethod(searchBox, "returnPressed", Qt::DirectConnection);
  app.processEvents();
  require(window.statusBar()->currentMessage() == "MyAnimeList search is not implemented yet.",
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
