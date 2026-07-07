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

#include "main_window.hpp"

#include <QDesktopServices>
#include <QFileDialog>
#include <QtWidgets>

#include "base/string.hpp"
#include "gui/history/history_widget.hpp"
#include "gui/library/library_widget.hpp"
#include "gui/list/list_widget.hpp"
#include "gui/main/about_dialog.hpp"
#include "gui/main/navigation_widget.hpp"
#include "gui/main/now_playing_page_widget.hpp"
#include "gui/main/now_playing_widget.hpp"
#include "gui/search/search_widget.hpp"
#include "gui/seasons/seasons_widget.hpp"
#include "gui/settings/settings_dialog.hpp"
#include "gui/statistics/statistics_widget.hpp"
#include "gui/torrents/torrents_widget.hpp"
#include "gui/utils/theme.hpp"
#include "gui/utils/tray_icon.hpp"
#include "gui/utils/widgets.hpp"
#include "media/anime_list_export.hpp"
#include "sync/service.hpp"
#include "taiga/application.hpp"
#include "taiga/session.hpp"
#include "taiga/settings.hpp"
#include "track/play.hpp"
#include "ui_main_window.h"

#ifdef Q_OS_WINDOWS
#include "gui/platforms/windows.hpp"
#endif

namespace gui {

MainWindow::MainWindow() : QMainWindow(), ui_(new Ui::MainWindow) {
  ui_->setupUi(this);

  ui_->menuList->setTitle(tr("&Services"));
  ui_->menuLibrary->menuAction()->setVisible(false);
  ui_->menubar->insertMenu(ui_->menuHelp->menuAction(), [this]() {
    auto* menu = new QMenu(tr("&View"), ui_->menubar);
    menu->addAction(ui_->actionToggleNowPlaying);
    menu->addAction(ui_->actionToggleStatusbar);
    return menu;
  }());

#ifdef Q_OS_WINDOWS
  enableMicaBackground(this);
#endif

  if (const auto geometry = taiga::session.mainWindowGeometry(); !geometry.isEmpty()) {
    restoreGeometry(geometry);
    centerWidgetToScreen(this);
  }

  ui_->stackedWidget->insertWidget(static_cast<int>(MainWindowPage::Statistics),
                                   new QWidget(ui_->stackedWidget));
  ui_->stackedWidget->insertWidget(static_cast<int>(MainWindowPage::Seasons),
                                   new QWidget(ui_->stackedWidget));

  // Do not call `init()` here, as it relies on the main window pointer being
  // available through the application instance.
}

MainWindow* mainWindow() {
  return taiga::app()->mainWindow();
}

NavigationWidget* MainWindow::navigation() const {
  return m_navigationWidget;
}

NowPlayingWidget* MainWindow::nowPlaying() const {
  return m_nowPlayingWidget;
}

QLineEdit* MainWindow::searchBox() const {
  return m_searchBox;
}

Ui::MainWindow* MainWindow::ui() const {
  return ui_;
}

void MainWindow::init() {
  initActions();
  initIcons();
  initTrayIcon();
  initToolbar();
  initNavigation();
  initStatusbar();
  initNowPlaying();
  updateTitle();
}

void MainWindow::initActions() {
  ui_->actionProfile->setToolTip(tr("Profile"));
  ui_->actionSynchronize->setToolTip(
      tr("Synchronize with %1").arg(taiga_sync::serviceName(taiga_sync::currentServiceId())));
  ui_->actionToggleDetection->setChecked(
      taiga::settings.boolValue("program.option.enableRecognition", true));
  ui_->actionToggleSharing->setChecked(
      taiga::settings.boolValue("program.option.enableSharing", true));
  ui_->actionToggleSynchronization->setChecked(
      taiga::settings.boolValue("program.option.enableSync", true));

  connect(ui_->actionAddNewFolder, &QAction::triggered, this, &MainWindow::addNewFolder);
  connect(ui_->actionExit, &QAction::triggered, this, &QApplication::quit, Qt::QueuedConnection);
  connect(ui_->actionSettings, &QAction::triggered, this, [this]() { SettingsDialog::show(this); });
  connect(ui_->actionAbout, &QAction::triggered, this, &MainWindow::about);
  connect(ui_->actionDonate, &QAction::triggered, this, &MainWindow::donate);
  connect(ui_->actionExportListAsMarkdown, &QAction::triggered, this,
          &MainWindow::exportListAsMarkdown);
  connect(ui_->actionExportListAsMyAnimeListXML, &QAction::triggered, this,
          &MainWindow::exportListAsXml);
  connect(ui_->actionLibraryFolders, &QAction::triggered, this, [this]() {
    SettingsDialog::show(this);
  });
  connect(ui_->actionPlayNextEpisode, &QAction::triggered, this, &MainWindow::playNextEpisode);
  connect(ui_->actionPlayRandomAnime, &QAction::triggered, this, &MainWindow::playRandomAnime);
  connect(ui_->actionScanAvailableEpisodes, &QAction::triggered, this,
          &MainWindow::scanAvailableEpisodes);
  connect(ui_->actionSupport, &QAction::triggered, this, &MainWindow::support);
  connect(ui_->actionProfile, &QAction::triggered, this, &MainWindow::profile);
  connect(ui_->actionDisplayWindow, &QAction::triggered, this, &MainWindow::displayWindow);
  connect(ui_->actionToggleStatusbar, &QAction::toggled, ui_->statusbar, &QWidget::setVisible);
  connect(ui_->actionToggleNowPlaying, &QAction::toggled, this, [this](bool visible) {
    if (m_nowPlayingWidget) m_nowPlayingWidget->setVisible(visible);
  });
  connect(ui_->actionToggleDetection, &QAction::toggled, this, [this](bool enabled) {
    taiga::settings.setBoolValue("program.option.enableRecognition", enabled);
    statusBar()->showMessage(enabled ? tr("Automatic anime recognition is now enabled.")
                                     : tr("Automatic anime recognition is now disabled."),
                             5000);
  });
  connect(ui_->actionToggleSharing, &QAction::toggled, this, [this](bool enabled) {
    taiga::settings.setBoolValue("program.option.enableSharing", enabled);
    statusBar()->showMessage(enabled ? tr("Automatic sharing is now enabled.")
                                     : tr("Automatic sharing is now disabled."),
                             5000);
  });
  connect(ui_->actionToggleSynchronization, &QAction::toggled, this, [this](bool enabled) {
    taiga::settings.setBoolValue("program.option.enableSync", enabled);
    statusBar()->showMessage(enabled ? tr("Automatic synchronization is now enabled.")
                                     : tr("Automatic synchronization is now disabled."),
                             5000);
  });

  connect(ui_->actionSynchronize, &QAction::triggered, this, [this]() {
    ui_->actionSynchronize->setEnabled(false);
    setEnabled(false);
    statusBar()->showMessage(
        tr("Synchronizing with %1...").arg(taiga_sync::serviceName(taiga_sync::currentServiceId())));
    taiga_sync::synchronize([this](bool ok, const QString& message) {
      ui_->actionSynchronize->setEnabled(true);
      setEnabled(true);
      navigation()->refresh();
      if (ok) {
        statusBar()->showMessage(message.isEmpty() ? tr("Synchronization complete.") : message,
                                 5000);
      } else {
        statusBar()->showMessage(message.isEmpty() ? tr("Synchronization failed.") : message,
                                 8000);
      }
    });
  });
}

void MainWindow::initIcons() {
  ui_->menuLibraryFolders->setIcon(theme.getIcon("classic/24px/folder-open", "png", false));
  ui_->menuExport->setIcon(theme.getIcon("classic/24px/application-export", "png", false));

  ui_->actionAddNewFolder->setIcon(theme.getIcon("create_new_folder"));
  ui_->actionAbout->setIcon(theme.getIcon("info"));
  ui_->actionBack->setIcon(theme.getIcon("arrow_back"));
  ui_->actionCheckForUpdates->setIcon(theme.getIcon("cloud_download"));
  ui_->actionDonate->setIcon(theme.getIcon("favorite"));
  ui_->actionExit->setIcon(theme.getIcon("logout"));
  ui_->actionExportListAsMarkdown->setIcon(theme.getIcon("export_notes"));
  ui_->actionExportListAsMyAnimeListXML->setIcon(theme.getIcon("export_notes"));
  ui_->actionForward->setIcon(theme.getIcon("arrow_forward"));
  ui_->actionLibraryFolders->setIcon(theme.getIcon("classic/24px/folder-open", "png", false));
  ui_->actionMenu->setIcon(theme.getIcon("menu"));
  ui_->actionPlayNextEpisode->setIcon(theme.getIcon("skip_next"));
  ui_->actionPlayRandomAnime->setIcon(theme.getIcon("shuffle"));
  ui_->actionProfile->setIcon(theme.getIcon("account_circle"));
  ui_->actionScanAvailableEpisodes->setIcon(theme.getIcon("pageview"));
  ui_->actionSettings->setIcon(theme.getIcon("classic/24px/gear", "png", false));
  ui_->actionSupport->setIcon(theme.getIcon("help"));
  ui_->actionSynchronize->setIcon(
      theme.getIcon("classic/24px/arrow-circle-double-135", "png", false));
}

void MainWindow::initNavigation() {
  m_navigationWidget = new NavigationWidget(this);

  connect(m_navigationWidget, &NavigationWidget::currentPageChanged, this, &MainWindow::setPage);

  navigateTo(MainWindowPage::List);

  ui_->splitter->insertWidget(0, m_navigationWidget);
}

void MainWindow::initNowPlaying() {
  m_nowPlayingWidget = new NowPlayingWidget(ui_->centralWidget);

  ui_->centralWidget->layout()->addWidget(m_nowPlayingWidget);
  m_nowPlayingWidget->hide();
}

void MainWindow::initPage(MainWindowPage page) {
  static QSet<MainWindowPage> initializedPages;

  if (initializedPages.contains(page)) return;

  static const auto init_page = [](QWidget* page, QWidget* widget) {
    if (auto* oldLayout = page->layout()) {
      while (auto* item = oldLayout->takeAt(0)) {
        delete item->widget();
        delete item;
      }
      delete oldLayout;
    }
    const auto layout = new QHBoxLayout();
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(widget);
    page->setLayout(layout);
  };

  switch (page) {
    case MainWindowPage::Home:
      m_nowPlayingPageWidget = new NowPlayingPageWidget(ui_->homePage);
      init_page(ui_->homePage, m_nowPlayingPageWidget);
      break;

    case MainWindowPage::Search:
      m_searchWidget = new SearchWidget(ui_->searchPage);
      init_page(ui_->searchPage, m_searchWidget);
      break;

    case MainWindowPage::List:
      m_listWidget = new ListWidget(ui_->listPage);
      init_page(ui_->listPage, m_listWidget);
      break;

    case MainWindowPage::History:
      m_historyWidget = new HistoryWidget(ui_->historyPage);
      init_page(ui_->historyPage, m_historyWidget);
      break;

    case MainWindowPage::Statistics:
      m_statisticsWidget =
          new StatisticsWidget(ui_->stackedWidget->widget(static_cast<int>(MainWindowPage::Statistics)));
      init_page(ui_->stackedWidget->widget(static_cast<int>(MainWindowPage::Statistics)),
                m_statisticsWidget);
      break;

    case MainWindowPage::Library:
      m_libraryWidget = new LibraryWidget(ui_->libraryPage);
      init_page(ui_->libraryPage, m_libraryWidget);
      break;

    case MainWindowPage::Seasons:
      m_seasonsWidget =
          new SeasonsWidget(ui_->stackedWidget->widget(static_cast<int>(MainWindowPage::Seasons)));
      init_page(ui_->stackedWidget->widget(static_cast<int>(MainWindowPage::Seasons)),
                m_seasonsWidget);
      break;

    case MainWindowPage::Torrents:
      m_torrentsWidget = new TorrentsWidget(ui_->torrentsPage);
      init_page(ui_->torrentsPage, m_torrentsWidget);
      break;

    case MainWindowPage::Profile:
      break;
  }

  initializedPages.insert(page);
}

void MainWindow::initStatusbar() {
  ui_->statusbar->setContentsMargins(0, 8, 0, 0);
}

void MainWindow::initToolbar() {
  ui_->toolbar->setIconSize(QSize{20, 20});
  ui_->toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
  ui_->toolbar->removeAction(ui_->actionMenu);
  ui_->toolbar->removeAction(ui_->actionProfile);
  ui_->toolbar->insertAction(ui_->actionSettings, ui_->actionAddNewFolder);
  ui_->toolbar->insertAction(ui_->actionSettings, ui_->actionExportListAsMyAnimeListXML);

  // Search box
  {
    m_searchBox = new QLineEdit();
    m_searchBox->setClearButtonEnabled(true);
    m_searchBox->setFixedWidth(250);
    m_searchBox->setPlaceholderText(tr("Filter list or search MyAnimeList"));
    connect(m_searchBox, &QLineEdit::returnPressed, this, &MainWindow::submitSearchBox);

    auto* spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    ui_->toolbar->addWidget(spacer);
    ui_->toolbar->addWidget(m_searchBox);
  }
}

void MainWindow::initTrayIcon() {
  auto menu = new QMenu(this);
  menu->addAction(ui_->actionDisplayWindow);
  menu->setDefaultAction(ui_->actionDisplayWindow);
  menu->addSeparator();
  menu->addAction(ui_->actionSettings);
  menu->addSeparator();
  menu->addAction(ui_->actionExit);

  m_trayIcon = new TrayIcon(this, windowIcon(), menu);

  connect(m_trayIcon, &TrayIcon::activated, this, &MainWindow::displayWindow);
  connect(m_trayIcon, &TrayIcon::messageClicked, this,
          []() { QMessageBox::information(nullptr, "Taiga", tr("Clicked message")); });
}

void MainWindow::closeEvent(QCloseEvent* event) {
  taiga::session.setMainWindowGeometry(saveGeometry());
  if (m_listWidget) m_listWidget->saveState();
  if (m_searchWidget) m_searchWidget->saveState();
  event->accept();
}

void MainWindow::addNewFolder() {
  constexpr auto options =
      QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks | QFileDialog::ReadOnly;

  const auto directory = QFileDialog::getExistingDirectory(this, tr("Add New Folder"), "", options);

  if (!directory.isEmpty()) {
    QMessageBox::information(this, "New Folder", directory);
  }
}

void MainWindow::navigateTo(MainWindowPage page) {
  if (const auto item = m_navigationWidget->findItemByPage(page)) {
    m_navigationWidget->setCurrentItem(item);
  }
}

void MainWindow::setPage(MainWindowPage page) {
  initPage(page);
  ui_->statusbar->clearMessage();
  ui_->stackedWidget->setCurrentIndex(static_cast<int>(page));
  updateSearchBoxForPage(page);
}

void MainWindow::submitSearchBox() {
  if (!m_searchBox) return;

  const auto text = m_searchBox->text().trimmed();
  if (text.isEmpty()) return;

  const auto page = static_cast<MainWindowPage>(ui_->stackedWidget->currentIndex());
  if (page == MainWindowPage::Torrents) {
    if (m_torrentsWidget) m_torrentsWidget->submitSearch(text);
    return;
  }

  if (page != MainWindowPage::Search) {
    navigateTo(MainWindowPage::Search);
  }

  const auto service = taiga_sync::serviceName(taiga_sync::currentServiceId());
  statusBar()->showMessage(tr("%1: Searching for \"%2\"...").arg(service, text));
  taiga_sync::searchTitle(text, [this](bool ok, const QString& message) {
    statusBar()->showMessage(message.isEmpty() ? (ok ? tr("Search complete.") : tr("Search failed."))
                                               : message,
                             ok ? 5000 : 8000);
  });
}

void MainWindow::updateTitle() {
  auto title = u"Taiga"_s;

  if (taiga::app()->isDebug()) {
    title += u" [debug]"_s;
  }

  setWindowTitle(title);
}

void MainWindow::updateSearchBoxForPage(MainWindowPage page) {
  if (!m_searchBox) return;

  if (m_pageSearchConnection) {
    disconnect(m_pageSearchConnection);
    m_pageSearchConnection = {};
  }

  const auto service = taiga_sync::serviceName(taiga_sync::currentServiceId());
  switch (page) {
    case MainWindowPage::List:
      m_searchBox->setPlaceholderText(tr("Filter list or search %1").arg(service));
      break;
    case MainWindowPage::Torrents:
      m_searchBox->setPlaceholderText(tr("Search for torrents"));
      if (m_torrentsWidget) m_torrentsWidget->setFilterText({});
      break;
    case MainWindowPage::Home:
    case MainWindowPage::Search:
    case MainWindowPage::Seasons:
      m_searchBox->setPlaceholderText(tr("Search %1 for anime").arg(service));
      break;
    case MainWindowPage::History:
    case MainWindowPage::Statistics:
    case MainWindowPage::Library:
    case MainWindowPage::Profile:
      m_searchBox->setPlaceholderText(tr("Search"));
      break;
  }
}

void MainWindow::displayWindow() {
  setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
  activateWindow();
}

void MainWindow::about() {
  displayAboutDialog(this);
}

void MainWindow::donate() const {
  QDesktopServices::openUrl(QUrl("https://taiga.moe/#donate"));
}

void MainWindow::exportList(const QString& extension, bool (*exportFunction)(const std::string&)) {
  const auto directory = QFileDialog::getExistingDirectory(
      this, tr("Select Export Location"), {},
      QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks | QFileDialog::ReadOnly);
  if (directory.isEmpty()) return;

  const auto timestamp = QDateTime::currentDateTime().toSecsSinceEpoch();
  const auto path = u"%1/animelist_%2.%3"_s.arg(directory).arg(timestamp).arg(extension);
  if (exportFunction(path.toStdString())) {
    statusBar()->showMessage(tr("Exported list to: %1").arg(path), 5000);
  } else {
    statusBar()->showMessage(tr("Could not export list to: %1").arg(path), 8000);
  }
}

void MainWindow::exportListAsMarkdown() {
  exportList("md", &anime::list::exportAsMarkdown);
}

void MainWindow::exportListAsXml() {
  exportList("xml", &anime::list::exportAsXml);
}

void MainWindow::playNextEpisode() {
  if (!track::playNextEpisodeOfLastWatchedAnime()) {
    statusBar()->showMessage(tr("Could not find the next episode."), 5000);
  }
}

void MainWindow::playRandomAnime() {
  if (!track::playRandomAnime()) {
    statusBar()->showMessage(tr("Could not find an episode to play."), 5000);
  }
}

void MainWindow::scanAvailableEpisodes() {
  statusBar()->showMessage(tr("Scanning available episodes is not implemented yet."), 5000);
}

void MainWindow::support() const {
  QDesktopServices::openUrl(QUrl("https://taiga.moe/#support"));
}

void MainWindow::profile() {
  setPage(MainWindowPage::Profile);
  m_navigationWidget->setCurrentIndex({});
}

}  // namespace gui
