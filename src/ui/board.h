#pragma once
#include "core/game.h"
#include "core/theme.h"
#include "board_art_cache.h"
#include <QWidget>
#include <optional>
namespace solitare {
class Board : public QWidget {
  Q_OBJECT
public:
  explicit Board(Game &game, QWidget *parent = nullptr);
  void setTheme(const Theme &theme);
  void resetInteraction();
  void showHint();
  QRectF cardRect(Location location) const;
  std::optional<Location> hit(QPointF point) const;
  int artworkRasterizations() const { return artwork_.rasterizations(); }
  int layerBuilds() const { return layerBuilds_; }
signals:
  void changed();
  void message(const QString &text);

protected:
  void paintEvent(QPaintEvent *) override;
  void mousePressEvent(QMouseEvent *) override;
  void mouseMoveEvent(QMouseEvent *) override;
  void mouseReleaseEvent(QMouseEvent *) override;
  void mouseDoubleClickEvent(QMouseEvent *) override;
  void keyPressEvent(QKeyEvent *) override;
  void focusOutEvent(QFocusEvent *) override;

private:
  Game &game_;
  Theme theme_;
  std::optional<Location> selected_, pressed_, hintFrom_, hintTo_;
  QPointF pressPoint_, dragPoint_;
  bool dragging_ = false;
  int keyboardPile_ = 0;
  struct RenderState {
    State game;
    QSize size;
    qreal dpr;
    std::optional<Location> selected, hintFrom, hintTo;
    bool dragging, focus;
    int keyboardPile;
    bool operator==(const RenderState &) const = default;
  };
  BoardArtCache artwork_;
  QPixmap decoration_, tableLayer_, dragLayer_;
  QSize decorationSize_;
  qreal decorationDpr_ = 0;
  std::optional<RenderState> rendered_;
  QRectF dragSourceBounds_;
  int layerBuilds_ = 0;
  void ensureLayers();
  void paintTable(QPainter &p);
  void paintDecoration(QPainter &p);
  void buildDragLayer();
  QRectF dragBounds(QPointF point) const;
  double topInset() const;
  double bottomInset() const;
  double cardWidth() const;
  std::vector<Location> focusPiles() const;
  const std::vector<Card> *pileCards(Location location) const;
  bool isLayoutGame() const;
  void paintLabels(QPainter &p);
  double tableauStep(int pile) const;
  void activate(Location location);
  void finishMove(bool moved);
  void paintPile(QPainter &p, Location location,
                 const std::vector<Card> &cards);
};
} // namespace solitare
