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

#include "painters.hpp"

#include <QGuiApplication>
#include <QPainter>
#include <QProxyStyle>
#include <QStyleOption>

#include "base/string.hpp"
#include "gui/models/anime_list_model.hpp"
#include "gui/utils/format.hpp"
#include "gui/utils/theme.hpp"
#include "media/anime.hpp"
#include "media/anime_list.hpp"
#include "media/anime_list_utils.hpp"

namespace gui {

QRect progressButtonRect(const QRect& rect, bool increment) {
  const auto size = std::max(0, rect.height());
  return increment ? QRect{rect.right() - size + 1, rect.top(), size, size}
                   : QRect{rect.left(), rect.top(), size, size};
}

void paintProgressButton(QPainter* painter, const QStyleOption& option, bool increment,
                         bool enabled) {
  if (!enabled) return;

  const auto rect = progressButtonRect(option.rect, increment);
  const auto border = theme.isDark() ? QColor{210, 210, 210, 180} : QColor{70, 70, 70, 220};
  const auto background = theme.isDark() ? QColor{56, 60, 64} : QColor{245, 245, 245};
  const auto foreground = theme.isDark() ? QColor{245, 245, 245} : QColor{20, 20, 20};

  painter->save();
  painter->setRenderHint(QPainter::Antialiasing, false);
  painter->fillRect(rect.adjusted(0, 0, -1, -1), background);
  painter->setPen(border);
  painter->drawRect(rect.adjusted(0, 0, -1, -1));

  const auto center = rect.center();
  const int half = std::max(3, rect.height() / 5);
  painter->setPen(QPen{foreground, 2});
  painter->drawLine(center.x() - half, center.y(), center.x() + half, center.y());
  if (increment) painter->drawLine(center.x(), center.y() - half, center.x(), center.y() + half);
  painter->restore();
}

void paintProgressButtons(QPainter* painter, const QStyleOption& option, const ListEntry* entry,
                          bool canIncrement) {
  const auto canDecrement = entry && entry->watched_episodes > 0;
  paintProgressButton(painter, option, false, canDecrement);
  paintProgressButton(painter, option, true, canIncrement);
}

void paintEmptyListText(QAbstractScrollArea* area, const QString& text) {
  QPainter painter(area->viewport());

  painter.setFont([&painter]() {
    auto font = painter.font();
    font.setItalic(true);
    return font;
  }());

  painter.drawText(area->viewport()->rect(), Qt::AlignCenter, text);
}

void paintProgressBar(QPainter* painter, const QStyleOption& option, const Anime* anime,
                      const ListEntry* entry, bool showButtons) {
  if (!anime || !entry) return;

  const int episodes = anime->episode_count;
  const int watched = std::clamp(entry->watched_episodes, 0,
                                 episodes > 0 ? episodes : std::numeric_limits<int>::max());

  if (episodes <= 0) {
    auto textRect = option.rect;
    if (showButtons) textRect.adjust(option.rect.height(), 0, -option.rect.height(), 0);
    const auto baseText = option.palette.color(QPalette::ColorRole::Text);
    const auto mutedText =
        option.palette.color(QPalette::ColorGroup::Disabled, QPalette::ColorRole::Text);
    const auto slashWidth = option.fontMetrics.horizontalAdvance("/");
    const auto gap = 4;
    const auto center = textRect.center().x();

    auto watchedRect = textRect;
    watchedRect.setRight(center - gap);
    auto totalRect = textRect;
    totalRect.setLeft(center + slashWidth + gap);

    painter->setPen(mutedText);
    painter->drawText(textRect, Qt::AlignCenter, "/");

    painter->setPen(watched > 0 ? baseText : mutedText);
    painter->drawText(watchedRect, Qt::AlignRight | Qt::AlignVCenter, formatNumber(watched, "0"));

    painter->setPen(mutedText);
    painter->drawText(totalRect, Qt::AlignLeft | Qt::AlignVCenter, "?");

    if (showButtons) {
      paintProgressButtons(painter, option, entry, true);
    }
    return;
  }

  QStyleOptionProgressBar styleOption{};
  styleOption.state = option.state | QStyle::State_Horizontal;
  styleOption.direction = option.direction;
  styleOption.rect = option.rect;
  styleOption.palette = option.palette;
  styleOption.palette.setCurrentColorGroup(QPalette::ColorGroup::Active);
  styleOption.palette.setColor(QPalette::ColorRole::Highlight, theme.isDark()
                                                                   ? QColor{12, 164, 12, 128}
                                                                   : QColor{12, 164, 12, 255});
  styleOption.fontMetrics = option.fontMetrics;
  styleOption.maximum = 100;
  styleOption.minimum = 0;
  styleOption.progress = static_cast<int>(anime::list::getProgressRatio(anime, entry) * 100);
  styleOption.text = {};
  styleOption.textAlignment = Qt::AlignCenter;
  styleOption.textVisible = false;

  static const auto proxyStyle{new QProxyStyle{"fusion"}};
  proxyStyle->drawControl(QStyle::CE_ProgressBar, &styleOption, painter);

  const auto baseText = option.palette.color(QPalette::ColorRole::Text);
  const auto mutedText =
      option.palette.color(QPalette::ColorGroup::Disabled, QPalette::ColorRole::Text);
  const auto slashWidth = option.fontMetrics.horizontalAdvance("/");
  const auto gap = 4;
  const auto center = option.rect.center().x();

  auto watchedRect = option.rect;
  watchedRect.setRight(center - gap);
  auto totalRect = option.rect;
  totalRect.setLeft(center + slashWidth + gap);

  painter->setPen(mutedText);
  painter->drawText(option.rect, Qt::AlignCenter, "/");

  painter->setPen(watched > 0 ? baseText : mutedText);
  painter->drawText(watchedRect, Qt::AlignRight | Qt::AlignVCenter,
                    formatNumber(watched, "0"));

  painter->setPen(episodes > 0 ? baseText : mutedText);
  painter->drawText(totalRect, Qt::AlignLeft | Qt::AlignVCenter, formatNumber(episodes, "?"));

  if (showButtons) {
    paintProgressButtons(painter, option, entry, entry->watched_episodes < episodes);
  }
}

}  // namespace gui
