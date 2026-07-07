#include <QApplication>
#include <QAbstractButton>
#include <QComboBox>
#include <QFile>
#include <QGroupBox>
#include <QImage>
#include <QLabel>
#include <QStackedWidget>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QLineEdit>
#include <QXmlStreamReader>
#include <QDir>

#include <cstdlib>
#include <iostream>
#include <string>

#include "gui/settings/settings_dialog.hpp"
#include "gui/utils/image_provider.hpp"
#include "media/anime.hpp"
#include "media/anime_db.hpp"
#include "media/anime_list.hpp"
#include "media/anime_list_export.hpp"

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(EXIT_FAILURE);
  }
}

QStringList visibleText(const gui::SettingsDialog& dialog) {
  QStringList values;
  for (const auto* label : dialog.findChildren<QLabel*>()) {
    values.push_back(label->text());
  }
  for (const auto* button : dialog.findChildren<QAbstractButton*>()) {
    values.push_back(button->text());
  }
  for (const auto* combo : dialog.findChildren<QComboBox*>()) {
    for (int i = 0; i < combo->count(); ++i) {
      values.push_back(combo->itemText(i));
    }
  }
  for (const auto* group : dialog.findChildren<QGroupBox*>()) {
    values.push_back(group->title());
  }
  return values;
}

void requireText(const QStringList& values, const QString& text) {
  const auto message = QString("Missing settings component: %1").arg(text).toStdString();
  require(values.contains(text), message.c_str());
}

QString xmlTextElement(const QString& path, const QString& name) {
  QFile file{path};
  require(file.open(QIODevice::ReadOnly | QIODevice::Text), "Could not open exported XML");
  QXmlStreamReader xml{&file};
  while (!xml.atEnd()) {
    xml.readNext();
    if (xml.isStartElement() && xml.name() == name) {
      return xml.readElementText();
    }
  }
  return {};
}

}  // namespace

int main(int argc, char* argv[]) {
  qputenv("QT_QPA_PLATFORM", "offscreen");

  QApplication app(argc, argv);
  QCoreApplication::setApplicationName("taiga-settings-dialog-test");
  QCoreApplication::setOrganizationName("taiga");
  QTemporaryDir dataDir;
  require(dataDir.isValid(), "Could not create isolated settings test data directory");
  qputenv("TAIGA_DATA_PATH", dataDir.path().toUtf8());
  anime::db.init();

  gui::SettingsDialog dialog(nullptr);

  const auto tree = dialog.findChild<QTreeWidget*>();
  const auto stack = dialog.findChild<QStackedWidget*>();

  require(tree != nullptr, "Settings tree was not created");
  require(stack != nullptr, "Settings page stack was not created");
  require(tree->topLevelItemCount() == 7, "Settings section count changed");
  require(stack->count() == 18, "Settings page count changed");
  require(tree->topLevelItem(0)->text(0) == "Services", "Services section missing");
  require(tree->topLevelItem(0)->childCount() == 3, "Service page count changed");
  require(tree->topLevelItem(5)->text(0) == "Torrents", "Torrents section missing");
  require(tree->topLevelItem(6)->childCount() == 1, "Advanced cache page missing");

  const auto values = visibleText(dialog);
  for (const auto& text : {
           "Authorize", "Log in", "Watch library folders", "External links format",
           "Double click", "Detect media players", "Detect streaming media",
           "Enable Discord sharing", "Edit format", "Test connection", "Command",
           "Feed source", "Download folder", "Enable torrent filters",
           "Filters", "Add", "Edit", "Select all", "Clear all",
           "Application / Remember main window position and size",
           "Torrents / Download path for .torrent files", "History", "Poster images",
           "Torrent archive",
       }) {
    requireText(values, text);
  }

  Anime one;
  one.id = 10;
  one.titles.romaji = "alpha";
  anime::db.updateItem(one);
  ListEntry watching;
  watching.id = 10;
  watching.anime_id = 10;
  watching.status = anime::list::Status::Watching;
  watching.score = 80;
  anime::db.updateEntry(watching);

  Anime two;
  two.id = 11;
  two.titles.romaji = "Beta";
  anime::db.updateItem(two);
  ListEntry completed;
  completed.id = 11;
  completed.anime_id = 11;
  completed.status = anime::list::Status::Completed;
  anime::db.updateEntry(completed);

  const auto xmlPath = dataDir.filePath("list.xml");
  require(anime::list::exportAsXml(xmlPath.toStdString()), "MAL XML export failed");
  require(xmlTextElement(xmlPath, "user_total_anime") == "2", "MAL XML total count changed");
  require(xmlTextElement(xmlPath, "user_total_watching") == "1", "MAL XML watching count changed");
  require(xmlTextElement(xmlPath, "user_total_completed") == "1", "MAL XML completed count changed");
  require(xmlTextElement(xmlPath, "my_score") == "8", "MAL XML score conversion changed");

  QDir{}.mkpath(dataDir.filePath("v1/db/image"));
  QImage image{1, 1, QImage::Format_ARGB32};
  image.fill(Qt::red);
  require(image.save(dataDir.filePath("v1/db/image/10.png")), "Could not create PNG poster cache fixture");
  const auto* poster = gui::imageProvider.loadPoster(10);
  require(poster != nullptr && !poster->isNull(), "Image provider did not load non-JPEG cached poster");

  return EXIT_SUCCESS;
}
