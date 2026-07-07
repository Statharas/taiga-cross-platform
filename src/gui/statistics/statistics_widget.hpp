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

#include <QWidget>

namespace gui {

class StatisticsWidget final : public QWidget {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(StatisticsWidget)

public:
  explicit StatisticsWidget(QWidget* parent = nullptr);
  ~StatisticsWidget() = default;
};

}  // namespace gui
