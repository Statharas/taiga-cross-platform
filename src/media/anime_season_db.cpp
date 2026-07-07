/**
 * Taiga
 * Copyright (C) 2010-2024, Eren Okka
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "anime_season_db.hpp"

#include <utility>

namespace anime {

void SeasonDatabase::set(const Season& season, QList<int> ids) {
  current_season = season;
  items = std::move(ids);
}

void SeasonDatabase::reset() {
  current_season = Season{};
  items.clear();
}

bool SeasonDatabase::matches(const Season& season) const {
  return current_season.name == season.name && current_season.year == season.year;
}

}  // namespace anime
