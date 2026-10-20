#pragma once
#include "core/game.h"
#include "core/theme.h"
#include <QPainter>
namespace solitare {
void drawTableDecoration(QPainter &painter, QRectF rect, const Theme &theme);
void drawSuit(QPainter &painter, QRectF rect, int suit, QColor color);
void drawCard(QPainter &painter, QRectF rect, const Card &card,
              const Theme &theme, bool selected = false);
} // namespace solitare
