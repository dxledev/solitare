#include "board_art_cache.h"
#include "card_art.h"
#include <cmath>
#include <memory>
namespace solitare {
BoardArtCache::BoardArtCache(int limitKiB) : cards_(limitKiB), theme_(presetTheme("lofi")) {}
void BoardArtCache::setTheme(const Theme &theme) {
  if (theme_ == theme) return;
  theme_ = theme;
  cards_.clear();
}
void BoardArtCache::synchronize(QSizeF cardSize, qreal dpr) {
  if (cardSize_ == cardSize && dpr_ == dpr) return;
  cardSize_ = cardSize;
  dpr_ = dpr;
  cards_.clear();
}
void BoardArtCache::paintCard(QPainter &p, QRectF r, const Card &card, bool selected, qreal dpr) {
  synchronize(r.size(), dpr);
  const qreal padding = std::ceil(3 * dpr) / dpr;
  const QPointF origin(std::floor(r.x() * dpr) / dpr - padding,
                        std::floor(r.y() * dpr) / dpr - padding);
  const auto local = r.translated(-origin);
  const QString key = QString("%1/%2/%3/%4/%5")
      .arg(card.faceUp ? card.rank + card.suit * 13 : 0).arg(selected)
      .arg(local.x(), 0, 'f', 7).arg(local.y(), 0, 'f', 7).arg(card.faceUp);
  auto *image = cards_.object(key);
  if (!image) {
    auto created = std::make_unique<QPixmap>(QSize(qCeil((r.width() + padding * 2 + 1 / dpr) * dpr),
                                                  qCeil((r.height() + padding * 2 + 1 / dpr) * dpr)));
    created->setDevicePixelRatio(dpr);
    created->fill(Qt::transparent);
    QPainter painter(created.get());
    drawCard(painter, local, card, theme_, selected);
    painter.end();
    ++rasterizations_;
    const int cost = std::max(1, int((qint64(created->width()) * created->height() * 4 + 1023) / 1024));
    p.drawPixmap(origin, *created);
    cards_.insert(key, created.release(), cost);
    return;
  }
  p.drawPixmap(origin, *image);
}
}
