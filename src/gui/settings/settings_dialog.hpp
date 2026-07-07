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

#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QList>
#include <QPair>
#include <QVariant>
#include <functional>
#include <vector>

class QCheckBox;
class QComboBox;
class QLabel;
class QListWidget;
class QSpinBox;
class QStackedWidget;

namespace Ui {
class SettingsDialog;
}

namespace gui {

class SettingsDialog final : public QDialog {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(SettingsDialog)

public:
  SettingsDialog(QWidget* parent);
  ~SettingsDialog() = default;

  static void show(QWidget* parent);

public slots:
  void accept() override;

private:
  bool eventFilter(QObject* object, QEvent* event) override;

  void addLibraryFolder();
  void authorizeAniList(QLineEdit* tokenEdit, QLineEdit* usernameEdit);
  void authorizeKitsu(QLineEdit* emailEdit, QLineEdit* passwordEdit, QLineEdit* usernameEdit,
                      QLineEdit* displayNameEdit);
  void authorizeMyAnimeList(QLineEdit* usernameEdit, QLineEdit* accessTokenEdit,
                            QLineEdit* refreshTokenEdit);
  void clearHistory();
  void clearPosterCache();
  void clearTorrentArchive();
  void clearTorrentCache();
  void editFormat(QLineEdit* formatEdit, const QString& title);
  void removeSelectedLibraryFolders();
  void refreshCachePage() const;
  void saveSettings() const;

  QCheckBox* addCheckBox(QWidget* page, const QString& text, QString key, bool defaultValue);
  QComboBox* addComboBox(QWidget* page, const QString& label, QString key,
                         const QList<QPair<QString, QVariant>>& items,
                         const QVariant& defaultValue);
  QComboBox* addEditableComboBox(QWidget* page, const QString& label, QString key,
                                 const QStringList& items, const QString& defaultValue = {});
  QLineEdit* addLineEdit(QWidget* page, const QString& label, QString key,
                         const QString& defaultValue = {}, QLineEdit::EchoMode echoMode = QLineEdit::Normal);
  QLineEdit* addLineEdit(QWidget* page, const QString& label, const QString& value,
                         std::function<void(const QString&)> save,
                         QLineEdit::EchoMode echoMode = QLineEdit::Normal);
  QLineEdit* addPathEdit(QWidget* page, const QString& label, QString key, bool directory,
                         const QString& defaultValue = {});
  QSpinBox* addSpinBox(QWidget* page, const QString& label, QString key, int defaultValue,
                       int minimum, int maximum, const QString& suffix = {});
  QWidget* createFormPage();
  QListWidget* createCheckedList(QWidget* page, const QString& label, QString keyPrefix,
                                 const QStringList& items, bool defaultValue);
  int addSettingsPage(QWidget* page);
  QWidget* settingsPage(int index) const;

  QComboBox* colorSchemeCombo_ = nullptr;
  QComboBox* serviceCombo_ = nullptr;
  QLabel* historyCacheLabel_ = nullptr;
  QLabel* posterCacheLabel_ = nullptr;
  QLabel* torrentArchiveLabel_ = nullptr;
  QLabel* torrentCacheLabel_ = nullptr;
  QListWidget* libraryFoldersList_ = nullptr;
  QSpinBox* mediaDetectionIntervalSpin_ = nullptr;
  std::vector<std::function<void()>> saveActions_;

  Ui::SettingsDialog* ui_ = nullptr;
};

}  // namespace gui
