#include "modal.h"
#include "modal_motion.h"
#include "modal_backdrop.h"
#include <QTimer>
#include "themed_combo.h"
#include <QApplication>
#include <QComboBox>
#include <QFormLayout>
#include <QGraphicsBlurEffect>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <algorithm>
namespace solitare {
Modal::Modal(QWidget *parent, QWidget *background)
    : QWidget(parent), background_(background), theme_(presetTheme("lofi")) {
  setObjectName("modal");
  setFocusPolicy(Qt::StrongFocus);
  setAttribute(Qt::WA_NoSystemBackground);
  if (ModalBackdrop::available()) {
    backdrop_ = new ModalBackdrop(this);
    backdrop_->setGeometry(rect());
    parent->installEventFilter(this);
  }
  card_ = new QWidget(this);
  card_->setObjectName("modalCard");
  motion_ = new ModalMotion(card_);
  card_->setGraphicsEffect(motion_);
  connect(motion_, &ModalMotion::progressChanged, this, [this](qreal progress) {
    if (backdrop_) backdrop_->setProgress(progress);
    if (!backdrop_ || !backdrop_->isValid()) update();
  });
  connect(motion_, &ModalMotion::finished, this, [this](bool open) { if (!open && closing_) finishDismiss(); });
  auto *layout = new QVBoxLayout(card_);
  layout->setContentsMargins(28, 28, 28, 28);
  layout->setSpacing(16);
  title_ = new QLabel(card_);
  title_->setObjectName("modalTitle");
  title_->setWordWrap(true);
  layout->addWidget(title_);
  bodyScroll_ = new QScrollArea(card_);
  bodyScroll_->setWidgetResizable(true);
  bodyScroll_->setFrameShape(QFrame::NoFrame);
  body_ = new QLabel;
  body_->setWordWrap(true);
  body_->setAlignment(Qt::AlignTop);
  bodyScroll_->setWidget(body_);
  body_->setAutoFillBackground(false);
  bodyScroll_->viewport()->setAutoFillBackground(false);
  layout->addWidget(bodyScroll_);
  dealControls_ = new QWidget(card_);
  auto *form = new QFormLayout(dealControls_);
  form->setContentsMargins(0, 0, 0, 0);
  form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  mode_ = new ThemedCombo(dealControls_);
  mode_->setObjectName("gameMode");
  for (auto mode : {GameMode::Klondike, GameMode::Spider, GameMode::FreeCell, GameMode::Pyramid, GameMode::TriPeaks})
    mode_->addItem(modeName(mode), int(mode));
  form->addRow("Game", mode_);
  variant_ = new ThemedCombo(dealControls_);
  variant_->setObjectName("gameVariant");
  variantLabel_ = new QLabel("Rules", dealControls_);
  form->addRow(variantLabel_, variant_);
  difficulty_ = new ThemedCombo(dealControls_);
  difficulty_->setObjectName("gameDifficulty");
  difficulty_->addItem("Casual", int(Difficulty::Casual));
  difficulty_->addItem("Standard", int(Difficulty::Standard));
  difficulty_->addItem("Expert", int(Difficulty::Expert));
  form->addRow("Difficulty", difficulty_);
  connect(mode_, &QComboBox::currentIndexChanged, this, [this] { updateVariants(); centerCard(); });
  layout->addWidget(dealControls_);
  dealControls_->hide();
  for (int i = 0; i < 3; ++i) {
    auto *button = new QPushButton(card_);
    button->setObjectName(QString("modalAction%1").arg(i));
    button->setCursor(Qt::PointingHandCursor);
    layout->addWidget(button);
    buttons_.append(button);
    connect(button, &QPushButton::clicked, this, [this, i] {
      beginDismiss(i);
    });
  }
  for (auto *child : card_->findChildren<QWidget *>()) child->installEventFilter(this);
  card_->installEventFilter(this);
  hide();
}
void Modal::setTheme(const Theme &theme) {
  theme_ = theme;
  mode_->setTheme(theme);
  variant_->setTheme(theme);
  difficulty_->setTheme(theme);
  if (isVisible())
    refreshSnapshot();
  update();
}
void Modal::open(const QString &title, const QString &body,
                 const QStringList &actions) {
  const bool wasVisible = isVisible();
  if (!wasVisible) {
    previousFocus_ = QApplication::focusWidget();
    backgroundWasEnabled_ = background_ && background_->isEnabled();
    backgroundUpdatesWereEnabled_ = background_ && background_->updatesEnabled();
    motion_->reset();
  }
  closing_ = false;
  pendingChoice_ = -1;
  card_->setAttribute(Qt::WA_TransparentForMouseEvents, false);
  dealControls_->hide();
  title_->setText(title);
  body_->setText(body);
  for (int i = 0; i < buttons_.size(); ++i) {
    buttons_[i]->setVisible(i < actions.size());
    if (i < actions.size())
      buttons_[i]->setText(actions[i]);
  }
  setGeometry(parentWidget()->rect());
  centerCard();
  refreshSnapshot();
  if (background_) background_->setEnabled(false);
  freezeBackground();
  show();
  raise();
  for (auto *button : buttons_)
    if (button->isVisible()) {
      button->setFocus();
      break;
    }
  motion_->animate(true);
}
void Modal::openDeal(GameMode mode, const GameOptions &options) {
  open("Choose a new deal", "Your current deal will be replaced. Pick a game and its rules.", {"Start deal", "Keep playing"});
  mode_->setCurrentIndex(mode_->findData(int(mode)));
  updateVariants();
  int value = 0;
  if (mode == GameMode::Klondike) value = options.drawCount;
  if (mode == GameMode::Spider) value = options.spiderSuits;
  if (mode == GameMode::Pyramid) value = options.pyramidRedeals;
  if (mode == GameMode::TriPeaks) value = options.tripeaksWraparound;
  variant_->setCurrentIndex(std::max(0, variant_->findData(value)));
  difficulty_->setCurrentIndex(std::max(0, difficulty_->findData(int(options.difficulty))));
  dealControls_->show();
  centerCard();
  mode_->setFocus();
}
GameMode Modal::selectedMode() const { return GameMode(mode_->currentData().toInt()); }
GameOptions Modal::selectedOptions() const {
  GameOptions options;
  options.difficulty = Difficulty(difficulty_->currentData().toInt());
  const int value = variant_->currentData().toInt();
  switch (selectedMode()) {
  case GameMode::Klondike: options.drawCount = value; break;
  case GameMode::Spider: options.spiderSuits = value; break;
  case GameMode::Pyramid: options.pyramidRedeals = value; break;
  case GameMode::TriPeaks: options.tripeaksWraparound = value; break;
  case GameMode::FreeCell: break;
  }
  return options;
}
void Modal::updateVariants() {
  variant_->clear();
  const auto mode = selectedMode();
  if (mode == GameMode::Klondike) { variant_->addItem("Draw one", 1); variant_->addItem("Draw three", 3); }
  if (mode == GameMode::Spider) { variant_->addItem("One suit", 1); variant_->addItem("Two suits", 2); variant_->addItem("Four suits", 4); }
  if (mode == GameMode::Pyramid) { variant_->addItem("No redeals", 0); variant_->addItem("One redeal", 1); variant_->addItem("Two redeals", 2); }
  if (mode == GameMode::TriPeaks) { variant_->addItem("Standard (ace and king do not connect)", 0); variant_->addItem("Wraparound (ace and king connect)", 1); }
  variant_->setVisible(mode != GameMode::FreeCell);
  variantLabel_->setVisible(mode != GameMode::FreeCell);
}
void Modal::centerCard() {
  card_->setFixedWidth(std::min(470, std::max(200, width() - 40)));
  const int controlsHeight = dealControls_->isHidden() ? 0 : dealControls_->sizeHint().height() + 16;
  body_->setMinimumHeight(0);
  const int bodyHeight =
      std::max(30, body_->heightForWidth(card_->width() - 72));
  int visibleButtons = 0;
  for (auto *button : buttons_)
    if (!button->isHidden())
      ++visibleButtons;
  bodyScroll_->setFixedHeight(std::clamp(
      bodyHeight, 30, std::max(30, height() - 140 - visibleButtons * 58 - controlsHeight)));
  body_->setMinimumHeight(bodyHeight);
  card_->layout()->activate();
  card_->adjustSize();
  card_->move((width() - card_->width()) / 2, (height() - card_->height()) / 2);
}
void Modal::refreshSnapshot() {
  if (!background_ || background_->size().isEmpty())
    return;
  if (background_->layout())
    background_->layout()->activate();
  if (backgroundFrozen_ && backgroundUpdatesWereEnabled_) background_->setUpdatesEnabled(true);
  auto captured = background_->grab();
  if (backgroundFrozen_) background_->setUpdatesEnabled(false);
  backgroundFrame_ = captured;
  if (captured.isNull())
    return;
  QSize small =
      (captured.deviceIndependentSize().toSize() / 3).expandedTo(QSize(1, 1));
  if (small.width() > 640 || small.height() > 640)
    small.scale(640, 640, Qt::KeepAspectRatio);
  auto source =
      captured.scaled(small, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
          .toImage()
          .convertToFormat(QImage::Format_RGB32);
  if (source.isNull() || source.width() < 1 || source.height() < 1)
    return;
  constexpr int padding = 32;
  QImage extended(source.size() + QSize(padding * 2, padding * 2),
                  QImage::Format_RGB32);
  if (extended.isNull())
    return;
  for (int y = 0; y < extended.height(); ++y) {
    const auto *row = reinterpret_cast<const QRgb *>(
        source.constScanLine(std::clamp(y - padding, 0, source.height() - 1)));
    auto *out = reinterpret_cast<QRgb *>(extended.scanLine(y));
    std::fill_n(out, padding, row[0]);
    std::copy_n(row, source.width(), out + padding);
    std::fill_n(out + padding + source.width(), padding,
                row[source.width() - 1]);
  }
  QGraphicsScene scene;
  auto *item = scene.addPixmap(QPixmap::fromImage(extended));
  auto *blur = new QGraphicsBlurEffect;
  blur->setBlurRadius(5);
  blur->setBlurHints(QGraphicsBlurEffect::QualityHint);
  item->setGraphicsEffect(blur);
  QPixmap result(small);
  result.fill(Qt::transparent);
  QPainter p(&result);
  scene.render(&p, QRectF(QPointF(), small),
               QRectF(QPointF(padding, padding), small));
  p.end();
  composeSnapshot(result);
  update();
}
void Modal::composeSnapshot(const QPixmap &blurred) {
  const auto dpr = devicePixelRatioF();
  snapshot_ = QPixmap(QSize(qCeil(width() * dpr), qCeil(height() * dpr)));
  snapshot_.setDevicePixelRatio(dpr);
  snapshot_.fill(Qt::transparent);
  QPainter p(&snapshot_);
  p.setRenderHint(QPainter::SmoothPixmapTransform);
  p.drawPixmap(rect(), blurred);
  auto tint = theme_.color("background");
  tint.setAlphaF(.7);
  p.fillRect(rect(), tint);
  p.end();
  if (backdrop_) backdrop_->setFrames(backgroundFrame_, snapshot_);
}
void Modal::freezeBackground() {
  if (!background_ || backgroundFrozen_) return;
  background_->setUpdatesEnabled(false);
  backgroundFrozen_ = true;
}
void Modal::restoreBackgroundUpdates() {
  if (background_ && backgroundFrozen_) background_->setUpdatesEnabled(backgroundUpdatesWereEnabled_);
  backgroundFrozen_ = false;
}
void Modal::dismiss() { beginDismiss(-1); }
void Modal::beginDismiss(int choice) {
  if (!isVisible() || closing_) return;
  closing_ = true;
  pendingChoice_ = choice;
  mode_->hidePopup();
  variant_->hidePopup();
  difficulty_->hidePopup();
  card_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
  motion_->animate(false);
}
void Modal::finishDismiss() {
  const int choice = pendingChoice_;
  pendingChoice_ = -1;
  closing_ = false;
  hide();
  if (background_) background_->setEnabled(backgroundWasEnabled_);
  restoreBackgroundUpdates();
  if (previousFocus_ && previousFocus_->isEnabled() && previousFocus_->isVisible()) previousFocus_->setFocus();
  previousFocus_.clear();
  if (choice >= 0) emit chosen(choice);
  emit dismissed();
}
bool Modal::eventFilter(QObject *object, QEvent *event) {
  if (backdrop_ && object == parentWidget() && event->type() == QEvent::Show)
    QTimer::singleShot(0, this, [this] { backdrop_->prewarm(); });
  if (closing_ && (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease ||
                   event->type() == QEvent::Wheel || event->type() == QEvent::ShortcutOverride)) return true;
  return QWidget::eventFilter(object, event);
}
bool Modal::event(QEvent *event) {
  const bool handled = QWidget::event(event);
  if (event->type() == QEvent::DevicePixelRatioChange && isVisible()) refreshSnapshot();
  return handled;
}
void Modal::paintEvent(QPaintEvent *) {
  if (backdrop_ && backdrop_->isValid() && backdrop_->hasFrames()) return;
  QPainter p(this);
  p.setCompositionMode(QPainter::CompositionMode_Source);
  p.drawPixmap(QPointF(), backgroundFrame_);
  p.setCompositionMode(QPainter::CompositionMode_SourceOver);
  p.setOpacity(motion_->progress());
  p.drawPixmap(QPointF(), snapshot_);
}
void Modal::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  if (backdrop_) backdrop_->setGeometry(rect());
  centerCard();
  if (isVisible())
    refreshSnapshot();
}
void Modal::hideEvent(QHideEvent *event) {
  snapshot_ = QPixmap();
  backgroundFrame_ = QPixmap();
  if (backdrop_) backdrop_->clearFrames();
  restoreBackgroundUpdates();
  QWidget::hideEvent(event);
}
void Modal::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Escape) {
    dismiss();
    event->accept();
  } else
    QWidget::keyPressEvent(event);
}
void Modal::mousePressEvent(QMouseEvent *event) {
  if (!closing_ && !card_->geometry().contains(event->position().toPoint()))
    dismiss();
  event->accept();
}
} // namespace solitare
