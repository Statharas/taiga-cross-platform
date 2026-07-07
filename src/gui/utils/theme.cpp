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

#include "theme.hpp"

#include <QApplication>
#include <QPalette>
#include <QStyle>
#include <QStyleHints>

#include "base/file.hpp"
#include "base/string.hpp"
#include "gui/utils/svg_icon_engine.hpp"
#include "taiga/settings.hpp"

namespace gui {

Theme::Theme() : QObject() {}

const QIcon& Theme::getIcon(const QString& key, const QString& extension, bool useSvgIconEngine) {
  if (!m_icons.contains(key)) {
    if (extension == "svg" && useSvgIconEngine) {
      m_icons[key] = QIcon(new SvgIconEngine(key));
    } else {
      m_icons[key] = QIcon(u":/icons/%1.%2"_s.arg(key, extension));
    }
  }

  return m_icons[key];
}

void Theme::initStyle() {
  qApp->styleHints()->setColorScheme(Qt::ColorScheme::Light);

  connect(qApp->styleHints(), &QStyleHints::colorSchemeChanged, this,
          [](Qt::ColorScheme) { qApp->styleHints()->setColorScheme(Qt::ColorScheme::Light); });

  qApp->setStyle("fusion");
  auto palette = qApp->style()->standardPalette();
  palette.setColor(QPalette::Window, QColor("#f0f0f0"));
  palette.setColor(QPalette::WindowText, Qt::black);
  palette.setColor(QPalette::Base, Qt::white);
  palette.setColor(QPalette::AlternateBase, QColor("#f7f7f7"));
  palette.setColor(QPalette::Text, Qt::black);
  palette.setColor(QPalette::Button, QColor("#f0f0f0"));
  palette.setColor(QPalette::ButtonText, Qt::black);
  palette.setColor(QPalette::Highlight, QColor("#3399ff"));
  palette.setColor(QPalette::HighlightedText, Qt::white);
  qApp->setPalette(palette);

  const QString mainStylesheet = readStylesheet("main");
  const QString themeStylesheet = readStylesheet("light");
  qApp->setStyleSheet(mainStylesheet + themeStylesheet);
}

bool Theme::isDark() const {
  return qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark;
}

QString Theme::readStylesheet(const QString& name) const {
  return base::readFile(u":/styles/%1.qss"_s.arg(name));
}

}  // namespace gui
