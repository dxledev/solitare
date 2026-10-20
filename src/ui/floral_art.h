#pragma once
#include "core/game.h"
#include "core/theme.h"
#include <QPainter>
namespace solitare::floral {
void drawFrame(QPainter &painter, QRectF rect, const Theme &theme);
void drawIllustration(QPainter &painter, QRectF rect, const Card &card, const Theme &theme);
void drawBack(QPainter &painter, QRectF rect, const Theme &theme);
}
