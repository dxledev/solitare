#include "board.h"
#include "card_art.h"
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <algorithm>
namespace solitare {
Board::Board(Game &game, QWidget *parent)
    : QWidget(parent), game_(game), theme_(presetTheme("lofi")) {
  setObjectName("board");
  setFocusPolicy(Qt::StrongFocus);
  setMouseTracking(true);
  setAccessibleName("Solitare card table");
  setAccessibleDescription("Arrow keys choose a pile; Enter selects or moves; "
                           "Space draws; A sends a card to a foundation.");
}
void Board::setTheme(const Theme &theme) {
  if (theme_ == theme) return;
  theme_ = theme;
  artwork_.setTheme(theme);
  decoration_ = QPixmap();
  rendered_.reset();
  update();
}
void Board::paintPile(QPainter &p, Location loc,
                      const std::vector<Card> &cards) {
  if (isLayoutGame() && loc.kind == Location::Tableau && cards.empty()) return;
  auto r = cardRect({loc.kind, loc.pile, 0});
  p.setPen(QPen(theme_.color("border"), 1, Qt::DashLine));
  p.setBrush(Qt::NoBrush);
  p.drawRoundedRect(r, 7, 7);
  if (loc.kind == Location::Foundation) {
    QFont mark = font();
    mark.setPixelSize(qRound(r.width() * .32));
    p.setFont(mark);
    p.setPen(theme_.color("muted"));
    p.drawText(r, Qt::AlignCenter, game_.state().mode == GameMode::Spider ? "K–A" : "A");
  }
  if (loc.kind == Location::Stock && cards.empty()) {
    p.setPen(theme_.color("muted"));
    p.drawText(r, Qt::AlignCenter,
               game_.state().mode == GameMode::Spider || game_.state().mode == GameMode::TriPeaks || game_.state().waste.empty() ? "Empty" : "Recycle");
  }
  const int start =
      loc.kind == Location::Tableau ? 0 : std::max(0, int(cards.size()) - 1);
  for (int i = start; i < int(cards.size()); ++i) {
    Location current{loc.kind, loc.pile, i};
    bool select = selected_ && selected_->kind == loc.kind &&
                  selected_->pile == loc.pile &&
                  (selected_->index < 0 || i >= selected_->index);
    if (dragging_ && select)
      continue;
    bool hint = hintFrom_ && hintFrom_->kind == loc.kind &&
                hintFrom_->pile == loc.pile &&
                (hintFrom_->index < 0 || i == hintFrom_->index);
    auto visibleCard = cards[i];
    if (isLayoutGame() && loc.kind == Location::Tableau && !game_.exposed(current))
      visibleCard.faceUp = false;
    artwork_.paintCard(p, cardRect(current), visibleCard, select || hint, devicePixelRatioF());
  }
  if (hintTo_ && hintTo_->kind == loc.kind && hintTo_->pile == loc.pile) {
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(theme_.color("accent"), 3));
    p.drawRoundedRect(cardRect({loc.kind, loc.pile, -1}).adjusted(-3, -3, 3, 3),
                      9, 9);
  }
}
void Board::paintEvent(QPaintEvent *) {
  ensureLayers();
  QPainter p(this);
  p.drawPixmap(QPointF(), tableLayer_);
  if (dragging_ && selected_ && !dragLayer_.isNull())
    p.drawPixmap(dragSourceBounds_.topLeft() + dragPoint_ - pressPoint_, dragLayer_);
}
void Board::paintLabels(QPainter &p) {
  const auto &state = game_.state();
  p.setPen(theme_.color("muted"));
  QFont small = font();
  small.setPixelSize(11);
  p.setFont(small);
  const auto r = cardRect({state.mode == GameMode::FreeCell ? Location::Cell : Location::Stock, 0, -1});
  QString label;
  if (state.mode == GameMode::FreeCell) label = "Four free cells";
  else if (state.mode == GameMode::Spider) label = QString("%1 deals left").arg(state.stock.size() / 10);
  else if (state.mode == GameMode::Pyramid) label = QString("%1 left · %2 redeals remaining").arg(state.stock.size()).arg(state.options.pyramidRedeals - state.redeals);
  else if (state.mode == GameMode::TriPeaks) label = QString("%1 stock cards left").arg(state.stock.size());
  else label = QString("Draw %1 · %2 left").arg(state.drawCount).arg(state.stock.size());
  p.drawText(QRectF(r.left(), r.bottom() + 8, width() - r.left() - 20, 24), Qt::AlignLeft, label);
}
void Board::resetInteraction() {
  selected_.reset();
  pressed_.reset();
  hintFrom_.reset();
  hintTo_.reset();
  dragging_ = false;
  keyboardPile_ = std::clamp(keyboardPile_, 0, std::max(0, int(focusPiles().size()) - 1));
  update();
}
void Board::finishMove(bool moved) {
  if (moved) {
    resetInteraction();
    emit changed();
  } else update();
}
void Board::activate(Location loc) {
  hintFrom_.reset();
  hintTo_.reset();
  if (loc.kind == Location::Stock) {
    finishMove(game_.draw());
    return;
  }
  if (selected_ && game_.move(*selected_, loc)) {
    finishMove(true);
    return;
  }
  if (isLayoutGame() && game_.activate(loc)) {
    finishMove(true);
    return;
  }
  if (game_.canSelect(loc)) {
    if (loc.index < 0) loc.index = int(pileCards(loc)->size()) - 1;
    selected_ = loc;
  }
  else selected_.reset();
  update();
}
void Board::mousePressEvent(QMouseEvent *event) {
  if (event->button() != Qt::LeftButton) return;
  setFocus();
  pressed_ = hit(event->position());
  pressPoint_ = event->position();
  dragPoint_ = pressPoint_;
}
void Board::mouseMoveEvent(QMouseEvent *event) {
  if (pressed_ && pressed_->kind != Location::Stock && (event->buttons() & Qt::LeftButton)) {
    const bool startDrag = !dragging_ && (event->position() - pressPoint_).manhattanLength() > 8 && game_.canSelect(*pressed_);
    if (startDrag) { selected_ = pressed_; dragging_ = true; }
    if (dragging_) {
      const auto previous = dragBounds(dragPoint_);
      dragPoint_ = event->position();
      setCursor(Qt::ClosedHandCursor);
      if (startDrag) update();
      else update(previous.united(dragBounds(dragPoint_)).toAlignedRect());
      return;
    }
  }
  const auto target = hit(event->position());
  const bool actionable = target && (target->kind == Location::Stock || game_.canSelect(*target) || (selected_ && game_.canMove(*selected_, *target)));
  setCursor(actionable ? Qt::PointingHandCursor : Qt::ArrowCursor);
}
void Board::mouseReleaseEvent(QMouseEvent *event) {
  if (event->button() != Qt::LeftButton) return;
  const auto target = hit(event->position());
  const bool wasDrag = dragging_;
  dragging_ = false;
  if (wasDrag) {
    if (target && selected_) finishMove(game_.move(*selected_, *target));
  } else if (target && pressed_ && *target == *pressed_) activate(*target);
  pressed_.reset();
  update();
}
void Board::mouseDoubleClickEvent(QMouseEvent *event) {
  if (event->button() != Qt::LeftButton) return;
  pressed_.reset();
  if (auto loc = hit(event->position())) finishMove(game_.autoMove(*loc));
}
void Board::focusOutEvent(QFocusEvent *event) {
  dragging_ = false;
  pressed_.reset();
  QWidget::focusOutEvent(event);
  update();
}
void Board::keyPressEvent(QKeyEvent *event) {
  const auto locations = focusPiles();
  if (locations.empty()) return;
  keyboardPile_ = std::clamp(keyboardPile_, 0, int(locations.size()) - 1);
  const auto loc = locations[keyboardPile_];
  const int key = event->key();
  if (key == Qt::Key_Right || key == Qt::Key_Left) {
    keyboardPile_ = (keyboardPile_ + (key == Qt::Key_Right ? 1 : int(locations.size()) - 1)) % int(locations.size());
  } else if (key == Qt::Key_Up || key == Qt::Key_Down) {
    bool changedSelection = false;
    if (!isLayoutGame() && selected_ && selected_->kind == Location::Tableau && loc.kind == Location::Tableau && selected_->pile == loc.pile) {
      auto next = *selected_;
      const auto *cards = pileCards(next);
      if (next.index < 0) next.index = int(cards->size()) - 1;
      next.index += key == Qt::Key_Up ? -1 : 1;
      if (next.index >= 0 && next.index < int(cards->size()) && game_.canSelect(next)) {
        selected_ = next;
        changedSelection = true;
      }
    }
    if (!changedSelection) {
      const int jump = isLayoutGame() ? 1 : int(game_.state().tableau.size());
      keyboardPile_ = std::clamp(keyboardPile_ + (key == Qt::Key_Up ? -jump : jump), 0, int(locations.size()) - 1);
    }
  } else if (key == Qt::Key_Return || key == Qt::Key_Enter) activate(loc);
  else if (key == Qt::Key_Space) finishMove(game_.draw());
  else if (key == Qt::Key_A) finishMove(game_.autoMove(selected_.value_or(loc)));
  else if (key == Qt::Key_Escape) resetInteraction();
  else { QWidget::keyPressEvent(event); return; }
  update();
  event->accept();
}
void Board::showHint() {
  resetInteraction();
  const auto hint = game_.hint();
  if (hint) {
    hintFrom_ = hint->first;
    hintTo_ = hint->second;
    emit message(hint->first.kind == Location::Stock ? "Draw from the highlighted stock." : "Try the highlighted cards.");
  } else emit message("No moves available. Undo a move or start a new deal.");
  update();
}
} // namespace solitare
