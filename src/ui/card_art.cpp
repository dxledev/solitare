#include "card_art.h"
#include "floral_art.h"
#include <QPainterPath>
#include <algorithm>
namespace solitare {
void drawSuit(QPainter &p, QRectF rect, int suit, QColor color) {
  p.save();
  p.translate(rect.center());
  p.scale(rect.width() / 24, rect.height() / 24);
  p.setPen(Qt::NoPen);
  p.setBrush(color);
  QPainterPath path;
  path.setFillRule(Qt::WindingFill);
  if (suit == 1) {
    path.moveTo(0, -11);
    path.lineTo(8, 0);
    path.lineTo(0, 11);
    path.lineTo(-8, 0);
    path.closeSubpath();
  } else if (suit == 3) {
    path.moveTo(0, 10);
    path.cubicTo(-18, -1, -7, -17, 0, -6);
    path.cubicTo(7, -17, 18, -1, 0, 10);
  } else if (suit == 2) {
    path.moveTo(0, -11);
    path.cubicTo(-19, 2, -6, 10, 0, 3);
    path.cubicTo(6, 10, 19, 2, 0, -11);
  } else {
    path.addEllipse(QRectF(-5, -11, 10, 10));
    path.addEllipse(QRectF(-10, -3, 10, 10));
    path.addEllipse(QRectF(0, -3, 10, 10));
  }
  if (suit == 0 || suit == 2) {
    QPainterPath stem;
    stem.moveTo(-1, 0);
    stem.lineTo(-5, 11);
    stem.lineTo(5, 11);
    stem.lineTo(1, 0);
    stem.closeSubpath();
    path = path.united(stem);
  }
  p.drawPath(path);
  p.restore();
}
namespace {
QColor blend(QColor first, QColor second, double amount) {
  return QColor::fromRgbF(first.redF() * (1 - amount) + second.redF() * amount,
                         first.greenF() * (1 - amount) + second.greenF() * amount,
                         first.blueF() * (1 - amount) + second.blueF() * amount);
}
Theme artworkTheme(const Theme &source) {
  Theme theme = source;
  for (const auto &key : {"floral_red", "floral_green", "floral_blue", "floral_gold", "floral_cream"})
    theme.colors[key] = blend(source.color(key), source.color("accent"), .22);
  theme.colors["card"] = blend(blend(QColor("#fffaf0"), source.color("card"), .06),
                               source.color("accent"), .04);
  return theme;
}
QColor suitInk(const Theme &source, const Theme &artwork, bool red) {
  auto ink = blend(artwork.color(red ? "floral_red" : "floral_blue"),
                   source.color(red ? "red_suit" : "black_suit"), red ? .25 : .16);
  if (ink.lightnessF() > .40)
    ink.setHslF(ink.hslHueF(), ink.hslSaturationF(), .40);
  return ink;
}
QString rankName(int rank) {
  if (rank == 1) return "A";
  if (rank == 11) return "J";
  if (rank == 12) return "Q";
  if (rank == 13) return "K";
  return QString::number(rank);
}
} // namespace
void drawTableDecoration(QPainter &p, QRectF r, const Theme &source) {
  floral::drawFrame(p, r, artworkTheme(source));
}
namespace {
void cornerMarks(QPainter &p, QRectF r, const Card &card, QColor ink) {
  QFont rankFont("Noto Sans");
  rankFont.setPixelSize(std::max(10, qRound(r.width() * .21)));
  rankFont.setWeight(QFont::DemiBold);
  p.setFont(rankFont); p.setPen(ink);
  p.drawText(r.adjusted(r.width() * .09, 2, -3, -2), Qt::AlignLeft | Qt::AlignTop, rankName(card.rank));
  drawSuit(p, QRectF(r.right() - r.width() * .28, r.top() + r.width() * .055,
                     r.width() * .20, r.width() * .20), card.suit, ink);
}
void faceMarks(QPainter &p, QRectF r, const Card &card, QColor ink) {
  cornerMarks(p, r, card, ink);
  p.save(); p.translate(r.center()); p.rotate(180); p.translate(-r.center());
  cornerMarks(p, r, card, ink);
  p.restore();
}
}
void drawCard(QPainter &p, QRectF r, const Card &card, const Theme &source, bool selected) {
  const auto theme = artworkTheme(source);
  p.save(); p.setRenderHint(QPainter::Antialiasing);
  p.setPen(QPen(theme.color(selected ? "accent" : "border"), selected ? 2.5 : 1));
  p.setBrush(card.faceUp ? theme.color("card") : source.color("card_back"));
  p.drawRoundedRect(r, 6, 6);
  if (!card.faceUp) floral::drawBack(p, r, theme);
  else {
    floral::drawIllustration(p, r, card, theme);
    faceMarks(p, r, card, suitInk(source, theme, card.red()));
  }
  if (selected) {
    p.setPen(QPen(source.color("accent"), 2.5)); p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(r, 6, 6);
  }
  p.restore();
}
} // namespace solitare
