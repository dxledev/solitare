#include "window.h"
#include "card_art.h"
#include <QApplication>
#include <QCloseEvent>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QLabel>
#include <QPushButton>
#include <QRandomGenerator>
#include <QSaveFile>
#include <QShortcut>
#include <QVBoxLayout>
#include <algorithm>
namespace solitare {
namespace {
QString css(QColor color) {
  return QString("rgba(%1,%2,%3,%4)")
      .arg(color.red())
      .arg(color.green())
      .arg(color.blue())
      .arg(color.alpha());
}
QPushButton *button(QString text, QString name, QWidget *parent) {
  auto *result = new QPushButton(text, parent);
  result->setObjectName(name);
  result->setCursor(Qt::PointingHandCursor);
  return result;
}
} // namespace
Window::Window(QString directory, Theme theme, QWidget *parent)
    : QWidget(parent), configDirectory_(std::move(directory)) {
  setWindowTitle("Solitare");
  setObjectName("solitareWindow");
  setAttribute(Qt::WA_TranslucentBackground);
  setMinimumSize(700, 580);
  resize(1120, 800);
  setFont(QFont("Noto Sans", 10));
  buildContent();
  modal_ = new Modal(this, content_);
  watcher_ = new ThemeWatcher(configDirectory_ + "/theme.json",
                              std::move(theme), this);
  connectActions();
  load();
  applyTheme();
  refresh();
  board_->setFocus();
  connect(&timer_, &QTimer::timeout, this, [this] {
    if (isActiveWindow() && !modal_->isVisible() && !game_.won()) {
      ++seconds_;
      refresh();
      if (seconds_ % 15 == 0)
        save();
    }
  });
  timer_.start(1000);
}
void Window::buildContent() {
  auto *outer = new QVBoxLayout(this);
  outer->setContentsMargins(0, 0, 0, 0);
  content_ = new QWidget(this);
  content_->setObjectName("content");
  outer->addWidget(content_);
  auto *layout = new QVBoxLayout(content_);
  layout->setContentsMargins(24, 20, 24, 16);
  layout->setSpacing(12);
  auto *header = new QHBoxLayout;
  header->setSpacing(10);
  auto *title = new QLabel("Solitare", content_);
  title->setObjectName("brand");
  header->addWidget(title);
  header->addStretch();
  undo_ = button("Undo", "undo", content_);
  header->addWidget(undo_);
  hint_ = button("Hint", "hint", content_);
  header->addWidget(hint_);
  header->addWidget(button("How to play", "help", content_));
  header->addWidget(button("New deal", "newDeal", content_));
  layout->addLayout(header);
  details_ = new QLabel(content_);
  details_->setObjectName("details");
  details_->setWordWrap(true);
  layout->addWidget(details_);
  board_ = new Board(game_, content_);
  layout->addWidget(board_, 1);
  status_ = new QLabel("Take your time. Every card has its place.", content_);
  status_->setObjectName("status");
  status_->setWordWrap(true);
  layout->addWidget(status_);
}
void Window::connectActions() {
  connect(board_, &Board::changed, this, [this] {
    refresh();
    save();
    if (game_.won())
      openDialog(Dialog::Won);
  });
  connect(board_, &Board::message, status_, &QLabel::setText);
  connect(undo_, &QPushButton::clicked, this, &Window::undo);
  connect(hint_, &QPushButton::clicked, board_, &Board::showHint);
  connect(findChild<QPushButton *>("newDeal"), &QPushButton::clicked, this,
          [this] { openDialog(Dialog::NewGame); });
  connect(findChild<QPushButton *>("help"), &QPushButton::clicked, this,
          [this] { openDialog(Dialog::Help); });
  connect(watcher_, &ThemeWatcher::changed, this, &Window::applyTheme);
  connect(watcher_, &ThemeWatcher::rejected, this,
          [this](const QString &error) {
            status_->setText("Theme unchanged: " + error);
          });
  connect(modal_, &Modal::chosen, this, [this](int index) {
    if (dialog_ == Dialog::NewGame && index == 0)
      newDeal(modal_->selectedMode(), modal_->selectedOptions());
    if (dialog_ == Dialog::Won && index == 0)
      newDeal(game_.state().mode, game_.state().options);
    dialog_ = Dialog::None;
  });
  auto shortcut = [this](QKeySequence keys, auto action) {
    auto *key = new QShortcut(keys, this);
    connect(key, &QShortcut::activated, this, [this, action] {
      if (!modal_->isVisible())
        action();
    });
  };
  shortcut(QKeySequence::Undo, [this] { undo(); });
  shortcut(QKeySequence("H"), [this] { board_->showHint(); });
  shortcut(QKeySequence("Ctrl+N"), [this] { openDialog(Dialog::NewGame); });
  shortcut(QKeySequence("F1"), [this] { openDialog(Dialog::Help); });
}
void Window::applyTheme() {
  const auto &theme = watcher_->theme();
  auto c = [&](const char *key) { return css(theme.color(key)); };
  setStyleSheet(
      QString("QWidget#solitareWindow {background:transparent;} "
              "QWidget#content {background:%1;}"
              "QWidget {color:%2;font-family:'Noto Sans';font-size:13px;} "
              "QLabel {background:transparent;}"
              "QLabel#brand {font-size:26px;font-weight:600;} "
              "QLabel#details,QLabel#status {color:%3;font-size:12px;}"
              "QPushButton {background:%4;border:1px solid "
              "%5;border-radius:7px;padding:9px 14px;}"
              "QPushButton:hover {background:%6;border-color:%7;} "
              "QPushButton:focus {border:2px solid %7;padding:8px 13px;}"
              "QPushButton:disabled {color:%3;border-color:%4;} "
              "QPushButton#newDeal {background:%7;color:%8;border-color:%7;}"
              "QWidget#modalCard {background:%4;border:1px solid "
              "%5;border-radius:14px;}"
              "QWidget#modalCard QLabel {border:none;} QLabel#modalTitle "
              "{font-size:24px;font-weight:600;}"
              "QWidget#modalCard QPushButton {min-height:20px;}"
)
          .arg(c("background"), c("text"), c("muted"), c("surface"),
               c("border"), c("surface_alt"), c("accent"), c("accent_text")));
  board_->setTheme(theme);
  modal_->setTheme(theme);
  QPixmap icon(64, 64);
  icon.fill(Qt::transparent);
  QPainter p(&icon);
  p.setRenderHint(QPainter::Antialiasing);
  drawSuit(p, QRectF(8, 8, 48, 48), 2, theme.color("accent"));
  p.end();
  setWindowIcon(QIcon(icon));
}
void Window::refresh() {
  int home = 0;
  for (const auto &pile : game_.state().foundations)
    home += int(pile.size());
  const auto &state = game_.state();
  QString rules;
  if (state.mode == GameMode::Klondike) rules = QString("Draw %1").arg(state.drawCount);
  if (state.mode == GameMode::Spider) rules = QString("%1 suit%2").arg(state.options.spiderSuits).arg(state.options.spiderSuits == 1 ? "" : "s");
  if (state.mode == GameMode::FreeCell) rules = "Four free cells";
  if (state.mode == GameMode::Pyramid) rules = QString("%1 redeals").arg(state.options.pyramidRedeals);
  if (state.mode == GameMode::TriPeaks) rules = state.options.tripeaksWraparound ? "Wraparound" : "Standard";
  const QString progress = state.mode == GameMode::Pyramid || state.mode == GameMode::TriPeaks
      ? QString("%1 / 28 cleared").arg(28 - std::count_if(state.tableau.begin(), state.tableau.end(), [](const auto &pile) { return !pile.empty(); }))
      : QString("%1 / %2 home").arg(home).arg(state.mode == GameMode::Spider ? 104 : 52);
  details_->setText(QString("%1 / %2     %3 moves     %4:%5     %6")
      .arg(modeName(state.mode), rules).arg(state.moves).arg(seconds_ / 60)
      .arg(seconds_ % 60, 2, 10, QChar('0')).arg(progress));
  undo_->setEnabled(game_.canUndo());
  const int undos = game_.undosLeft();
  undo_->setText(undos > 0 ? QString("Undo (%1)").arg(undos) : "Undo");
  const int hints = game_.hintsLeft();
  const bool expert = state.options.difficulty == Difficulty::Expert;
  undo_->setVisible(!expert);
  hint_->setVisible(!expert);
  hint_->setEnabled(hints != 0);
  if (state.hintEarned && !hintEarned_)
    status_->setText("You've earned a hint.");
  hintEarned_ = state.hintEarned;
}
void Window::undo() {
  if (game_.undo()) {
    board_->resetInteraction();
    refresh();
    save();
    status_->setText("Move undone.");
  }
}
void Window::newDeal(int drawCount) {
  GameOptions options;
  options.drawCount = drawCount;
  newDeal(GameMode::Klondike, options);
}
void Window::newDeal(GameMode mode, const GameOptions &options) {
  game_.newGame(QRandomGenerator::global()->generate(), mode, options);
  seconds_ = 0;
  hintEarned_ = false;
  gameFileInvalid_ = false;
  board_->resetInteraction();
  refresh();
  save();
  status_->setText("A fresh deck. Take your time.");
}
void Window::openDialog(Dialog dialog) {
  if (modal_->isVisible())
    return;
  dialog_ = dialog;
  board_->resetInteraction();
  if (dialog == Dialog::NewGame)
    modal_->openDeal(game_.state().mode, game_.state().options);
  if (dialog == Dialog::Help) {
    QString rules;
    switch (game_.state().mode) {
    case GameMode::Klondike:
      rules = "Build four foundations from ace to king by suit. Stack cards in descending order with alternating colors. Only kings fill empty columns. Draw one or three cards; stock recycling is unlimited.";
      break;
    case GameMode::Spider:
      rules = "Build descending same-suit runs from king to ace to complete all eight runs. You may stack descending cards of any suit, but only a same-suit run moves together. Any card fills an empty column. Stock deals one card to each column; fill every empty column before dealing.";
      break;
    case GameMode::FreeCell:
      rules = "Build four foundations from ace to king by suit. Stack descending cards with alternating colors. Each of the four free cells holds one card. Empty columns accept any card. The number of empty cells and columns limits how many cards can move together.";
      break;
    case GameMode::Pyramid:
      rules = "Clear all 28 pyramid cards by pairing exposed cards that add to 13. Aces count as 1, jacks 11 and queens 12; click an exposed king to remove it alone. A card is exposed when both cards below it are gone. Pair exposed pyramid cards with each other or the waste. Click the stock to draw; recycling follows your chosen redeal limit.";
      break;
    case GameMode::TriPeaks:
      rules = "Clear all 28 cards from the three peaks. Click an exposed card one rank above or below the waste card, regardless of suit. Cards become exposed when both cards below them are gone. Draw from the stock when stuck; there are no redeals. Standard rules do not connect ace and king; Wraparound rules do.";
      break;
    }
    modal_->open(QString::fromLatin1(modeName(game_.state().mode)) + " rules", rules +
      "\n\nDrag a card or run, or click a card and its destination. Double-click a top card to send it to a foundation where available.\n\nKeyboard: Left and Right choose piles; Enter selects or plays. Up and Down extend or shorten a movable run, or choose another pile. Space draws, A sends home, H hints, Ctrl+Z undoes. Ctrl+N opens a new deal. Escape clears a selection or closes this panel.", {"Back to the table"});
  }
  if (dialog == Dialog::Won)
    modal_->open(
        "All cards home.",
        QString("You finished in %1 moves. A quiet moment, well spent.")
            .arg(game_.state().moves),
        {"Another deal", "Stay here"});
}
void Window::save() {
  if (gameFileInvalid_)
    return;
  QJsonObject json{{"game", game_.toJson()},
                   {"elapsedSeconds", double(seconds_)}};
  QDir().mkpath(configDirectory_);
  QSaveFile file(configDirectory_ + "/game.json");
  const auto bytes = QJsonDocument(json).toJson();
  if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() ||
      !file.commit())
    status_->setText("Could not save your deal: " + file.errorString());
}
void Window::load() {
  QFile file(configDirectory_ + "/game.json");
  if (!file.exists())
    return;
  if (!file.open(QIODevice::ReadOnly) || file.size() > 1024 * 1024) {
    gameFileInvalid_ = true;
    status_->setText(
        "Saved deal could not be read. Start a new deal to replace it.");
    return;
  }
  QJsonParseError error;
  const auto document = QJsonDocument::fromJson(file.readAll(), &error);
  const auto elapsed = document.object().value("elapsedSeconds").toDouble(0);
  if (error.error != QJsonParseError::NoError || !document.isObject() ||
      elapsed < 0 || elapsed > 315360000 ||
      !game_.restore(document.object().value("game").toObject())) {
    gameFileInvalid_ = true;
    status_->setText("Saved deal is invalid. Start a new deal to replace it.");
    return;
  }
  seconds_ = qint64(elapsed);
  hintEarned_ = game_.state().hintEarned;
  status_->setText("Your cards are right where you left them.");
}
void Window::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  if (modal_)
    modal_->setGeometry(rect());
}
void Window::closeEvent(QCloseEvent *event) {
  save();
  QWidget::closeEvent(event);
}
} // namespace solitare
