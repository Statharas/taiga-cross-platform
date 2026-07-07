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

#include "settings_dialog.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QDirIterator>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QMimeData>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <QXmlStreamReader>

#include <anisthesia.hpp>

#include <utility>

#include "base/file.hpp"
#include "base/string.hpp"
#include "gui/utils/theme.hpp"
#include "media/anime_history.hpp"
#include "sync/anilist_utils.hpp"
#include "sync/myanimelist.hpp"
#include "sync/myanimelist_utils.hpp"
#include "sync/service.hpp"
#include "taiga/accounts.hpp"
#include "taiga/network.hpp"
#include "taiga/path.hpp"
#include "taiga/settings.hpp"
#include "ui_settings_dialog.h"

#ifdef Q_OS_WINDOWS
#include "gui/platforms/windows.hpp"
#endif

namespace {

QStringList supportedMediaPlayers() {
  std::vector<anisthesia::Player> players;
  const auto file = base::readFile(":/players.anisthesia");
  anisthesia::ParsePlayersData(file.toStdString(), players);

  QStringList names;
  for (const auto& player : players) {
    if (player.type != anisthesia::PlayerType::WebBrowser) {
      names.push_back(QString::fromStdString(player.name));
    }
  }
  names.removeDuplicates();
  names.sort(Qt::CaseInsensitive);
  return names;
}

QString slug(QString value) {
  value = value.toLower();
  value.replace(QRegularExpression("[^a-z0-9]+"), ".");
  value.replace(QRegularExpression("^\\.|\\.$"), "");
  return value;
}

struct DirectoryStats {
  qsizetype count = 0;
  qint64 size = 0;
};

DirectoryStats directoryStats(const QString& path, const QStringList& nameFilters = {}) {
  DirectoryStats stats;
  if (!QDir(path).exists()) return stats;

  QDirIterator it(path, nameFilters, QDir::Files, QDirIterator::Subdirectories);
  while (it.hasNext()) {
    it.next();
    const QFileInfo info = it.fileInfo();
    ++stats.count;
    stats.size += info.size();
  }
  return stats;
}

QString formatSize(qint64 bytes) {
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

int removeFiles(const QString& path, const QStringList& nameFilters = {}) {
  int removed = 0;
  QDirIterator it(path, nameFilters, QDir::Files, QDirIterator::Subdirectories);
  while (it.hasNext()) {
    if (QFile::remove(it.next())) {
      ++removed;
    }
  }
  return removed;
}

QString posterCachePath() {
  return u"%1/v1/db/image"_s.arg(QString::fromStdString(taiga::get_data_path()));
}

QString torrentCachePath() {
  return u"%1/v1/rss"_s.arg(QString::fromStdString(taiga::get_data_path()));
}

QString torrentArchivePath() {
  return torrentCachePath() + "/archive.xml";
}

int torrentArchiveCount() {
  QFile file{torrentArchivePath()};
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return 0;

  QXmlStreamReader xml{&file};
  int count = 0;
  while (!xml.atEnd()) {
    xml.readNext();
    if (xml.isStartElement() && (xml.name() == u"item" || xml.name() == u"torrent")) {
      ++count;
    }
  }
  return count;
}

QPushButton* addActionButton(QWidget* page, const QString& text) {
  auto* button = new QPushButton(text, page);
  auto* layout = qobject_cast<QVBoxLayout*>(page->layout());
  layout->insertWidget(layout->count() - 1, button, 0, Qt::AlignLeft);
  return button;
}

QByteArray formData(const QList<QPair<QString, QString>>& values) {
  QUrlQuery query;
  for (const auto& [key, value] : values) {
    query.addQueryItem(key, value);
  }
  return query.toString(QUrl::FullyEncoded).toUtf8();
}

QString queryValueFromInput(const QString& input, const QString& key) {
  const auto trimmed = input.trimmed();
  if (trimmed.isEmpty()) return {};

  const QUrl url{trimmed};
  if (url.isValid()) {
    const auto queryValue = QUrlQuery(url.query()).queryItemValue(key);
    if (!queryValue.isEmpty()) return queryValue;
    const auto fragmentValue = QUrlQuery(url.fragment()).queryItemValue(key);
    if (!fragmentValue.isEmpty()) return fragmentValue;
  }

  const QUrlQuery query{trimmed};
  const auto queryValue = query.queryItemValue(key);
  if (!queryValue.isEmpty()) return queryValue;

  const QRegularExpression expression{uR"((?:^|[?#&])%1=([^&#\s]+))"_s.arg(key)};
  const auto match = expression.match(trimmed);
  if (match.hasMatch()) {
    return QUrl::fromPercentEncoding(match.captured(1).toUtf8());
  }
  return {};
}

QJsonObject replyObject(QNetworkReply* reply, QString* errorMessage) {
  const auto payload = reply->readAll();
  const auto document = QJsonDocument::fromJson(payload);
  if (!document.isObject()) {
    if (errorMessage) *errorMessage = QObject::tr("The service returned an unreadable response.");
    return {};
  }

  const auto object = document.object();
  const auto error = object.value("error").toString();
  if (!error.isEmpty()) {
    const auto description = object.value("error_description").toString();
    if (errorMessage) {
      *errorMessage = description.isEmpty() ? error : u"%1: %2"_s.arg(error, description);
    }
    return {};
  }
  return object;
}

QJsonArray defaultTorrentFilters() {
  return {
      QJsonObject{{"name", "Discard batches"}, {"enabled", true}},
      QJsonObject{{"name", "Prefer fansub groups"}, {"enabled", true}},
      QJsonObject{{"name", "Prefer best resolution"}, {"enabled", true}},
      QJsonObject{{"name", "Ignore unknown episodes"}, {"enabled", true}},
  };
}

QJsonArray torrentFilters() {
  const auto text = taiga::settings.stringValue("rss.torrent.filters.itemsJson");
  const auto document = QJsonDocument::fromJson(text.toUtf8());
  return document.isArray() ? document.array() : defaultTorrentFilters();
}

void addTorrentFilterItem(QListWidget* list, const QString& name, bool enabled) {
  auto* item = new QListWidgetItem(name, list);
  item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEditable);
  item->setCheckState(enabled ? Qt::Checked : Qt::Unchecked);
}

void loadTorrentFilters(QListWidget* list, const QJsonArray& filters) {
  list->clear();
  for (const auto& value : filters) {
    const auto object = value.toObject();
    const auto name = object.value("name").toString();
    if (!name.isEmpty()) {
      addTorrentFilterItem(list, name, object.value("enabled").toBool(true));
    }
  }
}

void saveTorrentFilters(QListWidget* list) {
  QJsonArray filters;
  for (int i = 0; i < list->count(); ++i) {
    const auto* item = list->item(i);
    filters.push_back(QJsonObject{
        {"name", item->text()},
        {"enabled", item->checkState() == Qt::Checked},
    });
  }
  taiga::settings.setStringValue("rss.torrent.filters.itemsJson",
                                 QJsonDocument(filters).toJson(QJsonDocument::Compact));
}

}  // namespace

namespace gui {

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent), ui_(new Ui::SettingsDialog) {
  ui_->setupUi(this);

#ifdef Q_OS_WINDOWS
  enableMicaBackground(this);
#endif

  setMinimumSize(920, 640);
  ui_->treeWidget->setIndentation(22);
  while (ui_->stackedWidget->count() > 0) {
    delete ui_->stackedWidget->widget(0);
  }

  const auto add_item = [this](QString icon, QString text, int page) {
    auto item = new QTreeWidgetItem(ui_->treeWidget, QStringList(text));
    item->setIcon(0, icon.startsWith("classic/") ? theme.getIcon(icon, "png", false)
                                                 : theme.getIcon(icon));
    item->setSizeHint(0, QSize{0, 24});
    item->setData(0, Qt::UserRole, page);
    return item;
  };

  const auto add_child = [](QTreeWidgetItem* parent, QString text, int page) {
    auto item = new QTreeWidgetItem(parent, QStringList(text));
    item->setData(0, Qt::UserRole, page);
    return item;
  };

  const int servicesMain = addSettingsPage(createFormPage());
  {
    auto* page = settingsPage(servicesMain);
    serviceCombo_ = addComboBox(page, tr("Service"), "v1.service",
                                {{taiga_sync::serviceName(taiga_sync::ServiceId::AniList),
                                  taiga_sync::serviceSlug(taiga_sync::ServiceId::AniList)},
                                 {taiga_sync::serviceName(taiga_sync::ServiceId::MyAnimeList),
                                  taiga_sync::serviceSlug(taiga_sync::ServiceId::MyAnimeList)},
                                 {taiga_sync::serviceName(taiga_sync::ServiceId::Kitsu),
                                  taiga_sync::serviceSlug(taiga_sync::ServiceId::Kitsu)}},
                                taiga_sync::serviceSlug(taiga_sync::ServiceId::AniList));
    addCheckBox(page, tr("Log in on startup"), "account.sync.autoLogin", false);
  }

  const int servicesMal = addSettingsPage(createFormPage());
  {
    auto* page = settingsPage(servicesMal);
    auto* usernameEdit = addLineEdit(
        page, tr("Username"), QString::fromStdString(taiga::accounts.myanimelistUsername()),
        [](const QString& value) { taiga::accounts.setMyanimelistUsername(value.toStdString()); });
    auto* accessTokenEdit = addLineEdit(
        page, tr("Access token"),
        QString::fromStdString(taiga::accounts.myanimelistAccessToken()),
        [](const QString& value) { taiga::accounts.setMyanimelistAccessToken(value.toStdString()); },
        QLineEdit::Password);
    auto* refreshTokenEdit = addLineEdit(
        page, tr("Refresh token"),
        QString::fromStdString(taiga::accounts.myanimelistRefreshToken()),
        [](const QString& value) { taiga::accounts.setMyanimelistRefreshToken(value.toStdString()); },
        QLineEdit::Password);
    auto* authButton = addActionButton(page, tr("Authorize"));
    connect(authButton, &QPushButton::clicked, this,
            [this, usernameEdit, accessTokenEdit, refreshTokenEdit]() {
              authorizeMyAnimeList(usernameEdit, accessTokenEdit, refreshTokenEdit);
            });
  }

  const int servicesKitsu = addSettingsPage(createFormPage());
  {
    auto* page = settingsPage(servicesKitsu);
    auto* emailEdit = addLineEdit(
        page, tr("Email"), QString::fromStdString(taiga::accounts.kitsuEmail()),
        [](const QString& value) { taiga::accounts.setKitsuEmail(value.toStdString()); });
    auto* passwordEdit = addLineEdit(
        page, tr("Password"), QString::fromStdString(taiga::accounts.kitsuPassword()),
        [](const QString& value) { taiga::accounts.setKitsuPassword(value.toStdString()); },
        QLineEdit::Password);
    auto* displayNameEdit = addLineEdit(page, tr("Display name"), "account.kitsu.displayName");
    auto* usernameEdit = addLineEdit(
        page, tr("Username"), QString::fromStdString(taiga::accounts.kitsuUsername()),
        [](const QString& value) { taiga::accounts.setKitsuUsername(value.toStdString()); });
    addComboBox(page, tr("Rating system"), "account.kitsu.ratingSystem",
                {{"Regular", "regular"}, {"Simple", "simple"}, {"Advanced", "advanced"}}, "regular");
    addCheckBox(page, tr("Use partial library sync"), "account.kitsu.partialLibrary", true);
    auto* loginButton = addActionButton(page, tr("Log in"));
    connect(loginButton, &QPushButton::clicked, this,
            [this, emailEdit, passwordEdit, usernameEdit, displayNameEdit]() {
              authorizeKitsu(emailEdit, passwordEdit, usernameEdit, displayNameEdit);
            });
  }

  const int servicesAniList = addSettingsPage(createFormPage());
  {
    auto* page = settingsPage(servicesAniList);
    auto* usernameEdit = addLineEdit(
        page, tr("Username"), QString::fromStdString(taiga::accounts.anilistUsername()),
        [](const QString& value) { taiga::accounts.setAnilistUsername(value.toStdString()); });
    auto* tokenEdit = addLineEdit(
        page, tr("Token"), QString::fromStdString(taiga::accounts.anilistToken()),
        [](const QString& value) { taiga::accounts.setAnilistToken(value.toStdString()); },
        QLineEdit::Password);
    addComboBox(page, tr("Rating system"), "account.anilist.ratingSystem",
                {{"10 point", "POINT_10"}, {"100 point", "POINT_100"}, {"5 stars", "POINT_5"},
                 {"3 point smiley", "POINT_3"}, {"Advanced", "POINT_10_DECIMAL"}},
                "POINT_10");
    auto* authButton = addActionButton(page, tr("Authorize"));
    connect(authButton, &QPushButton::clicked, this,
            [this, tokenEdit, usernameEdit]() { authorizeAniList(tokenEdit, usernameEdit); });
  }

  const int libraryFolders = addSettingsPage(createFormPage());
  {
    auto* page = settingsPage(libraryFolders);
    auto* layout = qobject_cast<QVBoxLayout*>(page->layout());
    addCheckBox(page, tr("Watch library folders"), "library.watchFolders", true);
    addSpinBox(page, tr("Minimum video size"), "library.fileSizeThresholdMb", 10, 1, 10240, tr(" MiB"));
    addPathEdit(page, tr("Media player path"), "library.mediaPlayerPath", false);

    auto* box = new QGroupBox(tr("Folders"), page);
    auto* boxLayout = new QVBoxLayout(box);
    libraryFoldersList_ = new QListWidget(box);
    libraryFoldersList_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    libraryFoldersList_->setAcceptDrops(true);
    libraryFoldersList_->setDragDropMode(QAbstractItemView::DropOnly);
    libraryFoldersList_->installEventFilter(this);
    for (const auto& folder : taiga::settings.libraryFolders()) {
      libraryFoldersList_->addItem(QString::fromStdString(folder));
    }
    auto* buttons = new QHBoxLayout();
    auto* addButton = new QPushButton(tr("Add"), box);
    auto* removeButton = new QPushButton(tr("Remove"), box);
    buttons->addWidget(addButton);
    buttons->addWidget(removeButton);
    buttons->addStretch();
    boxLayout->addWidget(libraryFoldersList_);
    boxLayout->addLayout(buttons);
    layout->insertWidget(layout->count() - 1, box);
    connect(addButton, &QPushButton::clicked, this, &SettingsDialog::addLibraryFolder);
    connect(removeButton, &QPushButton::clicked, this, &SettingsDialog::removeSelectedLibraryFolders);
    connect(libraryFoldersList_, &QListWidget::itemDoubleClicked, this, [](QListWidgetItem* item) {
      QDesktopServices::openUrl(QUrl::fromLocalFile(item->text()));
    });
  }

  const int appGeneral = addSettingsPage(createFormPage());
  {
    auto* page = settingsPage(appGeneral);
    colorSchemeCombo_ = addComboBox(page, tr("Color scheme"), "app.colorScheme",
                                    {{tr("System"), static_cast<int>(Qt::ColorScheme::Unknown)},
                                     {tr("Light"), static_cast<int>(Qt::ColorScheme::Light)},
                                     {tr("Dark"), static_cast<int>(Qt::ColorScheme::Dark)}},
                                    static_cast<int>(Qt::ColorScheme::Unknown));
    addCheckBox(page, tr("Start with the system"), "program.general.autostart", false);
    addCheckBox(page, tr("Start minimized"), "program.startup.minimize", false);
    addCheckBox(page, tr("Close to tray"), "program.general.closeToTray", false);
    addCheckBox(page, tr("Minimize to tray"), "program.general.minimizeToTray", false);
    addCheckBox(page, tr("Check for updates on startup"), "program.startup.checkVersion", true);
    addCheckBox(page, tr("Scan available episodes on startup"), "program.startup.checkEpisodes", false);
    addCheckBox(page, tr("Reuse active connections"), "program.connection.reuseActive", true);
    addCheckBox(page, tr("Do not revoke SSL certificates"), "program.connection.noRevoke", false);
    addLineEdit(page, tr("External links format"), "program.general.externalLinks",
                "https://anilist.co/anime/%id%/");
    addLineEdit(page, tr("Proxy host"), "program.proxy.host");
    addLineEdit(page, tr("Proxy username"), "program.proxy.username");
    addLineEdit(page, tr("Proxy password"), "program.proxy.password", QString{}, QLineEdit::Password);
  }

  const int appList = addSettingsPage(createFormPage());
  {
    auto* page = settingsPage(appList);
    const QList<QPair<QString, QVariant>> listActions{
        {tr("Do nothing"), 0}, {tr("Edit details"), 1}, {tr("Open folder"), 2},
        {tr("Play next episode"), 3}, {tr("View anime info"), 4}, {tr("View anime page"), 5}};
    addComboBox(page, tr("Double click"), "program.list.doubleClickAction", listActions, 4);
    addComboBox(page, tr("Middle click"), "program.list.middleClickAction", listActions, 3);
    addComboBox(page, tr("Title language"), "program.list.titleLanguage",
                {{tr("Romaji"), "romaji"}, {tr("English"), "english"}, {tr("Native"), "native"}},
                "romaji");
    addCheckBox(page, tr("Highlight new episodes"), "program.list.highlightNewEpisodes", true);
    addCheckBox(page, tr("Display highlighted entries on top"), "program.list.highlightedOnTop", false);
    addCheckBox(page, tr("Show aired progress"), "program.list.progressAired", true);
    addCheckBox(page, tr("Show available progress"), "program.list.progressAvailable", true);
  }

  const int recognitionGeneral = addSettingsPage(createFormPage());
  {
    auto* page = settingsPage(recognitionGeneral);
    mediaDetectionIntervalSpin_ = addSpinBox(page, tr("Media detection interval"), "track.detection.interval", 3000, 250, 60000, tr(" ms"));
    addSpinBox(page, tr("Update delay"), "account.update.delay", 120, 10, 3600, tr(" sec"));
    addCheckBox(page, tr("Ask before updating list"), "account.update.askToConfirm", true);
    addCheckBox(page, tr("Check media player before update"), "account.update.checkPlayer", false);
    addCheckBox(page, tr("Update when episode is out of range"), "account.update.outOfRange", false);
    addCheckBox(page, tr("Update outside library folders"), "account.update.outOfRoot", false);
    addCheckBox(page, tr("Wait for player before update"), "account.update.waitPlayer", false);
    addCheckBox(page, tr("Show recognized notifications"), "program.notifications.recognized", true);
    addCheckBox(page, tr("Show unrecognized notifications"), "program.notifications.notRecognized", true);
    addCheckBox(page, tr("Go to Now Playing when recognized"), "account.update.gotoRecognized", true);
    addCheckBox(page, tr("Go to Now Playing when not recognized"), "account.update.gotoNotRecognized", false);
    addCheckBox(page, tr("Look up parent directories"), "recognition.lookupParentDirectories", true);
    addLineEdit(page, tr("Ignored strings"), "recognition.ignoredStrings");
  }

  const int recognitionMedia = addSettingsPage(createFormPage());
  addCheckBox(settingsPage(recognitionMedia), tr("Detect media players"), "recognition.mediaPlayers.enabled", true);
  createCheckedList(settingsPage(recognitionMedia), tr("Supported media players"),
                    "recognition.mediaPlayers", supportedMediaPlayers(), true);

  const int recognitionStreaming = addSettingsPage(createFormPage());
  addCheckBox(settingsPage(recognitionStreaming), tr("Detect streaming media"), "recognition.streaming.enabled", false);
  createCheckedList(settingsPage(recognitionStreaming), tr("Supported streaming providers"),
                    "recognition.streaming.providers",
                    {"ADN", "AnimeLab", "Anime News Network", "Bilibili", "Crunchyroll",
                     "Funimation", "HIDIVE", "Jellyfin", "Plex", "Roku Channel", "Tubi",
                     "Veoh", "VIZ", "VRV", "Wakanim", "Yahoo", "YouTube"},
                    true);

  const int sharingDiscord = addSettingsPage(createFormPage());
  addCheckBox(settingsPage(sharingDiscord), tr("Enable Discord sharing"), "announce.discord.enabled", false);
  addCheckBox(settingsPage(sharingDiscord), tr("Show username"), "announce.discord.username", true);
  addCheckBox(settingsPage(sharingDiscord), tr("Show release group"), "announce.discord.group", true);
  addCheckBox(settingsPage(sharingDiscord), tr("Show time"), "announce.discord.time", true);

  const int sharingHttp = addSettingsPage(createFormPage());
  addCheckBox(settingsPage(sharingHttp), tr("Enable HTTP sharing"), "announce.http.enabled", false);
  addLineEdit(settingsPage(sharingHttp), tr("URL"), "announce.http.url");
  auto* httpFormatEdit = addLineEdit(settingsPage(sharingHttp), tr("Format"), "announce.http.format",
                                     "user=%user%&name=%title%&ep=%episode%");
  {
    auto* formatButton = addActionButton(settingsPage(sharingHttp), tr("Edit format"));
    connect(formatButton, &QPushButton::clicked, this,
            [this, httpFormatEdit]() { editFormat(httpFormatEdit, tr("HTTP format")); });
  }

  const int sharingMirc = addSettingsPage(createFormPage());
  addCheckBox(settingsPage(sharingMirc), tr("Enable mIRC sharing"), "announce.mirc.enabled", false);
  addLineEdit(settingsPage(sharingMirc), tr("Service"), "announce.mirc.service", "mIRC");
  auto* mircCommandEdit = addLineEdit(settingsPage(sharingMirc), tr("Command"), "announce.mirc.command");
  addCheckBox(settingsPage(sharingMirc), tr("Use /me action"), "announce.mirc.useAction", true);
  addCheckBox(settingsPage(sharingMirc), tr("Use multiple servers"), "announce.mirc.multiServer", false);
  addComboBox(settingsPage(sharingMirc), tr("Target"), "announce.mirc.mode",
              {{tr("Active channel"), 1}, {tr("All channels"), 2}, {tr("Selected channels"), 3}}, 1);
  addLineEdit(settingsPage(sharingMirc), tr("Channels"), "announce.mirc.channels",
              "#kitsu, #myanimelist, #taiga");
  auto* mircFormatEdit = addLineEdit(settingsPage(sharingMirc), tr("Format"), "announce.mirc.format");
  {
    auto* formatButton = addActionButton(settingsPage(sharingMirc), tr("Edit format"));
    connect(formatButton, &QPushButton::clicked, this,
            [this, mircFormatEdit]() { editFormat(mircFormatEdit, tr("mIRC format")); });
  }
  {
    auto* testButton = addActionButton(settingsPage(sharingMirc), tr("Test connection"));
    connect(testButton, &QPushButton::clicked, this, [this, mircCommandEdit]() {
      const auto command = mircCommandEdit->text().trimmed();
      if (command.isEmpty()) {
        QMessageBox::information(this, tr("mIRC"), tr("Set a command to test external mIRC sharing."));
        return;
      }
      const auto ok = QProcess::startDetached(command, {});
      QMessageBox::information(this, tr("mIRC"),
                               ok ? tr("Command launched.") : tr("Could not launch command."));
    });
  }

  const int torrentsDiscovery = addSettingsPage(createFormPage());
  {
    auto* page = settingsPage(torrentsDiscovery);
    addEditableComboBox(page, tr("Feed source"), "rss.torrent.source",
                        {"https://anidex.info/rss/?cat=1&lang_id=1",
                         "https://nyaa.net/feed?c=3_5&s=0",
                         "https://nyaa.si/?page=rss&c=1_2&f=0",
                         "http://tracker.minglong.org/rss.xml",
                         "https://www.shanaproject.com/feeds/site/",
                         "https://subsplease.org/rss/?t&r=1080",
                         "https://www.tokyotosho.info/rss.php?filter=1,11&zwnj=0"},
                        "https://www.tokyotosho.info/rss.php?filter=1,11&zwnj=0");
    addEditableComboBox(page, tr("Search URL"), "rss.torrent.search",
                        {"https://anidex.info/rss/?cat=1&lang_id=1&q=%title%",
                         "https://nyaa.net/feed?c=3_5&s=0&q=%title%",
                         "https://nyaa.si/?page=rss&c=1_2&f=0&q=%title%"},
                        "https://nyaa.si/?page=rss&c=1_2&f=0&q=%title%");
    auto* autoCheck = addCheckBox(page, tr("Check automatically"), "rss.torrent.autoCheck", true);
    auto* interval = addSpinBox(page, tr("Check interval"), "rss.torrent.checkInterval", 60, 10, 3600, tr(" min"));
    auto* newAction = addComboBox(page, tr("New torrent action"), "rss.torrent.newAction",
                {{tr("Notify"), 1}, {tr("Download"), 2}}, 1);
    connect(autoCheck, &QCheckBox::toggled, interval, &QWidget::setEnabled);
    connect(autoCheck, &QCheckBox::toggled, newAction, &QWidget::setEnabled);
    interval->setEnabled(autoCheck->isChecked());
    newAction->setEnabled(autoCheck->isChecked());
  }

  const int torrentsDownloads = addSettingsPage(createFormPage());
  {
    auto* page = settingsPage(torrentsDownloads);
    addComboBox(page, tr("Sort by"), "rss.torrent.downloadSortBy",
                {{tr("Episode number"), "episode_number"}, {tr("Release date"), "release_date"}},
                "episode_number");
    addComboBox(page, tr("Sort order"), "rss.torrent.downloadSortOrder",
                {{tr("Ascending"), "ascending"}, {tr("Descending"), "descending"}}, "ascending");
    auto* useAnimeFolder = addCheckBox(page, tr("Use anime folder"), "rss.torrent.useAnimeFolder", true);
    auto* fallbackFolder = addCheckBox(page, tr("Fallback to download folder"), "rss.torrent.fallbackFolder", false);
    auto* downloadFolder = addPathEdit(page, tr("Download folder"), "rss.torrent.downloadLocation", true);
    auto* createSubfolder = addCheckBox(page, tr("Create anime subfolder"), "rss.torrent.createSubfolder", false);
    auto* openApp = addCheckBox(page, tr("Open torrent application"), "rss.torrent.openApp", true);
    auto* appMode = addComboBox(page, tr("Torrent application"), "rss.torrent.appMode",
                {{tr("System default"), 1}, {tr("Custom application"), 2}}, 1);
    auto* appPath = addPathEdit(page, tr("Application path"), "rss.torrent.appPath", false);
    addCheckBox(page, tr("Use magnet links"), "rss.torrent.useMagnet", false);
    connect(useAnimeFolder, &QCheckBox::toggled, fallbackFolder, &QWidget::setEnabled);
    connect(fallbackFolder, &QCheckBox::toggled, downloadFolder, &QWidget::setEnabled);
    connect(fallbackFolder, &QCheckBox::toggled, createSubfolder, &QWidget::setEnabled);
    connect(openApp, &QCheckBox::toggled, appMode, &QWidget::setEnabled);
    connect(openApp, &QCheckBox::toggled, appPath, &QWidget::setEnabled);
    fallbackFolder->setEnabled(useAnimeFolder->isChecked());
    downloadFolder->setEnabled(fallbackFolder->isChecked());
    createSubfolder->setEnabled(fallbackFolder->isChecked());
    appMode->setEnabled(openApp->isChecked());
    appPath->setEnabled(openApp->isChecked());
  }

  const int torrentsFilters = addSettingsPage(createFormPage());
  {
    auto* page = settingsPage(torrentsFilters);
    addCheckBox(page, tr("Enable torrent filters"), "rss.torrent.filters.enabled", true);
    addSpinBox(page, tr("Archive limit"), "rss.torrent.filters.archiveMaxCount", 1000, 0, 100000);

    auto* group = new QGroupBox(tr("Filters"), page);
    auto* groupLayout = new QVBoxLayout(group);
    auto* list = new QListWidget(group);
    list->setSelectionMode(QAbstractItemView::SingleSelection);
    loadTorrentFilters(list, torrentFilters());
    groupLayout->addWidget(list);

    auto* buttons = new QWidget(group);
    auto* row = new QHBoxLayout(buttons);
    row->setContentsMargins(0, 0, 0, 0);
    auto* addButton = new QPushButton(tr("Add"), buttons);
    auto* removeButton = new QPushButton(tr("Remove"), buttons);
    auto* upButton = new QPushButton(tr("Move up"), buttons);
    auto* downButton = new QPushButton(tr("Move down"), buttons);
    auto* importButton = new QPushButton(tr("Import"), buttons);
    auto* exportButton = new QPushButton(tr("Export"), buttons);
    auto* resetButton = new QPushButton(tr("Reset"), buttons);
    for (auto* button : {addButton, removeButton, upButton, downButton, importButton, exportButton, resetButton}) {
      row->addWidget(button);
    }
    row->addStretch();
    groupLayout->addWidget(buttons);

    auto* layout = qobject_cast<QVBoxLayout*>(page->layout());
    layout->insertWidget(layout->count() - 1, group);

    connect(addButton, &QPushButton::clicked, this, [this, list]() {
      bool accepted = false;
      const auto name = QInputDialog::getText(this, tr("Add torrent filter"), tr("Name"),
                                              QLineEdit::Normal, {}, &accepted);
      if (accepted && !name.trimmed().isEmpty()) addTorrentFilterItem(list, name.trimmed(), true);
    });
    connect(removeButton, &QPushButton::clicked, this, [list]() {
      delete list->takeItem(list->currentRow());
    });
    connect(upButton, &QPushButton::clicked, this, [list]() {
      const auto row = list->currentRow();
      if (row <= 0) return;
      auto* item = list->takeItem(row);
      list->insertItem(row - 1, item);
      list->setCurrentItem(item);
    });
    connect(downButton, &QPushButton::clicked, this, [list]() {
      const auto row = list->currentRow();
      if (row < 0 || row >= list->count() - 1) return;
      auto* item = list->takeItem(row);
      list->insertItem(row + 1, item);
      list->setCurrentItem(item);
    });
    connect(importButton, &QPushButton::clicked, this, [this, list]() {
      const auto fileName = QFileDialog::getOpenFileName(this, tr("Import torrent filters"), {},
                                                         tr("JSON files (*.json);;All files (*)"));
      if (fileName.isEmpty()) return;
      QFile file{fileName};
      if (!file.open(QIODevice::ReadOnly)) return;
      const auto document = QJsonDocument::fromJson(file.readAll());
      if (document.isArray()) loadTorrentFilters(list, document.array());
    });
    connect(exportButton, &QPushButton::clicked, this, [this, list]() {
      const auto fileName = QFileDialog::getSaveFileName(this, tr("Export torrent filters"), {},
                                                         tr("JSON files (*.json);;All files (*)"));
      if (fileName.isEmpty()) return;
      saveTorrentFilters(list);
      QFile file{fileName};
      if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(taiga::settings.stringValue("rss.torrent.filters.itemsJson").toUtf8());
      }
    });
    connect(resetButton, &QPushButton::clicked, this,
            [list]() { loadTorrentFilters(list, defaultTorrentFilters()); });
    saveActions_.push_back([list]() { saveTorrentFilters(list); });
  }

  const int advancedSettings = addSettingsPage(createFormPage());
  {
    auto* page = settingsPage(advancedSettings);
    addCheckBox(page, tr("Hide sidebar"), "program.general.hideSidebar", false);
    addCheckBox(page, tr("Enable recognition"), "program.general.enableRecognition", true);
    addCheckBox(page, tr("Enable sharing"), "program.general.enableSharing", true);
    addCheckBox(page, tr("Enable synchronization"), "program.general.enableSync", true);
    addCheckBox(page, tr("Remember window position"), "program.exit.rememberPosition", true);
    addSpinBox(page, tr("Window width"), "program.position.width", 960, 320, 7680);
    addSpinBox(page, tr("Window height"), "program.position.height", 640, 240, 4320);
    auto* group = new QGroupBox(tr("Editable advanced values"), page);
    auto* groupLayout = new QVBoxLayout(group);
    auto* advancedPage = new QWidget(group);
    auto* advancedLayout = new QVBoxLayout(advancedPage);
    advancedLayout->setContentsMargins(0, 0, 0, 0);
    advancedLayout->addStretch();
    groupLayout->addWidget(advancedPage);
    addCheckBox(advancedPage, tr("Application / Disable certificate revocation checks"), "program.connection.noRevoke", false);
    addLineEdit(advancedPage, tr("Application / Episode notification format"), "program.notifications.format");
    addLineEdit(advancedPage, tr("Application / Proxy host"), "program.proxy.host");
    addLineEdit(advancedPage, tr("Application / Proxy username"), "program.proxy.username");
    addLineEdit(advancedPage, tr("Application / Proxy password"), "program.proxy.password", QString{}, QLineEdit::Password);
    addCheckBox(advancedPage, tr("Application / Remember main window position and size"), "program.exit.rememberPosition", true);
    addCheckBox(advancedPage, tr("Application / Reuse active connections"), "program.connection.reuseActive", true);
    addLineEdit(advancedPage, tr("Application / UI theme"), "program.general.theme", "Default");
    addSpinBox(advancedPage, tr("Library / File size threshold"), "library.fileSizeThreshold", 10485760, 0, 2147483647, tr(" bytes"));
    addPathEdit(advancedPage, tr("Library / Media player path"), "library.mediaPlayerPath", false);
    addLineEdit(advancedPage, tr("Recognition / Ignored strings"), "recognition.ignoredStrings");
    addCheckBox(advancedPage, tr("Recognition / Look up parent directories"), "recognition.lookupParentDirectories", true);
    addSpinBox(advancedPage, tr("Recognition / Media detection interval"), "track.detection.interval", 3000, 250, 60000, tr(" ms"));
    addCheckBox(advancedPage, tr("Services / Kitsu / Download partial library"), "account.kitsu.partialLibrary", true);
    addLineEdit(advancedPage, tr("Sharing / Discord / Application ID"), "announce.discord.applicationId");
    addLineEdit(advancedPage, tr("Sharing / mIRC / Command"), "announce.mirc.command");
    addSpinBox(advancedPage, tr("Torrents / Archive limit"), "rss.torrent.filters.archiveMaxCount", 1000, 0, 100000);
    addPathEdit(advancedPage, tr("Torrents / Download path for .torrent files"), "rss.torrent.fileDownloadLocation", true);
    addCheckBox(advancedPage, tr("Torrents / Use magnet links if available"), "rss.torrent.useMagnet", false);
    auto* layout = qobject_cast<QVBoxLayout*>(page->layout());
    layout->insertWidget(layout->count() - 1, group);
  }

  const int advancedCache = addSettingsPage(createFormPage());
  {
    auto* page = settingsPage(advancedCache);
    auto* layout = qobject_cast<QVBoxLayout*>(page->layout());
    auto* cacheGroup = new QGroupBox(tr("Local data"), page);
    auto* cacheLayout = new QFormLayout(cacheGroup);
    historyCacheLabel_ = new QLabel(cacheGroup);
    posterCacheLabel_ = new QLabel(cacheGroup);
    torrentArchiveLabel_ = new QLabel(cacheGroup);
    torrentCacheLabel_ = new QLabel(cacheGroup);

    auto addCacheRow = [this, cacheLayout](const QString& label, QLabel* value,
                                           auto slot) {
      auto* row = new QWidget(cacheLayout->parentWidget());
      auto* rowLayout = new QHBoxLayout(row);
      rowLayout->setContentsMargins(0, 0, 0, 0);
      rowLayout->addWidget(value, 1);
      auto* clearButton = new QPushButton(tr("Clear"), row);
      rowLayout->addWidget(clearButton);
      cacheLayout->addRow(label, row);
      connect(clearButton, &QPushButton::clicked, this, slot);
    };

    addCacheRow(tr("History"), historyCacheLabel_, &SettingsDialog::clearHistory);
    addCacheRow(tr("Poster images"), posterCacheLabel_, &SettingsDialog::clearPosterCache);
    addCacheRow(tr("Torrent files"), torrentCacheLabel_, &SettingsDialog::clearTorrentCache);
    addCacheRow(tr("Torrent archive"), torrentArchiveLabel_, &SettingsDialog::clearTorrentArchive);

    auto* refreshButton = new QPushButton(tr("Refresh"), cacheGroup);
    cacheLayout->addRow({}, refreshButton);
    connect(refreshButton, &QPushButton::clicked, this, &SettingsDialog::refreshCachePage);
    layout->insertWidget(0, cacheGroup);
    refreshCachePage();
  }

  add_child(add_item("classic/24px/globe", "Services", servicesMain), "MyAnimeList", servicesMal);
  add_child(ui_->treeWidget->topLevelItem(0), "Kitsu", servicesKitsu);
  add_child(ui_->treeWidget->topLevelItem(0), "AniList", servicesAniList);
  add_item("classic/24px/folder-open", "Library", libraryFolders);
  add_child(add_item("classic/24px/application-sidebar-list", "Application", appGeneral), "Anime List",
            appList);
  add_child(add_item("classic/24px/inbox-document", "Recognition", recognitionGeneral),
            "Media players", recognitionMedia);
  add_child(ui_->treeWidget->topLevelItem(3), "Streaming", recognitionStreaming);
  add_child(add_item("classic/24px/megaphone", "Sharing", sharingDiscord), "HTTP", sharingHttp);
  add_child(ui_->treeWidget->topLevelItem(4), "mIRC", sharingMirc);
  add_child(add_item("classic/24px/feed", "Torrents", torrentsDiscovery), "Downloads",
            torrentsDownloads);
  add_child(ui_->treeWidget->topLevelItem(5), "Filters", torrentsFilters);
  add_child(add_item("classic/24px/gear", "Advanced", advancedSettings), "Cache", advancedCache);

  ui_->treeWidget->expandAll();
  connect(ui_->treeWidget, &QTreeWidget::currentItemChanged, this,
          [this](QTreeWidgetItem* current, QTreeWidgetItem*) {
            if (!current) return;
            auto text = current->text(0);
            if (current->parent()) {
              text = u"%1 / %2"_s.arg(current->parent()->text(0), text);
            }
            ui_->titleLabel->setText(text);
            ui_->stackedWidget->setCurrentIndex(current->data(0, Qt::UserRole).toInt());
          });
  ui_->treeWidget->setCurrentItem(ui_->treeWidget->topLevelItem(0));
}

void SettingsDialog::accept() {
  saveSettings();
  QDialog::accept();
}

void SettingsDialog::show(QWidget* parent) {
  auto dlg = new SettingsDialog(parent);
  dlg->setAttribute(Qt::WA_DeleteOnClose);
  dlg->setModal(true);
  dlg->QDialog::show();
}

bool SettingsDialog::eventFilter(QObject* object, QEvent* event) {
  if (object == libraryFoldersList_ && event->type() == QEvent::DragEnter) {
    auto* dragEvent = static_cast<QDragEnterEvent*>(event);
    if (dragEvent->mimeData()->hasUrls()) {
      dragEvent->acceptProposedAction();
      return true;
    }
  }

  if (object == libraryFoldersList_ && event->type() == QEvent::Drop) {
    auto* dropEvent = static_cast<QDropEvent*>(event);
    for (const auto& url : dropEvent->mimeData()->urls()) {
      const auto path = url.toLocalFile();
      if (path.isEmpty() || !QFileInfo{path}.isDir()) continue;
      if (libraryFoldersList_->findItems(path, Qt::MatchExactly).isEmpty()) {
        libraryFoldersList_->addItem(path);
      }
    }
    dropEvent->acceptProposedAction();
    return true;
  }

  return QDialog::eventFilter(object, event);
}

void SettingsDialog::addLibraryFolder() {
  const auto directory = QFileDialog::getExistingDirectory(
      this, tr("Add Library Folder"), {}, QFileDialog::ShowDirsOnly | QFileDialog::ReadOnly);
  if (directory.isEmpty()) return;

  if (libraryFoldersList_->findItems(directory, Qt::MatchExactly).isEmpty()) {
    libraryFoldersList_->addItem(directory);
  }
}

void SettingsDialog::authorizeAniList(QLineEdit* tokenEdit, QLineEdit* usernameEdit) {
  QDesktopServices::openUrl(QUrl{QString::fromStdString(taiga_sync::anilist::requestTokenUrl())});

  bool accepted = false;
  const auto input = QInputDialog::getText(
      this, tr("Authorize AniList"),
      tr("Paste the token or redirected URL from AniList."), QLineEdit::Normal,
      {}, &accepted);
  if (!accepted) return;

  auto token = queryValueFromInput(input, "access_token");
  if (token.isEmpty()) token = input.trimmed();
  if (token.isEmpty()) return;

  tokenEdit->setText(token);
  taiga::accounts.setAnilistToken(token.toStdString());

  QNetworkRequest request{QUrl{"https://graphql.anilist.co"}};
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
  request.setRawHeader("Authorization", "Bearer " + token.toUtf8());

  const auto payload = QJsonDocument{QJsonObject{
      {"query", "query { Viewer { name } }"},
  }}.toJson(QJsonDocument::Compact);

  auto* reply = taiga::network()->post(request, payload);
  const QPointer<SettingsDialog> guard{this};
  connect(reply, &QNetworkReply::finished, this, [guard, reply, usernameEdit]() {
    if (!guard) {
      reply->deleteLater();
      return;
    }

    if (reply->error() != QNetworkReply::NoError) {
      QMessageBox::warning(guard, QObject::tr("AniList"),
                           QObject::tr("Could not verify token: %1").arg(reply->errorString()));
      reply->deleteLater();
      return;
    }

    QString errorMessage;
    const auto object = replyObject(reply, &errorMessage);
    const auto viewer = object.value("data").toObject().value("Viewer").toObject();
    const auto username = viewer.value("name").toString();
    if (username.isEmpty()) {
      QMessageBox::warning(guard, QObject::tr("AniList"),
                           errorMessage.isEmpty() ? QObject::tr("Could not read the AniList user.")
                                                  : errorMessage);
      reply->deleteLater();
      return;
    }

    usernameEdit->setText(username);
    taiga::accounts.setAnilistUsername(username.toStdString());
    QMessageBox::information(guard, QObject::tr("AniList"),
                             QObject::tr("Authorized as %1.").arg(username));
    reply->deleteLater();
  });
}

void SettingsDialog::authorizeKitsu(QLineEdit* emailEdit, QLineEdit* passwordEdit,
                                    QLineEdit* usernameEdit, QLineEdit* displayNameEdit) {
  const auto email = emailEdit->text().trimmed();
  const auto username = usernameEdit->text().trimmed();
  const auto password = passwordEdit->text();
  const auto login = email.isEmpty() ? username : email;
  if (login.isEmpty() || password.isEmpty()) {
    QMessageBox::warning(this, tr("Kitsu"), tr("Enter your email or username and password first."));
    return;
  }

  taiga::accounts.setKitsuEmail(email.toStdString());
  taiga::accounts.setKitsuUsername(username.toStdString());
  taiga::accounts.setKitsuPassword(password.toStdString());

  QNetworkRequest request{QUrl{"https://kitsu.app/api/oauth/token"}};
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
  request.setRawHeader("Accept", "application/vnd.api+json");

  constexpr auto kClientId =
      "dd031b32d2f56c990b1425efe6c42ad847e7fe3ab46bf1299f05ecd856bdb7dd";
  constexpr auto kClientSecret =
      "54d7307928f63414defd96399fc31ba847961ceaecef3a5fd93144e960c0e151";
  auto* reply = taiga::network()->post(
      request,
      formData({
          {"grant_type", "password"},
          {"username", login},
          {"password", password},
          {"client_id", kClientId},
          {"client_secret", kClientSecret},
      }));

  const QPointer<SettingsDialog> guard{this};
  connect(reply, &QNetworkReply::finished, this,
          [guard, reply, usernameEdit, displayNameEdit]() {
            if (!guard) {
              reply->deleteLater();
              return;
            }

            if (reply->error() != QNetworkReply::NoError) {
              QMessageBox::warning(guard, QObject::tr("Kitsu"),
                                   QObject::tr("Could not log in: %1").arg(reply->errorString()));
              reply->deleteLater();
              return;
            }

            QString errorMessage;
            const auto object = replyObject(reply, &errorMessage);
            const auto accessToken = object.value("access_token").toString();
            if (accessToken.isEmpty()) {
              QMessageBox::warning(guard, QObject::tr("Kitsu"),
                                   errorMessage.isEmpty() ? QObject::tr("Kitsu did not return an access token.")
                                                          : errorMessage);
              reply->deleteLater();
              return;
            }
            taiga::accounts.setKitsuAccessToken(accessToken.toStdString());
            reply->deleteLater();

            QUrl userUrl{"https://kitsu.app/api/edge/users"};
            QUrlQuery query;
            query.addQueryItem("filter[self]", "true");
            userUrl.setQuery(query);

            QNetworkRequest userRequest{userUrl};
            userRequest.setRawHeader("Accept", "application/vnd.api+json");
            userRequest.setRawHeader("Authorization", "Bearer " + accessToken.toUtf8());
            auto* userReply = taiga::network()->get(userRequest);
            connect(userReply, &QNetworkReply::finished, guard,
                    [guard, userReply, usernameEdit, displayNameEdit]() {
                      if (!guard) {
                        userReply->deleteLater();
                        return;
                      }

                      if (userReply->error() != QNetworkReply::NoError) {
                        QMessageBox::warning(guard, QObject::tr("Kitsu"),
                                             QObject::tr("Token saved, but user lookup failed: %1")
                                                 .arg(userReply->errorString()));
                        userReply->deleteLater();
                        return;
                      }

                      QString errorMessage;
                      const auto object = replyObject(userReply, &errorMessage);
                      const auto data = object.value("data").toArray();
                      const auto user = data.isEmpty() ? QJsonObject{} : data.first().toObject();
                      const auto userId = user.value("id").toString();
                      const auto attributes = user.value("attributes").toObject();
                      const auto slug = attributes.value("slug").toString();
                      const auto name = attributes.value("name").toString();
                      if (!userId.isEmpty()) {
                        taiga::accounts.setKitsuUserId(userId.toStdString());
                      }
                      if (!slug.isEmpty()) {
                        usernameEdit->setText(slug);
                        taiga::accounts.setKitsuUsername(slug.toStdString());
                      }
                      if (!name.isEmpty()) {
                        displayNameEdit->setText(name);
                        taiga::settings.setStringValue("account.kitsu.displayName", name);
                      }

                      QMessageBox::information(guard, QObject::tr("Kitsu"),
                                               slug.isEmpty()
                                                   ? QObject::tr("Logged in.")
                                                   : QObject::tr("Logged in as %1.").arg(slug));
                      userReply->deleteLater();
                    });
          });
}

void SettingsDialog::authorizeMyAnimeList(QLineEdit* usernameEdit, QLineEdit* accessTokenEdit,
                                          QLineEdit* refreshTokenEdit) {
  std::string codeVerifier;
  QDesktopServices::openUrl(
      QUrl{QString::fromStdString(taiga_sync::myanimelist::authorizationCodeUrl(codeVerifier))});

  bool accepted = false;
  const auto input = QInputDialog::getText(
      this, tr("Authorize MyAnimeList"),
      tr("Paste the authorization code or redirected URL from MyAnimeList."),
      QLineEdit::Normal, {}, &accepted);
  if (!accepted) return;

  auto code = queryValueFromInput(input, "code");
  if (code.isEmpty()) code = input.trimmed();
  if (code.isEmpty()) return;

  QNetworkRequest request{QUrl{"https://myanimelist.net/v1/oauth2/token"}};
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
  auto* reply = taiga::network()->post(
      request,
      formData({
          {"client_id", taiga_sync::myanimelist::kClientId},
          {"grant_type", "authorization_code"},
          {"code", code},
          {"redirect_uri", taiga_sync::myanimelist::kRedirectUrl},
          {"code_verifier", QString::fromStdString(codeVerifier)},
      }));

  const QPointer<SettingsDialog> guard{this};
  connect(reply, &QNetworkReply::finished, this,
          [guard, reply, usernameEdit, accessTokenEdit, refreshTokenEdit]() {
            if (!guard) {
              reply->deleteLater();
              return;
            }

            if (reply->error() != QNetworkReply::NoError) {
              QMessageBox::warning(guard, QObject::tr("MyAnimeList"),
                                   QObject::tr("Could not request token: %1").arg(reply->errorString()));
              reply->deleteLater();
              return;
            }

            QString errorMessage;
            const auto object = replyObject(reply, &errorMessage);
            const auto accessToken = object.value("access_token").toString();
            const auto refreshToken = object.value("refresh_token").toString();
            if (accessToken.isEmpty()) {
              QMessageBox::warning(guard, QObject::tr("MyAnimeList"),
                                   errorMessage.isEmpty()
                                       ? QObject::tr("MyAnimeList did not return an access token.")
                                       : errorMessage);
              reply->deleteLater();
              return;
            }

            accessTokenEdit->setText(accessToken);
            refreshTokenEdit->setText(refreshToken);
            taiga::accounts.setMyanimelistAccessToken(accessToken.toStdString());
            taiga::accounts.setMyanimelistRefreshToken(refreshToken.toStdString());
            reply->deleteLater();

            QNetworkRequest userRequest{QUrl{"https://api.myanimelist.net/v2/users/@me"}};
            userRequest.setRawHeader("Authorization", "Bearer " + accessToken.toUtf8());
            auto* userReply = taiga::network()->get(userRequest);
            connect(userReply, &QNetworkReply::finished, guard,
                    [guard, userReply, usernameEdit]() {
                      if (!guard) {
                        userReply->deleteLater();
                        return;
                      }

                      if (userReply->error() != QNetworkReply::NoError) {
                        QMessageBox::warning(guard, QObject::tr("MyAnimeList"),
                                             QObject::tr("Token saved, but user lookup failed: %1")
                                                 .arg(userReply->errorString()));
                        userReply->deleteLater();
                        return;
                      }

                      QString errorMessage;
                      const auto object = replyObject(userReply, &errorMessage);
                      const auto username = object.value("name").toString();
                      if (!username.isEmpty()) {
                        usernameEdit->setText(username);
                        taiga::accounts.setMyanimelistUsername(username.toStdString());
                      }
                      QMessageBox::information(guard, QObject::tr("MyAnimeList"),
                                               username.isEmpty()
                                                   ? QObject::tr("Authorized.")
                                                   : QObject::tr("Authorized as %1.").arg(username));
                      userReply->deleteLater();
                    });
          });
}

void SettingsDialog::clearHistory() {
  anime::history.clear();
  refreshCachePage();
}

void SettingsDialog::clearPosterCache() {
  const int removed = removeFiles(posterCachePath(), {"*.jpg", "*.jpeg", "*.png", "*.webp"});
  refreshCachePage();
  QMessageBox::information(this, tr("Poster cache"), tr("Removed %1 file(s).").arg(removed));
}

void SettingsDialog::clearTorrentArchive() {
  const auto removed = QFile::remove(torrentArchivePath());
  refreshCachePage();
  QMessageBox::information(this, tr("Torrent archive"),
                           removed ? tr("Removed the torrent archive.")
                                   : tr("No torrent archive was found."));
}

void SettingsDialog::clearTorrentCache() {
  const int removed = removeFiles(torrentCachePath(), {"*.torrent"});
  refreshCachePage();
  QMessageBox::information(this, tr("Torrent cache"), tr("Removed %1 file(s).").arg(removed));
}

void SettingsDialog::editFormat(QLineEdit* formatEdit, const QString& title) {
  bool accepted = false;
  const auto value = QInputDialog::getMultiLineText(
      this, title, tr("Format"), formatEdit->text(), &accepted);
  if (accepted) {
    formatEdit->setText(value);
  }
}

void SettingsDialog::removeSelectedLibraryFolders() {
  for (auto* item : libraryFoldersList_->selectedItems()) {
    delete item;
  }
}

void SettingsDialog::refreshCachePage() const {
  if (!historyCacheLabel_ || !posterCacheLabel_ || !torrentArchiveLabel_ || !torrentCacheLabel_) {
    return;
  }

  const auto posterStats = directoryStats(posterCachePath(), {"*.jpg", "*.jpeg", "*.png", "*.webp"});
  const auto torrentStats = directoryStats(torrentCachePath(), {"*.torrent"});
  const auto archiveInfo = QFileInfo{torrentArchivePath()};

  historyCacheLabel_->setText(tr("%1 item(s)").arg(anime::history.items().size()));
  posterCacheLabel_->setText(tr("%1 file(s), %2").arg(posterStats.count).arg(formatSize(posterStats.size)));
  torrentCacheLabel_->setText(tr("%1 file(s), %2").arg(torrentStats.count).arg(formatSize(torrentStats.size)));
  torrentArchiveLabel_->setText(archiveInfo.exists()
                                    ? tr("%1 item(s), %2")
                                          .arg(torrentArchiveCount())
                                          .arg(formatSize(archiveInfo.size()))
                                    : tr("No archive"));
}

void SettingsDialog::saveSettings() const {
  for (const auto& action : saveActions_) {
    action();
  }

  std::vector<std::string> folders;
  folders.reserve(libraryFoldersList_->count());
  for (int i = 0; i < libraryFoldersList_->count(); ++i) {
    folders.push_back(libraryFoldersList_->item(i)->text().toStdString());
  }
  taiga::settings.setLibraryFolders(std::move(folders));
}

QCheckBox* SettingsDialog::addCheckBox(QWidget* page, const QString& text, QString key,
                                       bool defaultValue) {
  auto* checkBox = new QCheckBox(text, page);
  checkBox->setChecked(taiga::settings.boolValue(key, defaultValue));
  auto* layout = qobject_cast<QVBoxLayout*>(page->layout());
  layout->insertWidget(layout->count() - 1, checkBox);
  saveActions_.push_back([checkBox, key]() {
    taiga::settings.setBoolValue(key, checkBox->isChecked());
  });
  return checkBox;
}

QComboBox* SettingsDialog::addComboBox(QWidget* page, const QString& label, QString key,
                                       const QList<QPair<QString, QVariant>>& items,
                                       const QVariant& defaultValue) {
  auto* comboBox = new QComboBox(page);
  for (const auto& [text, data] : items) {
    comboBox->addItem(text, data);
  }

  const auto current = taiga::settings.stringValue(key, defaultValue.toString());
  int index = comboBox->findData(current);
  if (index == -1) index = comboBox->findData(defaultValue);
  comboBox->setCurrentIndex(std::max(0, index));

  auto* layout = qobject_cast<QVBoxLayout*>(page->layout());
  auto* row = new QWidget(page);
  auto* rowLayout = new QFormLayout(row);
  rowLayout->setContentsMargins(0, 0, 0, 0);
  rowLayout->addRow(label, comboBox);
  layout->insertWidget(layout->count() - 1, row);
  saveActions_.push_back([comboBox, key]() {
    const auto value = comboBox->currentData();
    if (value.typeId() == QMetaType::Int) {
      taiga::settings.setIntValue(key, value.toInt());
    } else {
      taiga::settings.setStringValue(key, value.toString());
    }
  });
  return comboBox;
}

QComboBox* SettingsDialog::addEditableComboBox(QWidget* page, const QString& label, QString key,
                                               const QStringList& items,
                                               const QString& defaultValue) {
  auto* comboBox = new QComboBox(page);
  comboBox->setEditable(true);
  comboBox->addItems(items);
  const auto current = taiga::settings.stringValue(key, defaultValue);
  if (!current.isEmpty()) comboBox->setCurrentText(current);

  auto* layout = qobject_cast<QVBoxLayout*>(page->layout());
  auto* row = new QWidget(page);
  auto* rowLayout = new QFormLayout(row);
  rowLayout->setContentsMargins(0, 0, 0, 0);
  rowLayout->addRow(label, comboBox);
  layout->insertWidget(layout->count() - 1, row);
  saveActions_.push_back([comboBox, key]() {
    taiga::settings.setStringValue(key, comboBox->currentText());
  });
  return comboBox;
}

QLineEdit* SettingsDialog::addLineEdit(QWidget* page, const QString& label, QString key,
                                       const QString& defaultValue, QLineEdit::EchoMode echoMode) {
  auto* lineEdit = new QLineEdit(taiga::settings.stringValue(key, defaultValue), page);
  lineEdit->setEchoMode(echoMode);
  auto* layout = qobject_cast<QVBoxLayout*>(page->layout());
  auto* row = new QWidget(page);
  auto* rowLayout = new QFormLayout(row);
  rowLayout->setContentsMargins(0, 0, 0, 0);
  rowLayout->addRow(label, lineEdit);
  layout->insertWidget(layout->count() - 1, row);
  saveActions_.push_back([lineEdit, key]() {
    taiga::settings.setStringValue(key, lineEdit->text());
  });
  return lineEdit;
}

QLineEdit* SettingsDialog::addLineEdit(QWidget* page, const QString& label, const QString& value,
                                       std::function<void(const QString&)> save,
                                       QLineEdit::EchoMode echoMode) {
  auto* lineEdit = new QLineEdit(value, page);
  lineEdit->setEchoMode(echoMode);
  auto* layout = qobject_cast<QVBoxLayout*>(page->layout());
  auto* row = new QWidget(page);
  auto* rowLayout = new QFormLayout(row);
  rowLayout->setContentsMargins(0, 0, 0, 0);
  rowLayout->addRow(label, lineEdit);
  layout->insertWidget(layout->count() - 1, row);
  saveActions_.push_back([lineEdit, save = std::move(save)]() {
    save(lineEdit->text());
  });
  return lineEdit;
}

QLineEdit* SettingsDialog::addPathEdit(QWidget* page, const QString& label, QString key,
                                       bool directory, const QString& defaultValue) {
  auto* lineEdit = new QLineEdit(taiga::settings.stringValue(key, defaultValue), page);
  auto* browseButton = new QPushButton(tr("Browse"), page);
  auto* row = new QWidget(page);
  auto* rowLayout = new QHBoxLayout(row);
  rowLayout->setContentsMargins(0, 0, 0, 0);
  rowLayout->addWidget(lineEdit, 1);
  rowLayout->addWidget(browseButton);

  auto* layout = qobject_cast<QVBoxLayout*>(page->layout());
  auto* formRow = new QWidget(page);
  auto* formLayout = new QFormLayout(formRow);
  formLayout->setContentsMargins(0, 0, 0, 0);
  formLayout->addRow(label, row);
  layout->insertWidget(layout->count() - 1, formRow);

  connect(browseButton, &QPushButton::clicked, this, [this, lineEdit, directory]() {
    const auto path = directory
        ? QFileDialog::getExistingDirectory(this, tr("Select Folder"), lineEdit->text())
        : QFileDialog::getOpenFileName(this, tr("Select File"), lineEdit->text());
    if (!path.isEmpty()) {
      lineEdit->setText(path);
    }
  });

  saveActions_.push_back([lineEdit, key]() {
    taiga::settings.setStringValue(key, lineEdit->text());
  });
  return lineEdit;
}

QSpinBox* SettingsDialog::addSpinBox(QWidget* page, const QString& label, QString key,
                                     int defaultValue, int minimum, int maximum,
                                     const QString& suffix) {
  auto* spinBox = new QSpinBox(page);
  spinBox->setRange(minimum, maximum);
  spinBox->setValue(taiga::settings.intValue(key, defaultValue));
  spinBox->setSuffix(suffix);
  auto* layout = qobject_cast<QVBoxLayout*>(page->layout());
  auto* row = new QWidget(page);
  auto* rowLayout = new QFormLayout(row);
  rowLayout->setContentsMargins(0, 0, 0, 0);
  rowLayout->addRow(label, spinBox);
  layout->insertWidget(layout->count() - 1, row);
  saveActions_.push_back([spinBox, key]() {
    taiga::settings.setIntValue(key, spinBox->value());
  });
  return spinBox;
}

QWidget* SettingsDialog::createFormPage() {
  auto* page = new QWidget(this);
  auto* layout = new QVBoxLayout(page);
  layout->setContentsMargins(16, 16, 16, 16);
  layout->setSpacing(10);
  layout->addStretch();
  return page;
}

QListWidget* SettingsDialog::createCheckedList(QWidget* page, const QString& label,
                                               QString keyPrefix, const QStringList& items,
                                               bool defaultValue) {
  auto* group = new QGroupBox(label, page);
  auto* groupLayout = new QVBoxLayout(group);
  auto* list = new QListWidget(group);
  for (const auto& item : items) {
    auto* listItem = new QListWidgetItem(item, list);
    listItem->setFlags(listItem->flags() | Qt::ItemIsUserCheckable);
    listItem->setCheckState(
        taiga::settings.boolValue(u"%1.%2"_s.arg(keyPrefix, slug(item)), defaultValue)
            ? Qt::Checked
            : Qt::Unchecked);
  }
  groupLayout->addWidget(list);
  auto* buttons = new QWidget(group);
  auto* buttonsLayout = new QHBoxLayout(buttons);
  buttonsLayout->setContentsMargins(0, 0, 0, 0);
  auto* selectAll = new QPushButton(tr("Select all"), buttons);
  auto* clearAll = new QPushButton(tr("Clear all"), buttons);
  buttonsLayout->addWidget(selectAll);
  buttonsLayout->addWidget(clearAll);
  buttonsLayout->addStretch();
  groupLayout->addWidget(buttons);
  connect(selectAll, &QPushButton::clicked, this, [list]() {
    for (int i = 0; i < list->count(); ++i) list->item(i)->setCheckState(Qt::Checked);
  });
  connect(clearAll, &QPushButton::clicked, this, [list]() {
    for (int i = 0; i < list->count(); ++i) list->item(i)->setCheckState(Qt::Unchecked);
  });

  auto* layout = qobject_cast<QVBoxLayout*>(page->layout());
  layout->insertWidget(layout->count() - 1, group);
  saveActions_.push_back([list, keyPrefix]() {
    for (int i = 0; i < list->count(); ++i) {
      auto* item = list->item(i);
      taiga::settings.setBoolValue(u"%1.%2"_s.arg(keyPrefix, slug(item->text())),
                                   item->checkState() == Qt::Checked);
    }
  });
  return list;
}

int SettingsDialog::addSettingsPage(QWidget* page) {
  auto* scrollArea = new QScrollArea(this);
  scrollArea->setWidgetResizable(true);
  scrollArea->setFrameShape(QFrame::NoFrame);
  scrollArea->setWidget(page);
  return ui_->stackedWidget->addWidget(scrollArea);
}

QWidget* SettingsDialog::settingsPage(int index) const {
  auto* scrollArea = qobject_cast<QScrollArea*>(ui_->stackedWidget->widget(index));
  return scrollArea ? scrollArea->widget() : nullptr;
}

}  // namespace gui
