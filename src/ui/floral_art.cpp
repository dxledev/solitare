#include "floral_art.h"
#include <QPainterPath>
#include <algorithm>
#include <cmath>
#include <numbers>
namespace solitare::floral {
namespace {
QColor color(const Theme &theme, const char *name) { return theme.color(QString("floral_") + name); }
QPointF polar(double angle, double radius) {
  const double radians = angle * std::numbers::pi / 180;
  return {std::cos(radians) * radius, std::sin(radians) * radius};
}
void stem(QPainter &p, const QPainterPath &path, QColor ink, double width = 2.4) {
  p.setBrush(Qt::NoBrush);
  p.setPen(QPen(ink, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  p.drawPath(path);
}
void leaf(QPainter &p, QPointF start, QPointF end, double width, QColor ink, QColor vein = {}) {
  const QPointF direction = end - start;
  const double length = std::hypot(direction.x(), direction.y());
  if (length < .01) return;
  const QPointF normal(-direction.y() / length * width, direction.x() / length * width);
  QPainterPath shape(start);
  shape.cubicTo(start + direction * .28 + normal, start + direction * .72 + normal, end);
  shape.cubicTo(start + direction * .74 - normal * .70, start + direction * .27 - normal * .85, start);
  p.setPen(Qt::NoPen); p.setBrush(ink); p.drawPath(shape);
  if (vein.isValid()) {
    QPainterPath line(start + direction * .10);
    line.quadTo(start + direction * .5 + normal * .08, end - direction * .13);
    stem(p, line, vein, std::max(.45, width * .06));
  }
}
void foliage(QPainter &p, QPointF start, QPointF end, double width, const Theme &theme) {
  auto vein = color(theme, "cream"); vein.setAlpha(105);
  leaf(p, start, end, width, color(theme, "green"), vein);
}
void berry(QPainter &p, QPointF center, double radius, const Theme &theme) {
  p.setPen(Qt::NoPen); p.setBrush(color(theme, "red"));
  p.drawEllipse(center, radius, radius);
}
void berryBranch(QPainter &p, QPointF base, double scale, double angle, const Theme &theme) {
  p.save(); p.translate(base); p.rotate(angle); p.scale(scale, scale);
  QPainterPath branch(QPointF(0, 0));
  branch.quadTo(0, -12, 0, -32);
  branch.moveTo(0, -12); branch.quadTo(-7, -19, -14, -20);
  branch.moveTo(0, -21); branch.quadTo(7, -24, 13, -30);
  stem(p, branch, color(theme, "green"), 1.8);
  berry(p, {0, -34}, 5.5, theme); berry(p, {-15, -20}, 5, theme); berry(p, {14, -31}, 5, theme);
  p.restore();
}
void blossom(QPainter &p, QPointF center, double radius, const Theme &theme) {
  p.save(); p.translate(center);
  p.setPen(Qt::NoPen); p.setBrush(color(theme, "cream"));
  for (int petal = 0; petal < 6; ++petal) {
    p.save(); p.rotate(petal * 60);
    p.drawEllipse(QRectF(-radius * .31, -radius, radius * .62, radius * .78));
    p.restore();
  }
  p.setBrush(color(theme, "red")); p.drawEllipse(QPointF(), radius * .24, radius * .24);
  p.restore();
}
void stamens(QPainter &p, QPointF center, double radius, const Theme &theme) {
  p.save(); p.translate(center);
  const auto navy = color(theme, "blue");
  for (int index = 0; index < 17; ++index) {
    const double angle = 194 + index * 9.5;
    const auto tip = polar(angle, radius * (index % 2 ? .72 : .81));
    p.setPen(QPen(navy, radius * .047, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(polar(angle, radius * .30), tip);
    p.setPen(Qt::NoPen); p.setBrush(navy);
    p.drawEllipse(tip, radius * .061, radius * .061);
    p.setBrush(color(theme, "gold"));
    p.drawEllipse(tip, radius * .020, radius * .020);
  }
  p.setBrush(navy); p.drawEllipse(QPointF(), radius * .31, radius * .27);
  p.setPen(QPen(color(theme, "gold"), radius * .018));
  p.setBrush(Qt::NoBrush); p.drawArc(QRectF(-radius * .30, -radius * .26, radius * .60, radius * .52), 16 * 18, 16 * 144);
  p.restore();
}
void poppy(QPainter &p, QPointF center, double radius, const Theme &theme) {
  p.save(); p.translate(center); p.scale(radius, radius);
  QPainterPath petal;
  petal.moveTo(-.15, .40);
  petal.cubicTo(-.38, .18, -.79, -.03, -.82, -.40);
  petal.cubicTo(-.89, -.77, -.56, -.99, -.29, -.86);
  petal.cubicTo(-.08, -1.02, .28, -.93, .37, -.61);
  petal.cubicTo(.50, -.22, .20, .16, .08, .43);
  petal.closeSubpath();
  const auto red = color(theme, "red");
  for (int index = 0; index < 5; ++index) {
    p.save(); p.rotate(index * 72 - 15);
    p.setPen(Qt::NoPen); p.setBrush(index % 2 ? red.darker(107) : red);
    p.drawPath(petal);
    p.restore();
  }
  p.setPen(QPen(color(theme, "gold"), .018)); p.setBrush(Qt::NoBrush);
  for (double angle : {215.0, 245.0, 278.0, 309.0, 340.0}) {
    QPainterPath vein(QPointF(0, .24));
    vein.quadTo(polar(angle, .36), polar(angle, .78)); p.drawPath(vein);
  }
  stamens(p, QPointF(0, .23), .70, theme);
  p.restore();
}
void bell(QPainter &p, QPointF center, double radius, const Theme &theme, QColor petals = {}) {
  p.save(); p.translate(center); p.scale(radius, radius);
  const auto ink = petals.isValid() ? petals : color(theme, "blue");
  QPainterPath shape;
  shape.moveTo(-.37, -.80);
  shape.cubicTo(-.40, -.20, -.52, .17, -.98, .48);
  shape.cubicTo(-.73, .70, -.30, .69, -.18, .35);
  shape.cubicTo(-.08, .84, .27, .94, .43, .48);
  shape.cubicTo(.57, .72, .85, .69, 1.00, .40);
  shape.cubicTo(.51, .18, .44, -.28, .36, -.80);
  shape.closeSubpath();
  p.setPen(Qt::NoPen); p.setBrush(ink); p.drawPath(shape);
  auto vein = color(theme, "gold"); vein.setAlpha(180);
  p.setPen(QPen(vein, .028)); p.setBrush(Qt::NoBrush);
  for (double end : {-.48, .06, .60}) {
    QPainterPath line(QPointF(0, -.67)); line.quadTo(QPointF(end * .30, -.10), QPointF(end, .46)); p.drawPath(line);
  }
  p.setBrush(color(theme, "green")); p.setPen(Qt::NoPen);
  p.drawEllipse(QRectF(-.40, -.94, .80, .25));
  p.restore();
}
void lotus(QPainter &p, QPointF base, double radius, const Theme &theme) {
  p.save(); p.translate(base);
  for (int layer = 0; layer < 2; ++layer)
    for (int index = -3; index <= 3; ++index) {
      const double angle = -90 + index * (layer ? 15 : 22);
      const double length = radius * (layer ? 1.45 : 1.20) * (1 - std::abs(index) * .055);
      const auto ink = color(theme, "red").darker(layer ? 100 : 111);
      leaf(p, {0, 0}, polar(angle, length), radius * (layer ? .19 : .24), ink, color(theme, "gold"));
    }
  foliage(p, {-radius * .20, -radius * .03}, {0, radius * .21}, radius * .20, theme);
  foliage(p, {radius * .20, -radius * .03}, {0, radius * .21}, radius * .20, theme);
  p.restore();
}
void sun(QPainter &p, QPointF center, double radius, const Theme &theme) {
  p.save(); p.translate(center);
  p.setPen(Qt::NoPen); p.setBrush(color(theme, "gold"));
  for (int index = 0; index < 24; ++index) {
    const double angle = index * 15;
    QPainterPath ray;
    ray.moveTo(polar(angle - 4.4, radius * .67));
    ray.lineTo(polar(angle, radius * (index % 2 ? .91 : 1.0)));
    ray.lineTo(polar(angle + 4.4, radius * .67)); ray.closeSubpath();
    p.drawPath(ray);
  }
  p.drawEllipse(QPointF(), radius * .60, radius * .60);
  p.setPen(QPen(color(theme, "cream"), radius * .018)); p.setBrush(Qt::NoBrush);
  p.drawEllipse(QPointF(), radius * .56, radius * .56);
  p.restore();
}
void topCorner(QPainter &p, const Theme &theme) {
  QPainterPath branch(QPointF(5, 5));
  branch.cubicTo(40, 39, 61, 66, 115, 62);
  branch.cubicTo(163, 64, 190, 39, 232, 36);
  branch.moveTo(38, 35); branch.cubicTo(24, 65, 31, 88, 44, 103);
  branch.moveTo(114, 62); branch.quadTo(139, 60, 151, 79);
  stem(p, branch, color(theme, "green"), 3.3);
  foliage(p, {30, 28}, {95, 11}, 14, theme);
  foliage(p, {57, 47}, {35, 96}, 13, theme);
  foliage(p, {91, 59}, {112, 18}, 13, theme);
  foliage(p, {121, 62}, {176, 89}, 15, theme);
  foliage(p, {165, 53}, {216, 18}, 13, theme);
  foliage(p, {30, 62}, {11, 100}, 11, theme);
  bell(p, {152, 87}, 20, theme, color(theme, "red"));
  berryBranch(p, {45, 100}, .65, -10, theme);
}
void sunBranch(QPainter &p, const Theme &theme) {
  QPainterPath branch(QPointF(132, 89));
  branch.cubicTo(96, 87, 62, 65, 45, 29);
  branch.moveTo(82, 73); branch.quadTo(80, 47, 101, 42);
  stem(p, branch, color(theme, "green"), 2.8);
  foliage(p, {111, 86}, {66, 99}, 13, theme);
  foliage(p, {77, 69}, {17, 61}, 15, theme);
  foliage(p, {60, 54}, {76, 5}, 13, theme);
  foliage(p, {48, 36}, {14, 17}, 11, theme);
  blossom(p, {43, 20}, 13, theme);
  berryBranch(p, {94, 65}, .65, 37, theme);
}
void bottomCorner(QPainter &p, const Theme &theme) {
  QPainterPath branch(QPointF(17, 230));
  branch.cubicTo(41, 173, 15, 122, 29, 60);
  branch.cubicTo(34, 36, 43, 21, 44, 8);
  branch.moveTo(20, 225); branch.cubicTo(86, 232, 124, 216, 196, 192);
  branch.moveTo(31, 126); branch.cubicTo(79, 95, 101, 98, 168, 118);
  branch.moveTo(122, 215); branch.quadTo(193, 191, 215, 149);
  branch.moveTo(29, 198); branch.quadTo(57, 192, 76, 169);
  stem(p, branch, color(theme, "green"), 3.5);
  foliage(p, {27, 88}, {19, 27}, 16, theme);
  foliage(p, {28, 111}, {81, 56}, 18, theme);
  foliage(p, {30, 143}, {85, 111}, 16, theme);
  foliage(p, {25, 183}, {8, 134}, 12, theme);
  foliage(p, {83, 226}, {139, 235}, 15, theme);
  foliage(p, {122, 216}, {186, 167}, 17, theme);
  foliage(p, {176, 200}, {224, 214}, 13, theme);
  foliage(p, {117, 106}, {138, 73}, 10, theme);
  berryBranch(p, {59, 84}, .95, 35, theme);
  bell(p, {169, 121}, 25, theme);
  blossom(p, {216, 147}, 15, theme);
  poppy(p, {77, 173}, 53, theme);
}
void centerPlantHalf(QPainter &p, const Theme &theme) {
  QPainterPath branch(QPointF(300, 225));
  branch.cubicTo(278, 171, 257, 132, 209, 154);
  branch.cubicTo(163, 179, 133, 149, 100, 119);
  branch.moveTo(300, 225); branch.cubicTo(258, 192, 199, 198, 155, 184);
  branch.moveTo(209, 154); branch.quadTo(153, 123, 108, 93);
  stem(p, branch, color(theme, "green"), 3.2);
  foliage(p, {270, 172}, {244, 102}, 16, theme);
  foliage(p, {248, 150}, {202, 102}, 17, theme);
  foliage(p, {231, 158}, {186, 187}, 14, theme);
  foliage(p, {186, 158}, {153, 109}, 13, theme);
  foliage(p, {158, 151}, {106, 168}, 14, theme);
  foliage(p, {139, 149}, {87, 124}, 13, theme);
  foliage(p, {248, 202}, {215, 225}, 13, theme);
  foliage(p, {202, 197}, {188, 228}, 12, theme);
  foliage(p, {166, 189}, {126, 217}, 12, theme);
  foliage(p, {136, 113}, {119, 65}, 12, theme);
  blossom(p, {107, 91}, 15, theme);
  bell(p, {154, 186}, 17, theme);
  berryBranch(p, {261, 133}, .85, -30, theme);
  berryBranch(p, {184, 163}, .70, -62, theme);
}
void centerPlant(QPainter &p, const Theme &theme) {
  centerPlantHalf(p, theme);
  p.save(); p.translate(600, 0); p.scale(-1, 1); centerPlantHalf(p, theme); p.restore();
  QPainterPath trunk(QPointF(300, 225)); trunk.cubicTo(282, 154, 303, 95, 300, 42);
  stem(p, trunk, color(theme, "green"), 3.7);
  foliage(p, {300, 64}, {300, 6}, 17, theme);
  foliage(p, {299, 125}, {264, 74}, 15, theme);
  foliage(p, {300, 106}, {337, 56}, 15, theme);
  berryBranch(p, {300, 85}, .90, -29, theme);
}
void lowerVine(QPainter &p, QRectF r, double height, double cornerWidth, bool mirror, const Theme &theme) {
  p.save();
  p.translate(mirror ? r.right() : r.left(), r.bottom());
  if (mirror) p.scale(-1, 1);
  QPainterPath vine(QPointF(r.width() * .5, -12));
  vine.cubicTo(r.width() * .37, -height * .49,
               r.width() * .24, -height * .14, cornerWidth * .51, -height * .20);
  stem(p, vine, color(theme, "green"), std::max(1.8, height * .014));
  for (int index = 0; index < 6; ++index) {
    const double progress = .20 + index * .125;
    const QPointF anchor = vine.pointAtPercent(progress);
    auto direction = vine.pointAtPercent(progress + .01) - vine.pointAtPercent(progress - .01);
    const double length = std::hypot(direction.x(), direction.y());
    if (length < .01) continue;
    direction /= length;
    const double reach = height * (index % 2 ? .18 : .23);
    const QPointF end(anchor.x() + direction.x() * reach * .65,
                      std::clamp(anchor.y() - reach * (index % 2 ? -.4 : .8), -height + 8, -7.0));
    foliage(p, anchor, end, reach * .25, theme);
  }
  const auto berries = vine.pointAtPercent(.58);
  berryBranch(p, berries, height / 230, -28, theme);
  p.restore();
}
void inBox(QPainter &p, QRectF box, QSizeF design, bool mirror, const auto &draw) {
  p.save(); p.translate(mirror ? box.right() : box.left(), box.top());
  p.scale((mirror ? -1 : 1) * box.width() / design.width(), box.height() / design.height());
  draw(); p.restore();
}
void faceStem(QPainter &p, const Theme &theme) {
  QPainterPath branch(QPointF(50, 98)); branch.cubicTo(47, 76, 54, 53, 50, 34);
  stem(p, branch, color(theme, "green"), 2.2);
  foliage(p, {50, 88}, {13, 62}, 12, theme);
  foliage(p, {50, 80}, {88, 49}, 13, theme);
}
void leafyFace(QPainter &p, const Theme &theme) {
  QPainterPath branch(QPointF(47, 98)); branch.cubicTo(60, 70, 47, 37, 52, 15);
  stem(p, branch, color(theme, "green"), 2.5);
  foliage(p, {53, 79}, {13, 51}, 13, theme);
  foliage(p, {53, 63}, {91, 32}, 13, theme);
  foliage(p, {52, 46}, {23, 14}, 12, theme);
  foliage(p, {52, 29}, {53, 1}, 10, theme);
  berryBranch(p, {50, 91}, .7, -50, theme);
  blossom(p, {77, 70}, 11, theme);
}
void illustratedFace(QPainter &p, const Card &card, const Theme &theme) {
  if (card.suit == 0) leafyFace(p, theme);
  else if (card.suit == 1) {
    sun(p, {50, 49}, 43, theme);
    foliage(p, {50, 97}, {26, 80}, 6, theme);
    foliage(p, {50, 97}, {74, 80}, 6, theme);
  } else {
    faceStem(p, theme);
    if (card.suit == 2) {
      p.save(); p.translate(50, 37); p.rotate(-12); bell(p, {}, 34, theme); p.restore();
      berryBranch(p, {50, 79}, .60, -55, theme);
    } else if (card.rank % 2) poppy(p, {50, 35}, 37, theme);
    else lotus(p, {50, 57}, 35, theme);
  }
}
void wovenVine(QPainter &p, const QPainterPath &vine, const Theme &theme) {
  const auto cream = color(theme, "cream");
  stem(p, vine, cream, 1.8);
  for (int index = 0; index < 13; ++index) {
    const double progress = .035 + index * .076;
    const QPointF anchor = vine.pointAtPercent(progress);
    auto direction = vine.pointAtPercent(std::min(.999, progress + .012)) - vine.pointAtPercent(std::max(.001, progress - .012));
    const double length = std::hypot(direction.x(), direction.y());
    if (length < .001) continue;
    direction /= length;
    const QPointF normal(-direction.y(), direction.x());
    const double side = index % 2 ? -1 : 1;
    leaf(p, anchor, anchor + direction * 14 + normal * (side * 13), 5.8, cream, color(theme, "blue"));
    leaf(p, anchor + direction * 3, anchor + direction * 9 - normal * (side * 9), 4.0, cream);
  }
}
void backWeave(QPainter &p, const Theme &theme) {
  QPainterPath outer(QPointF(7, 151)); outer.cubicTo(44, 119, -13, 105, 21, 73); outer.cubicTo(48, 44, 1, 24, 20, -8);
  QPainterPath inner(QPointF(44, 151)); inner.cubicTo(9, 119, 66, 101, 37, 73); inner.cubicTo(10, 45, 59, 21, 41, -8);
  wovenVine(p, outer, theme); wovenVine(p, inner, theme);
  p.save(); p.translate(100, 144); p.scale(-1, -1);
  wovenVine(p, outer, theme); wovenVine(p, inner, theme); p.restore();
  poppy(p, {50, 72}, 23, theme);
  poppy(p, {50, 24}, 13, theme);
  poppy(p, {50, 120}, 13, theme);
}
} // namespace
void drawFrame(QPainter &p, QRectF r, const Theme &theme) {
  if (r.isEmpty()) return;
  const double topHeight = std::clamp(r.height() * .14, 56.0, 100.0);
  const double bottomHeight = std::min(r.height() * .30, 250.0);
  const double cornerWidth = std::min(r.width() * .245, 330.0);
  p.save(); p.setRenderHint(QPainter::Antialiasing); p.setClipRect(r);
  lowerVine(p, r, bottomHeight, cornerWidth, false, theme);
  lowerVine(p, r, bottomHeight, cornerWidth, true, theme);
  for (const bool mirror : {false, true}) {
    const QRectF top(mirror ? r.right() - cornerWidth - 8 : r.left() + 8, r.top() + 4, cornerWidth, topHeight - 8);
    inBox(p, top, {240, 110}, mirror, [&] { topCorner(p, theme); });
    const QRectF bottom(mirror ? r.right() - cornerWidth - 6 : r.left() + 6, r.bottom() - bottomHeight - 4, cornerWidth, bottomHeight);
    inBox(p, bottom, {240, 240}, mirror, [&] { bottomCorner(p, theme); });
  }
  const double radius = (topHeight - 10) * .46;
  sun(p, {r.center().x(), r.top() + topHeight * .49}, radius, theme);
  const double wingWidth = std::min(r.width() * .16, 195.0);
  for (const bool mirror : {false, true}) {
    const QRectF wing(mirror ? r.center().x() + radius + 4 : r.center().x() - radius - wingWidth - 4, r.top() + 4, wingWidth, topHeight - 8);
    inBox(p, wing, {140, 104}, mirror, [&] { sunBranch(p, theme); });
  }
  const double plantWidth = std::min(r.width() * .60, 850.0);
  const QRectF plant(r.center().x() - plantWidth / 2, r.bottom() - bottomHeight + 1, plantWidth, bottomHeight - 6);
  inBox(p, plant, {600, 230}, false, [&] { centerPlant(p, theme); });
  p.restore();
}
void drawIllustration(QPainter &p, QRectF r, const Card &card, const Theme &theme) {
  const QRectF body(r.left() + r.width() * .12, r.top() + r.width() * .31,
                    r.width() * .76, r.height() - r.width() * .64);
  inBox(p, body, {100, 100}, false, [&] { illustratedFace(p, card, theme); });
}
void drawBack(QPainter &p, QRectF r, const Theme &theme) {
  const QRectF inner = r.adjusted(r.width() * .065, r.width() * .065, -r.width() * .065, -r.width() * .065);
  p.save(); p.setPen(QPen(color(theme, "cream"), r.width() * .028));
  const auto navy = color(theme, "blue"), base = theme.color("card_back");
  p.setBrush(QColor::fromRgbF(base.redF() * .22 + navy.redF() * .78,
                             base.greenF() * .22 + navy.greenF() * .78,
                             base.blueF() * .22 + navy.blueF() * .78));
  p.drawRoundedRect(inner, 3, 3);
  QPainterPath clip; clip.addRoundedRect(inner.adjusted(2, 2, -2, -2), 2, 2);
  p.setClipPath(clip, Qt::IntersectClip);
  inBox(p, inner, {100, 144}, false, [&] { backWeave(p, theme); });
  p.restore();
}
} // namespace solitare::floral
