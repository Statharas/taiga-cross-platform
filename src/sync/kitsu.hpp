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

#include "sync/service.hpp"

#include <functional>
#include <memory>

#include <QList>

namespace taiga_sync::kitsu {

enum ListPrivacy {
  kPrivate = 1,
  kPublic,
};

enum ListStatus {
  kCurrentlyWatching = 1,
  kPlanToWatch,
  kCompleted,
  kOnHold,
  kDropped,
};

class Service final : public taiga_sync::Service {
public:
  static Service* instance();

  void fetchListEntries(std::function<void(bool, const QString&)> done = {});
  void fetchSeason(const anime::Season season, std::function<void(bool, const QString&)> done = {});

private:
  void fetchAuthenticatedUser(std::function<void(bool, const QString&, const QString&)> done);
  void fetchListEntriesPage(const QString& userId, int offset, bool clearFirst, int updatedCount,
                            std::function<void(bool, const QString&)> done);
  void fetchSeasonPage(const anime::Season season, int offset,
                       const std::shared_ptr<QList<int>>& ids,
                       std::function<void(bool, const QString&)> done);
};

}  // namespace taiga_sync::kitsu
