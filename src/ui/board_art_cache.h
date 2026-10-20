#pragma once
#include "core/game.h"
#include "core/theme.h"
#include <QCache>
#include <QPainter>
namespace solitare {
class BoardArtCache {
public:
  explicit BoardArtCache(int limitKiB = 32768);
  void setTheme(const Theme &theme);
  void paintCard(QPainter &painter, QRectF rect, const Card &card, bool selected, qreal dpr);
  int rasterizations() const { return rasterizations_; }
private:
  QCache<QString, QPixmap> cards_;
  Theme theme_;
  QSizeF cardSize_;
  qreal dpr_ = 0;
  int rasterizations_ = 0;
  void synchronize(QSizeF cardSize, qreal dpr);
};
}
