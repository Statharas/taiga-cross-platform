/**
 * Taiga
 * Copyright (C) 2010-2024, Eren Okka
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include <QList>

#include "media/anime_season.hpp"

namespace anime {

class SeasonDatabase final {
public:
  void set(const Season& season, QList<int> ids);
  void reset();
  bool matches(const Season& season) const;

  Season current_season;
  QList<int> items;
};

inline SeasonDatabase season_db;

}  // namespace anime
