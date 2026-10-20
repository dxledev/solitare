#include "board.h"
#include "card_art.h"
#include <QPainter>
#include <algorithm>
#include <cmath>
namespace solitare {
namespace {
QPixmap layerImage(QSizeF size, qreal dpr) {
  QPixmap image(QSize(qCeil(size.width() * dpr), qCeil(size.height() * dpr)));
  image.setDevicePixelRatio(dpr);
  image.fill(Qt::transparent);
  return image;
}
}
void Board::paintDecoration(QPainter &p) {
  const auto dpr = devicePixelRatioF();
  if (decoration_.isNull() || decorationSize_ != size() || decorationDpr_ != dpr) {
    decoration_ = layerImage(size(), dpr);
    QPainter painter(&decoration_);
    drawTableDecoration(painter, rect(), theme_);
    decorationSize_ = size();
    decorationDpr_ = dpr;
  }
  p.drawPixmap(QPointF(), decoration_);
}
void Board::paintTable(QPainter &p) {
  p.setRenderHint(QPainter::Antialiasing);
  paintDecoration(p);
  const auto locations = focusPiles();
  for (const auto loc : locations)
    if (loc.kind != Location::Tableau) paintPile(p, loc, *pileCards(loc));
  for (int i = 0; i < int(game_.state().tableau.size()); ++i)
    paintPile(p, {Location::Tableau, i, -1}, game_.state().tableau[i]);
  paintLabels(p);
  if (game_.state().mode == GameMode::Klondike && game_.won()) {
    const auto first = cardRect({Location::Tableau, 0, -1}), last = cardRect({Location::Tableau, 6, -1});
    QFont big = font();
    big.setPixelSize(qRound(first.width() * .45));
    big.setBold(true);
    p.setFont(big);
    p.setPen(theme_.color("accent"));
    p.drawText(QRectF(first.left(), first.bottom() + 16, last.right() - first.left(), first.width() * .7), Qt::AlignCenter, "You won!");
  }
  if (hasFocus() && !locations.empty()) {
    const auto loc = locations[std::clamp(keyboardPile_, 0, int(locations.size()) - 1)];
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(theme_.color("accent"), 1.5, Qt::DashLine));
    p.drawRoundedRect(cardRect(loc).adjusted(-4, -4, 4, 4), 9, 9);
  }
}
void Board::ensureLayers() {
  const RenderState current{game_.state(), size(), devicePixelRatioF(), selected_, hintFrom_, hintTo_, dragging_, hasFocus(), keyboardPile_};
  if (rendered_ && *rendered_ == current) return;
  tableLayer_ = layerImage(size(), current.dpr);
  QPainter painter(&tableLayer_);
  paintTable(painter);
  painter.end();
  buildDragLayer();
  rendered_ = current;
  ++layerBuilds_;
}
void Board::buildDragLayer() {
  dragLayer_ = QPixmap();
  if (!dragging_ || !selected_) return;
  const auto loc = *selected_;
  const auto *cards = pileCards(loc);
  if (!cards || cards->empty()) return;
  const int start = loc.index < 0 ? int(cards->size()) - 1 : loc.index;
  const auto first = cardRect({loc.kind, loc.pile, start});
  const auto last = cardRect({loc.kind, loc.pile, int(cards->size()) - 1});
  const auto bounds = first.united(last).adjusted(-3, -3, 3, 3);
  const auto dpr = devicePixelRatioF();
  dragSourceBounds_ = QRectF(QPointF(std::floor(bounds.left() * dpr) / dpr, std::floor(bounds.top() * dpr) / dpr),
                             QPointF(std::ceil(bounds.right() * dpr) / dpr, std::ceil(bounds.bottom() * dpr) / dpr));
  dragLayer_ = layerImage(dragSourceBounds_.size(), dpr);
  QPainter painter(&dragLayer_);
  for (int i = start; i < int(cards->size()); ++i)
    artwork_.paintCard(painter, cardRect({loc.kind, loc.pile, i}).translated(-dragSourceBounds_.topLeft()), (*cards)[i], true, devicePixelRatioF());
}
QRectF Board::dragBounds(QPointF point) const {
  if (!selected_) return {};
  const auto loc = *selected_;
  const auto *cards = pileCards(loc);
  if (!cards || cards->empty()) return {};
  const int start = loc.index < 0 ? int(cards->size()) - 1 : loc.index;
  return cardRect({loc.kind, loc.pile, start}).united(cardRect({loc.kind, loc.pile, int(cards->size()) - 1}))
      .translated(point - pressPoint_).adjusted(-5, -5, 5, 5);
}
}
