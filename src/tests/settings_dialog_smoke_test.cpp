#include <QApplication>
#include <QAbstractButton>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QStackedWidget>
#include <QTreeWidget>
#include <QLineEdit>

#include <cstdlib>
#include <iostream>
#include <string>

#include "gui/settings/settings_dialog.hpp"

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

}  // namespace

int main(int argc, char* argv[]) {
  qputenv("QT_QPA_PLATFORM", "offscreen");

  QApplication app(argc, argv);
  QCoreApplication::setApplicationName("taiga-settings-dialog-test");
  QCoreApplication::setOrganizationName("taiga");

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
           "Filters", "Select all", "Clear all",
           "Application / Remember main window position and size",
           "Torrents / Download path for .torrent files", "History", "Poster images",
           "Torrent archive",
       }) {
    requireText(values, text);
  }

  return EXIT_SUCCESS;
}
