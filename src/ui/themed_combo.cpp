#include "themed_combo.h"
#include <QApplication>
#include <QEvent>
#include <QGuiApplication>
#include <QLayout>
#include <QListView>
#include <QPainter>
#include <QPainterPath>
#include <QProxyStyle>
#include <QScreen>
#include <QScrollBar>
#include <QStyleFactory>
#include <QStyledItemDelegate>
#include <algorithm>
namespace solitare {
namespace {
class ComboStyle : public QProxyStyle {
public:
  ComboStyle() : QProxyStyle(QStyleFactory::create("Fusion")) {}
  int styleHint(StyleHint hint, const QStyleOption *option, const QWidget *widget, QStyleHintReturn *data) const override {
    if (hint == SH_ComboBox_Popup) return false;
    if (hint == SH_ComboBox_PopupFrameStyle) return QFrame::NoFrame;
    return QProxyStyle::styleHint(hint, option, widget, data);
  }
};
class ComboRows : public QStyledItemDelegate {
public:
  explicit ComboRows(QObject *parent) : QStyledItemDelegate(parent), theme_(presetTheme("lofi")) {}
  void setTheme(const Theme &theme) { theme_ = theme; }
  QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override {
    return {option.fontMetrics.horizontalAdvance(index.data().toString()) + 28,
            std::max(36, option.fontMetrics.height() + 16)};
  }
  void paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
    p->save();
    const bool selected = option.state & QStyle::State_Selected;
    p->fillRect(option.rect, theme_.color(selected ? "accent" : "surface"));
    p->setPen(theme_.color(selected ? "accent_text" : "text"));
    p->setFont(option.font);
    p->drawText(option.rect.adjusted(12, 0, -12, 0), Qt::AlignVCenter | Qt::AlignLeft, index.data().toString());
    p->restore();
  }
private:
  Theme theme_;
};
}
ThemedCombo::ThemedCombo(QWidget *parent) : QComboBox(parent), theme_(presetTheme("lofi")) {
  auto *comboStyle = new ComboStyle;
  comboStyle->setParent(this);
  setStyle(comboStyle);
  auto *list = new QListView;
  list->setStyle(comboStyle);
  list->setFrameShape(QFrame::NoFrame);
  list->setUniformItemSizes(true);
  list->setTextElideMode(Qt::ElideNone);
  list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  list->setVerticalScrollMode(QAbstractItemView::ScrollPerItem);
  list->setMouseTracking(true);
  list->setItemDelegate(new ComboRows(list));
  setView(list);
  setCursor(Qt::PointingHandCursor);
  setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
  setTheme(theme_);
}
int ThemedCombo::rowHeight() const { return std::max(36, fontMetrics().height() + 16); }
int ThemedCombo::contentWidth() const {
  int result = 0;
  for (int row = 0; row < count(); ++row) result = std::max(result, fontMetrics().horizontalAdvance(itemText(row)));
  return result + 32;
}
QSize ThemedCombo::sizeHint() const { return {std::max(180, contentWidth() + 24), rowHeight()}; }
QSize ThemedCombo::minimumSizeHint() const { return {160, rowHeight()}; }
void ThemedCombo::setTheme(const Theme &theme) {
  theme_ = theme;
  QPalette colors = palette();
  colors.setColor(QPalette::Base, theme.color("surface"));
  colors.setColor(QPalette::Text, theme.color("text"));
  colors.setColor(QPalette::Highlight, theme.color("accent"));
  colors.setColor(QPalette::HighlightedText, theme.color("accent_text"));
  static_cast<ComboRows *>(view()->itemDelegate())->setTheme(theme);
  stylePopup();
  view()->setStyleSheet(QString("QListView {border:none;padding:0px;margin:0px;background:%1;color:%2;selection-background-color:%3;selection-color:%4;}")
      .arg(theme.color("surface").name(QColor::HexArgb), theme.color("text").name(QColor::HexArgb),
           theme.color("accent").name(QColor::HexArgb), theme.color("accent_text").name(QColor::HexArgb)));
  view()->setPalette(colors);
  view()->viewport()->update();
  update();
}
void ThemedCombo::stylePopup() {
  auto *popup = view()->window();
  if (popup == window()) return;
  popup->setObjectName("comboPopup");
  popup->setStyleSheet(QString("QFrame#comboPopup {background:%1;border:1px solid %2;}")
      .arg(theme_.color("surface").name(QColor::HexArgb), theme_.color("border").name(QColor::HexArgb)));
  popup->installEventFilter(this);
}
void ThemedCombo::positionPopup() {
  auto *popup = view()->window();
  const QRect anchor(mapToGlobal(QPoint()), size());
  const auto *screen = QGuiApplication::screenAt(anchor.center());
  if (!screen) screen = QGuiApplication::primaryScreen();
  const QRect available = screen->availableGeometry().adjusted(4, 4, -4, -4);
  const int below = available.bottom() - anchor.bottom() - 3;
  const int above = anchor.top() - available.top() - 3;
  const int rows = std::min(count(), std::max(1, (std::max(above, below) - 2) / rowHeight()));
  const int height = rows * rowHeight() + 2;
  const int scrollWidth = rows < count() ? style()->pixelMetric(QStyle::PM_ScrollBarExtent) : 0;
  const int width = std::min(available.width(), std::max(this->width(), contentWidth() + scrollWidth + 2));
  const int left = std::clamp(anchor.left(), available.left(), available.right() - width + 1);
  const int top = below >= height ? anchor.bottom() + 3 : std::clamp(anchor.top() - height - 3, available.top(), available.bottom() - height + 1);
  if (popup->layout()) {
    popup->layout()->setContentsMargins(1, 1, 1, 1);
    popup->layout()->setSpacing(0);
  }
  view()->setFixedHeight(rows * rowHeight());
  popup->setFixedSize(width, height);
  popup->move(left, top);
  if (popup->layout()) popup->layout()->activate();
  view()->doItemsLayout();
  view()->scrollTo(model()->index(currentIndex(), modelColumn(), rootModelIndex()), QAbstractItemView::EnsureVisible);
}
void ThemedCombo::showPopup() {
  if (!count()) return;
  view()->setFont(font());
  view()->setMinimumWidth(0);
  view()->setMaximumHeight(QWIDGETSIZE_MAX);
  view()->setMinimumHeight(0);
  QComboBox::showPopup();
  stylePopup();
  positionPopup();
  update();
}
void ThemedCombo::hidePopup() { QComboBox::hidePopup(); update(); }
bool ThemedCombo::eventFilter(QObject *object, QEvent *event) {
  if (object == view()->window() && event->type() == QEvent::Hide) update();
  return QComboBox::eventFilter(object, event);
}
void ThemedCombo::paintEvent(QPaintEvent *) {
  QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
  p.setBrush(theme_.color(underMouse() ? "surface_alt" : "surface"));
  p.setPen(QPen(theme_.color(hasFocus() ? "accent" : "border"), hasFocus() ? 2 : 1));
  p.drawRoundedRect(QRectF(rect()).adjusted(1, 1, -1, -1), 5, 5);
  p.setPen(theme_.color(isEnabled() ? "text" : "muted"));
  p.drawText(rect().adjusted(12, 0, -34, 0), Qt::AlignVCenter | Qt::AlignLeft, currentText());
  QPainterPath chevron;
  const QPointF center(width() - 18, height() / 2.0);
  chevron.moveTo(center + QPointF(-4, -2)); chevron.lineTo(center + QPointF(0, 2)); chevron.lineTo(center + QPointF(4, -2));
  p.setBrush(Qt::NoBrush); p.setPen(QPen(theme_.color("text"), 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  p.drawPath(chevron);
}
}
