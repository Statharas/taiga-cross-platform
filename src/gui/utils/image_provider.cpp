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

#include "image_provider.hpp"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

#include "base/string.hpp"
#include "media/anime_db.hpp"
#include "taiga/network.hpp"
#include "taiga/path.hpp"

namespace gui {

void ImageProvider::fetchPoster(const int id) {
  if (m_pending.contains(id)) return;

  const auto item = anime::db.item(id);

  if (!item || item->image_url.empty()) return;

  const QUrl url{QString::fromStdString(item->image_url)};
  if (!url.isValid() || url.scheme().isEmpty()) return;

  m_pending.insert(id);
  const auto reply = taiga::network()->get(QNetworkRequest{url});

  connect(reply, &QNetworkReply::finished, this, [this, id, reply]() {
    m_pending.remove(id);
    if (reply->error() != QNetworkReply::NoError) return;

    const auto payload = reply->readAll();
    QBuffer buffer;
    buffer.setData(payload);
    buffer.open(QIODevice::ReadOnly);
    QImageReader payloadReader{&buffer};
    const auto format = QString::fromLatin1(payloadReader.format()).toLower();
    if (payload.isEmpty() || payloadReader.read().isNull()) return;

    QFile file{fileName(id, format.isEmpty() ? "jpg" : format)};
    QDir{}.mkpath(QFileInfo{file}.absolutePath());
    if (!file.open(QIODevice::WriteOnly)) return;
    file.write(payload);
    reloadPoster(id);
  });
}

const QPixmap* ImageProvider::loadPoster(const int id) {
  if (const auto it = m_pixmaps.find(id); it != m_pixmaps.end()) {
    return &it.value();
  }

  QImageReader reader(cachedFileName(id));
  const QImage image = reader.read();

  m_pixmaps[id] = !image.isNull() ? QPixmap::fromImage(image) : QPixmap{};

  if (image.isNull()) fetchPoster(id);

  return &m_pixmaps[id];
}

void ImageProvider::reloadPoster(const int id) {
  m_pixmaps.remove(id);
  loadPoster(id);
  emit posterChanged(id);
}

QString ImageProvider::cacheDirectory() const {
  const auto path = QString::fromStdString(taiga::get_data_path());
  return u"%1/v1/db/image"_s.arg(path);
}

QString ImageProvider::fileName(const int id, const QString& extension) const {
  auto normalized = extension.toLower();
  if (normalized == "jpeg") normalized = "jpg";
  return u"%1/%2.%3"_s.arg(cacheDirectory()).arg(id).arg(normalized);
}

QString ImageProvider::cachedFileName(const int id) const {
  static const QStringList extensions{"jpg", "jpeg", "png", "webp"};
  for (const auto& extension : extensions) {
    const auto path = fileName(id, extension);
    if (QFileInfo::exists(path)) return path;
  }
  return fileName(id);
}

}  // namespace gui
