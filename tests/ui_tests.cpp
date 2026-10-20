#include "core/theme.h"
#include "ui/window.h"
#include "ui/card_art.h"
#include "ui/modal_motion.h"
#include <QAbstractItemView>
#include <QScreen>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QComboBox>
#include <QLabel>
#include <QPainter>
#include <QPaintEvent>
#include <QFile>
#include <QFontMetrics>
#include <QJsonDocument>
#include <QJsonArray>
#include <QPushButton>
#include <QTemporaryDir>
#include <QtTest>
#include <QWindow>
using namespace solitare;
namespace {
const std::array modes{GameMode::Klondike, GameMode::Spider, GameMode::FreeCell, GameMode::Pyramid, GameMode::TriPeaks};
QPoint cardPoint(const Board &board, const Game &game, Location location) {
  auto rectangle = board.cardRect(location);
  if (location.kind == Location::Tableau) {
    const auto &cards = game.state().tableau[location.pile];
    if (location.index >= 0 && location.index + 1 < int(cards.size()))
      rectangle.setBottom(std::min(rectangle.bottom(), board.cardRect({Location::Tableau, location.pile, location.index + 1}).top() - 1));
  }
  return rectangle.center().toPoint();
}
bool activateTestWindow(QWidget &window) {
  window.activateWindow();
  if (auto *handle = window.windowHandle()) handle->requestActivate();
  return QTest::qWaitForWindowActive(&window);
}
bool settled(const Modal *modal) {
  const auto *motion = modal->findChild<ModalMotion *>();
  return motion && !motion->isRunning() && motion->progress() == 1;
}
bool focusInside(const QWidget *widget) {
  const auto *focus = QApplication::focusWidget();
  return focus && widget->isAncestorOf(focus);
}
void clickCard(Board &board, const Game &game, Location location) {
  QTest::mouseClick(&board, Qt::LeftButton, Qt::NoModifier, cardPoint(board, game, location));
}
}

class PaintCounter : public QObject {
public:
  int count = 0;
  QRegion lastRegion;
  bool eventFilter(QObject *, QEvent *event) override {
    if (event->type() == QEvent::Paint) {
      ++count;
      lastRegion = static_cast<QPaintEvent *>(event)->region();
    }
    return false;
  }
};

class UiTests : public QObject {
  Q_OBJECT
private slots:
  void drawUndoAndResume() {
    QTemporaryDir dir;
    saveTheme(dir.filePath("theme.json"), presetTheme("lofi"));
    State drawn;
    {
      Window window(dir.path(), presetTheme("lofi"));
      window.show();
      QVERIFY(QTest::qWaitForWindowExposed(&window));
      QVERIFY(activateTestWindow(window));
      auto original = window.game().state();
      QTest::mouseClick(window.board(), Qt::LeftButton, Qt::NoModifier,
                        window.board()
                            ->cardRect({Location::Stock, 0, -1})
                            .center()
                            .toPoint());
      QCOMPARE(window.game().state().stock.size(), original.stock.size() - 1);
      QTest::mouseClick(window.findChild<QPushButton *>("undo"),
                        Qt::LeftButton);
      QVERIFY(window.game().state() == original);
      window.board()->setFocus();
      QTest::keyClick(window.board(), Qt::Key_Space);
      drawn = window.game().state();
      window.close();
    }
    Window resumed(dir.path(), presetTheme("lofi"));
    QVERIFY(resumed.game().state() == drawn);
  }
  void clickAndDragLegalMoves() {
    Game game(3);
    Board board(game);
    board.resize(1000, 650);
    board.show();
    auto hint = game.hint();
    for (int i = 0; i < 100 && (!hint || hint->second.kind == Location::Waste);
         ++i) {
      game.draw();
      hint = game.hint();
    }
    QVERIFY(hint);
    QVERIFY(hint->second.kind != Location::Waste);
    const auto before = game.state();
    const auto from = cardPoint(board, game, hint->first);
    const auto to = board.cardRect(hint->second).center().toPoint();
    QTest::mouseClick(&board, Qt::LeftButton, Qt::NoModifier, from);
    QTest::mouseClick(&board, Qt::LeftButton, Qt::NoModifier, to);
    QCOMPARE(game.state().moves, before.moves + 1);
    QVERIFY(game.undo());
    board.resetInteraction();
    QTest::mousePress(&board, Qt::LeftButton, Qt::NoModifier, from);
    QMouseEvent move(QEvent::MouseMove, to, board.mapToGlobal(to), Qt::NoButton,
                     Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&board, &move);
    QTest::mouseRelease(&board, Qt::LeftButton, Qt::NoModifier, to);
    QCOMPARE(game.state().moves, before.moves + 1);
  }
  void repeatedModalThemeChanges() {
    QTemporaryDir dir;
    saveTheme(dir.filePath("theme.json"), presetTheme("lofi"));
    Window window(dir.path(), presetTheme("lofi"));
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QVERIFY(activateTestWindow(window));
    auto *modal = window.findChild<Modal *>();
    for (int i = 0; i < 15; ++i) {
      QTest::mouseClick(window.findChild<QPushButton *>("help"),
                        Qt::LeftButton);
      window.resize(i % 2 ? 700 : 1120, i % 2 ? 580 : 800);
      auto theme = presetTheme("lofi");
      theme.colors["accent"] = QColor::fromHsv(i * 20, 100, 200);
      modal->setTheme(theme);
      QTest::qWait(2);
      QVERIFY(window.rect().contains(
          modal->findChild<QWidget *>("modalCard")->geometry()));
      QTRY_VERIFY(focusInside(modal));
      QTest::keyClick(QApplication::focusWidget(), Qt::Key_Escape);
      QTRY_VERIFY(!modal->isVisible());
    }
  }
  void selectionAcrossPileKinds() {
    Game game(42);
    Board board(game);
    board.resize(1000, 650);
    board.show();
    for (int i = 0; i < 24; ++i)
      game.draw();
    QTest::mouseClick(
        &board, Qt::LeftButton, Qt::NoModifier,
        board.cardRect({Location::Waste, 0, -1}).center().toPoint());
    for (int i = 0; i < 6; ++i)
      QTest::keyClick(&board, Qt::Key_Right);
    QTest::keyClick(&board, Qt::Key_Up);
    QTest::keyClick(&board, Qt::Key_Down);
    QCOMPARE(game.state().waste.size(), size_t(24));
  }
  void modalBlocksUnderlyingInput() {
    QTemporaryDir dir;
    saveTheme(dir.filePath("theme.json"), presetTheme("lofi"));
    Window window(dir.path(), presetTheme("lofi"));
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QVERIFY(activateTestWindow(window));
    QTest::mouseClick(window.findChild<QPushButton *>("newDeal"),
                      Qt::LeftButton);
    auto *modal = window.findChild<Modal *>();
    QVERIFY(modal->isVisible());
    QVERIFY(!modal->isWindow());
    QVERIFY(!window.board()->isEnabled());
    auto before = window.game().state();
    QTest::keyClick(&window, Qt::Key_Z, Qt::ControlModifier);
    QVERIFY(window.game().state() == before);
    for (int i = 0; i < 12; ++i) {
      QTRY_VERIFY(focusInside(modal));
      QTest::keyClick(QApplication::focusWidget(), Qt::Key_Tab);
      QTRY_VERIFY(focusInside(modal));
    }
    window.resize(700, 580);
    QTest::qWait(40);
    QVERIFY(modal->rect().contains(
        modal->findChild<QWidget *>("modalCard")->geometry()));
    modal->findChild<QComboBox *>("gameVariant")->setCurrentIndex(1);
    QTest::mouseClick(modal->findChild<QPushButton *>("modalAction0"), Qt::LeftButton);
    QTRY_VERIFY(!modal->isVisible());
    QVERIFY(window.board()->isEnabled());
    QCOMPARE(window.game().state().drawCount, 3);
    QTest::mouseClick(window.findChild<QPushButton *>("help"), Qt::LeftButton);
    QTRY_VERIFY(focusInside(modal));
    QTest::keyClick(QApplication::focusWidget(), Qt::Key_Escape);
    QTRY_VERIFY(!modal->isVisible());
  }
  void modeControlsAndResume() {
    for (const auto mode : modes) {
      QTemporaryDir dir;
      saveTheme(dir.filePath("theme.json"), presetTheme("lofi"));
      State expected;
      {
        Window window(dir.path(), presetTheme("lofi"));
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QVERIFY(activateTestWindow(window));
        QTest::mouseClick(window.findChild<QPushButton *>("newDeal"), Qt::LeftButton);
        auto *modal = window.findChild<Modal *>();
        auto *selector = modal->findChild<QComboBox *>("gameMode");
        auto *variant = modal->findChild<QComboBox *>("gameVariant");
        selector->setCurrentIndex(selector->findData(int(mode)));
        if (mode != GameMode::FreeCell) variant->setCurrentIndex(variant->count() - 1);
        window.resize(700, 580);
        QTest::qWait(5);
        QVERIFY(modal->rect().contains(modal->findChild<QWidget *>("modalCard")->geometry()));
        for (int i = 0; i < 15; ++i) {
          QTRY_VERIFY(focusInside(modal));
          QTest::keyClick(QApplication::focusWidget(), Qt::Key_Tab);
          QTRY_VERIFY(focusInside(modal));
        }
        QTest::mouseClick(modal->findChild<QPushButton *>("modalAction0"), Qt::LeftButton);
        QTRY_VERIFY(!modal->isVisible());
        QCOMPARE(window.game().state().mode, mode);
        if (mode == GameMode::Klondike) QCOMPARE(window.game().state().drawCount, 3);
        if (mode == GameMode::Spider) QCOMPARE(window.game().state().options.spiderSuits, 4);
        if (mode == GameMode::Pyramid) QCOMPARE(window.game().state().options.pyramidRedeals, 2);
        if (mode == GameMode::TriPeaks) QVERIFY(window.game().state().options.tripeaksWraparound);
        QVERIFY(window.board()->isEnabled());
        expected = window.game().state();
        window.close();
      }
      Window resumed(dir.path(), presetTheme("lofi"));
      QVERIFY(resumed.game().state() == expected);
    }
  }
  void popupRowsAndSelection() {
    QTemporaryDir dir;
    Window window(dir.path(), presetTheme("lofi"));
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QVERIFY(activateTestWindow(window));
    auto *modal = window.findChild<Modal *>();
    auto *game = modal->findChild<QComboBox *>("gameMode");
    auto *rules = modal->findChild<QComboBox *>("gameVariant");
    const auto output = qEnvironmentVariable("SOLITARE_SCREENSHOT_DIR");
    for (const QSize size : {QSize(700, 580), QSize(1120, 800), QSize(1600, 1000)}) {
      window.resize(size);
      for (const auto mode : modes) {
        modal->openDeal(mode, {});
        QTRY_VERIFY(settled(modal));
        QTest::mouseClick(game, Qt::LeftButton);
        QTRY_VERIFY(game->view()->isVisible());
        auto *popup = game->view()->window();
        QTRY_VERIFY(game->view()->viewport()->rect().contains(game->view()->visualRect(game->model()->index(game->count() - 1, 0))));
        for (int row = 0; row < game->count(); ++row)
          QVERIFY(game->view()->viewport()->rect().contains(game->view()->visualRect(game->model()->index(row, 0))));
        QVERIFY(popup->screen()->availableGeometry().contains(popup->frameGeometry()));
        if (!output.isEmpty() && mode == GameMode::TriPeaks) {
          QVERIFY(popup->grab().save(output + QString("/solitare-game-popup-%1.png").arg(size.width())));
          QVERIFY(window.grab().save(output + QString("/solitare-game-popup-window-%1.png").arg(size.width())));
        }
        const auto lastGame = game->model()->index(game->count() - 1, 0);
        QTest::mouseMove(game->view()->viewport(), game->view()->visualRect(lastGame).center());
        QTest::qWait(250);
        QTest::mouseClick(game->view()->viewport(), Qt::LeftButton, Qt::NoModifier, game->view()->visualRect(lastGame).center());
        QTRY_COMPARE(game->currentIndex(), game->count() - 1);
        QTRY_VERIFY(!popup->isVisible());
        game->showPopup();
        QTRY_VERIFY(game->view()->isVisible());
        QTest::keyClick(game->view(), Qt::Key_Home);
        for (int row = 0; row < int(mode); ++row) QTest::keyClick(game->view(), Qt::Key_Down);
        QTest::keyClick(game->view(), Qt::Key_Return);
        QTRY_COMPARE(game->currentData().toInt(), int(mode));
        QTRY_VERIFY(!popup->isVisible());
        if (mode != GameMode::FreeCell) {
          QTest::mouseClick(rules, Qt::LeftButton);
          QTRY_VERIFY(rules->view()->isVisible());
          popup = rules->view()->window();
          for (int row = 0; row < rules->count(); ++row) {
            const auto rectangle = rules->view()->visualRect(rules->model()->index(row, 0));
            QVERIFY(rules->view()->viewport()->rect().contains(rectangle));
            QVERIFY(rectangle.width() >= rules->fontMetrics().horizontalAdvance(rules->itemText(row)) + 24);
          }
          QVERIFY(popup->screen()->availableGeometry().contains(popup->frameGeometry()));
          if (!output.isEmpty() && mode == GameMode::TriPeaks) {
            QVERIFY(popup->grab().save(output + QString("/solitare-rules-popup-%1.png").arg(size.width())));
            QVERIFY(window.grab().save(output + QString("/solitare-rules-popup-window-%1.png").arg(size.width())));
          }
          const auto lastRule = rules->model()->index(rules->count() - 1, 0);
          QTest::mouseMove(rules->view()->viewport(), rules->view()->visualRect(lastRule).center());
          QTest::qWait(250);
          QTest::mouseClick(rules->view()->viewport(), Qt::LeftButton, Qt::NoModifier, rules->view()->visualRect(lastRule).center());
          QTRY_COMPARE(rules->currentIndex(), rules->count() - 1);
          QTRY_VERIFY(!popup->isVisible());
          rules->showPopup();
          QTRY_VERIFY(rules->view()->isVisible());
          QTest::keyClick(rules->view(), Qt::Key_Home);
          QTest::keyClick(rules->view(), Qt::Key_End);
          QTest::keyClick(rules->view(), Qt::Key_Return);
          QTRY_COMPARE(rules->currentIndex(), rules->count() - 1);
          QTRY_VERIFY(!popup->isVisible());
        }
        if (!output.isEmpty() && mode == GameMode::TriPeaks)
          QVERIFY(window.grab().save(output + QString("/solitare-modal-settled-%1.png").arg(size.width())));
        modal->dismiss();
        QTRY_VERIFY(!modal->isVisible());
      }
    }
  }
  void popupUsesLiveTheme() {
    QTemporaryDir dir;
    auto theme = presetTheme("lofi");
    theme.colors["accent"] = QColor("#9bcbfa");
    theme.colors["surface"] = QColor("#181c20");
    Window window(dir.path(), theme);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QVERIFY(activateTestWindow(window));
    auto *modal = window.findChild<Modal *>();
    modal->openDeal(GameMode::Klondike, {});
    QTRY_VERIFY(settled(modal));
    auto *combo = modal->findChild<QComboBox *>("gameMode");
    combo->showPopup();
    QTRY_VERIFY(combo->view()->isVisible());
    const auto first = combo->view()->visualRect(combo->model()->index(0, 0));
    const auto second = combo->view()->visualRect(combo->model()->index(1, 0));
    auto sample = [&](const QRect &row) {
      const auto pixmap = combo->view()->viewport()->grab();
      return pixmap.toImage().pixelColor(qRound((row.left() + 3) * pixmap.devicePixelRatio()), qRound(row.center().y() * pixmap.devicePixelRatio()));
    };
    QTRY_COMPARE(sample(first), theme.color("accent"));
    QCOMPARE(sample(second), theme.color("surface"));
    theme.colors["accent"] = QColor("#dfad77");
    theme.colors["surface"] = QColor("#26342f");
    modal->setTheme(theme);
    QTRY_COMPARE(sample(first), theme.color("accent"));
    QCOMPARE(sample(second), theme.color("surface"));
    combo->hidePopup();
    modal->dismiss();
    QTRY_VERIFY(!modal->isVisible());
  }
  void modalAnimationAndDeferredChoice() {
    QTemporaryDir dir;
    Window window(dir.path(), presetTheme("lofi"));
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QVERIFY(activateTestWindow(window));
    auto *modal = window.findChild<Modal *>();
    auto *motion = modal->findChild<ModalMotion *>();
    QVERIFY(motion);
    auto *timeline = motion->findChild<QTimeLine *>("modalTimeline");
    QVERIFY(timeline);
    QSignalSpy dismissed(modal, &Modal::dismissed), chosen(modal, &Modal::chosen);
    QTest::mouseClick(window.findChild<QPushButton *>("newDeal"), Qt::LeftButton);
    QCOMPARE(timeline->duration(), 120);
    QCOMPARE(timeline->easingCurve().type(), QEasingCurve::OutCubic);
    timeline->setPaused(true); timeline->setCurrentTime(0);
    QCOMPARE(motion->progress(), 0.0);
    QCOMPARE(motion->scale(), .98);
    timeline->setCurrentTime(60);
    QCOMPARE(motion->progress(), .875);
    QCOMPARE(motion->scale(), .9975);
    const auto center = modal->findChild<QWidget *>("modalCard")->geometry().center();
    QVERIFY((center - modal->rect().center()).manhattanLength() <= 2);
    QVERIFY(!window.board()->isEnabled());
    timeline->setPaused(false);
    QTRY_VERIFY(settled(modal));
    auto *rules = modal->findChild<QComboBox *>("gameVariant");
    rules->setCurrentIndex(1);
    const auto before = window.game().state();
    auto *start = modal->findChild<QPushButton *>("modalAction0");
    QTest::mouseClick(start, Qt::LeftButton);
    QTest::mouseClick(start, Qt::LeftButton);
    QVERIFY(modal->isVisible());
    QVERIFY(!window.board()->isEnabled());
    QVERIFY(window.game().state() == before);
    QCOMPARE(chosen.count(), 0);
    timeline->setPaused(true); timeline->setCurrentTime(60);
    QCOMPARE(motion->progress(), .125);
    QCOMPARE(motion->scale(), .9825);
    QTest::keyClick(rules, Qt::Key_Up);
    QCOMPARE(rules->currentIndex(), 1);
    QTest::keyClick(&window, Qt::Key_N, Qt::ControlModifier);
    QVERIFY(window.game().state() == before);
    QCOMPARE(chosen.count(), 0);
    timeline->setPaused(false);
    QTRY_VERIFY(!modal->isVisible());
    QCOMPARE(chosen.count(), 1);
    QCOMPARE(dismissed.count(), 1);
    QCOMPARE(window.game().state().drawCount, 3);
    QVERIFY(window.board()->isEnabled());
    QTRY_VERIFY(window.findChild<QPushButton *>("newDeal")->hasFocus());
    QCOMPARE(motion->progress(), 0.0);
  }
  void modalReversalAndReducedMotion() {
    QTemporaryDir dir;
    Window window(dir.path(), presetTheme("lofi"));
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QVERIFY(activateTestWindow(window));
    auto *modal = window.findChild<Modal *>();
    auto *motion = modal->findChild<ModalMotion *>();
    QSignalSpy dismissed(modal, &Modal::dismissed), chosen(modal, &Modal::chosen);
    for (int attempt = 0; attempt < 4; ++attempt) {
      modal->openDeal(GameMode::TriPeaks, {});
      QTest::qWait(20);
      modal->dismiss();
      QTest::qWait(20);
      window.resize(attempt % 2 ? 700 : 1120, attempt % 2 ? 580 : 800);
      modal->open("Reopened panel", "Keep focus and the latest panel while reversing.", {"Close"});
      QTRY_VERIFY(settled(modal));
      QCOMPARE(dismissed.count(), attempt);
      QVERIFY(!window.board()->isEnabled());
      QVERIFY(modal->rect().contains(modal->findChild<QWidget *>("modalCard")->geometry()));
      QTRY_VERIFY(focusInside(modal));
      QTest::keyClick(QApplication::focusWidget(), Qt::Key_Escape);
      QTest::keyClick(modal, Qt::Key_Escape);
      QTRY_VERIFY(!modal->isVisible());
      QCOMPARE(dismissed.count(), attempt + 1);
      QCOMPARE(chosen.count(), 0);
      QVERIFY(window.board()->isEnabled());
    }
    const auto previous = qgetenv("SOLITARE_REDUCED_MOTION");
    const auto restore = qScopeGuard([previous] { if (previous.isNull()) qunsetenv("SOLITARE_REDUCED_MOTION"); else qputenv("SOLITARE_REDUCED_MOTION", previous); });
    qputenv("SOLITARE_REDUCED_MOTION", "1");
    modal->openDeal(GameMode::Spider, {});
    QVERIFY(settled(modal));
    QVERIFY(!motion->isRunning());
    modal->dismiss();
    QVERIFY(!modal->isVisible());
    QVERIFY(window.board()->isEnabled());
    QCOMPARE(dismissed.count(), 5);
  }
  void modeInputAndUndo() {
    for (const auto mode : modes) {
      Game game(3, mode);
      Board board(game);
      board.resize(1000, 650);
      board.show();
      QVERIFY(QTest::qWaitForWindowExposed(&board));
      auto hint = game.hint();
      QVERIFY(hint);
      const auto before = game.state();
      if (hint->first.kind == Location::Stock || hint->first == hint->second) clickCard(board, game, hint->first);
      else {
        clickCard(board, game, hint->first);
        clickCard(board, game, hint->second);
      }
      QCOMPARE(game.state().moves, before.moves + 1);
      QVERIFY(game.undo());
      QVERIFY(game.state() == before);
      board.resetInteraction();
      if (mode != GameMode::FreeCell) {
        QTest::keyClick(&board, Qt::Key_Space);
        QCOMPARE(game.state().moves, before.moves + 1);
      }
    }
  }
  void coveredLayoutCardsDoNotActivate() {
    for (const auto mode : {GameMode::Pyramid, GameMode::TriPeaks}) {
      Game game(3, mode);
      Board board(game);
      board.resize(1000, 650);
      board.show();
      const auto before = game.state();
      const Location covered{Location::Tableau, 0, 0};
      QVERIFY(!game.exposed(covered));
      const auto r = board.cardRect(covered);
      QTest::mouseClick(&board, Qt::LeftButton, Qt::NoModifier, QPoint(int(r.center().x()), int(r.top() + 8)));
      QVERIFY(game.state() == before);
      for (int i = mode == GameMode::Pyramid ? 21 : 18; i < 28; ++i) {
        clickCard(board, game, {Location::Tableau, i, 0});
        if (game.state() != before) break;
      }
      if (game.state() != before) {
        QVERIFY(game.undo());
        QVERIFY(game.state() == before);
      }
      else QVERIFY(game.state() == before);
      QVERIFY(!game.state().tableau[0].empty());
    }
  }
  void coveredLayoutCardsRenderAsBacks() {
    for (const auto mode : {GameMode::Pyramid, GameMode::TriPeaks}) {
      Game game(3, mode);
      Board board(game);
      board.resize(1000, 650);
      board.show();
      QVERIFY(QTest::qWaitForWindowExposed(&board));
      const auto screenshot = board.grab();
      QImage expected(screenshot.size(), QImage::Format_ARGB32_Premultiplied);
      expected.setDevicePixelRatio(screenshot.devicePixelRatio());
      expected.fill(Qt::transparent);
      const auto rectangle = board.cardRect({Location::Tableau, 0, 0});
      QPainter painter(&expected);
      drawCard(painter, rectangle, {1, 0, false}, presetTheme("lofi"));
      painter.end();
      const double ratio = screenshot.devicePixelRatio();
      const QRect area(qRound((rectangle.left() + rectangle.width() * .1) * ratio),
                       qRound((rectangle.top() + rectangle.width() * .07) * ratio),
                       qRound(rectangle.width() * .8 * ratio),
                       qRound(rectangle.height() * .15 * ratio));
      QCOMPARE(screenshot.toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied).copy(area), expected.copy(area));
    }
  }
  void exposedUpperLayoutCardsAndEmptySlots() {
    for (const auto mode : {GameMode::Pyramid, GameMode::TriPeaks}) {
      Game game(3, mode);
      if (mode == GameMode::Pyramid)
        for (int seed = 0; game.state().tableau[0][0].rank != 13; ++seed) game.newGame(seed, mode);
      auto json = game.toJson();
      auto tableau = json["tableau"].toArray();
      auto destination = json[mode == GameMode::Pyramid ? "removed" : "waste"].toArray();
      for (int pile = 1; pile < tableau.size(); ++pile) {
        destination.append(tableau[pile].toArray()[0]);
        tableau[pile] = QJsonArray{};
      }
      if (mode == GameMode::TriPeaks) {
        const int rank = game.state().tableau[0][0].rank;
        int match = -1;
        for (int i = 0; i < destination.size(); ++i)
          if (std::abs(destination[i].toObject()["rank"].toInt() - rank) == 1) { match = i; break; }
        if (match < 0) {
          auto stock = json["stock"].toArray();
          for (int i = 0; i < stock.size(); ++i)
            if (std::abs(stock[i].toObject()["rank"].toInt() - rank) == 1) {
              auto card = stock.takeAt(i).toObject(); card["faceUp"] = true; destination.append(card); break;
            }
          json["stock"] = stock;
        } else destination.append(destination.takeAt(match));
      }
      json["tableau"] = tableau;
      json[mode == GameMode::Pyramid ? "removed" : "waste"] = destination;
      QVERIFY(game.restore(json));
      Board board(game);
      board.resize(1000, 650);
      board.show();
      QVERIFY(QTest::qWaitForWindowExposed(&board));
      QVERIFY(game.exposed({Location::Tableau, 0, 0}));
      QVERIFY(!board.hit(board.cardRect({Location::Tableau, 27, 0}).center()));
      board.showHint();
      QTest::keyClick(&board, Qt::Key_Right);
      QTest::keyClick(&board, Qt::Key_Right);
      QTest::keyClick(&board, Qt::Key_Return);
      QVERIFY(game.state().tableau[0].empty());
      QVERIFY(game.won());
    }
  }
  void cardArtworkCachePreservesPixelsAndDpr() {
    for (const qreal dpr : {1.0, 1.25, 2.0}) {
      BoardArtCache cache;
      auto theme = presetTheme("lofi");
      cache.setTheme(theme);
      const auto image = [&] {
        QImage result(QSize(qCeil(180 * dpr), qCeil(210 * dpr)), QImage::Format_ARGB32_Premultiplied);
        result.setDevicePixelRatio(dpr); result.fill(Qt::transparent); return result;
      };
      const QRectF rectangle(20.2, 11.4, 83.6, 119.548);
      for (const Card card : {Card{12, 3, true}, Card{1, 0, false}}) {
        auto actual = image(), expected = image();
        { QPainter painter(&actual); cache.paintCard(painter, rectangle, card, false, dpr); }
        { QPainter painter(&expected); drawCard(painter, rectangle, card, theme); }
        QCOMPARE(actual, expected);
        const auto builds = cache.rasterizations();
        actual.fill(Qt::transparent);
        { QPainter painter(&actual); cache.paintCard(painter, rectangle, card, false, dpr); }
        QCOMPARE(cache.rasterizations(), builds);
        QCOMPARE(actual, expected);
      }
      const auto builds = cache.rasterizations();
      theme.colors["floral_red"] = QColor("#278eaa");
      cache.setTheme(theme);
      auto actual = image();
      { QPainter painter(&actual); cache.paintCard(painter, rectangle, {12, 3, true}, false, dpr); }
      QVERIFY(cache.rasterizations() > builds);
      const auto themedBuilds = cache.rasterizations();
      { QPainter painter(&actual); cache.paintCard(painter, rectangle, {12, 3, true}, false, dpr + .25); }
      QVERIFY(cache.rasterizations() > themedBuilds);
    }
  }
  void boardLayersReuseAndInvalidate() {
    Game game(3, GameMode::Spider);
    Board board(game);
    board.resize(1000, 650); board.show();
    QVERIFY(QTest::qWaitForWindowExposed(&board));
    const auto original = board.grab().toImage();
    const auto layers = board.layerBuilds(), art = board.artworkRasterizations();
    for (int i = 0; i < 8; ++i) board.grab();
    QCOMPARE(board.layerBuilds(), layers);
    QCOMPARE(board.artworkRasterizations(), art);
    QVERIFY(game.draw());
    const auto drawn = board.grab().toImage();
    QVERIFY(drawn != original);
    QVERIFY(board.layerBuilds() > layers);
    QVERIFY(game.undo());
    QCOMPARE(board.grab().toImage(), original);
    auto theme = presetTheme("lofi"); theme.colors["floral_green"] = QColor("#167faa");
    board.setTheme(theme);
    const auto themed = board.grab().toImage();
    QVERIFY(themed != original);
    const auto themedLayers = board.layerBuilds();
    board.resize(1050, 700); board.grab();
    QVERIFY(board.layerBuilds() > themedLayers);
    board.showHint();
    const auto hinted = board.grab().toImage();
    board.resetInteraction();
    QVERIFY(board.grab().toImage() != hinted);
  }
  void draggingReusesLayersAndLimitsDamage() {
    Game game(3, GameMode::Spider);
    Board board(game);
    board.resize(1000, 650); board.show();
    QVERIFY(QTest::qWaitForWindowExposed(&board));
    PaintCounter paints; board.installEventFilter(&paints);
    const auto before = game.state();
    const auto from = board.cardRect({Location::Tableau, 0, -1}).center().toPoint();
    QTest::mousePress(&board, Qt::LeftButton, Qt::NoModifier, from);
    auto move = [&](QPoint point) {
      QMouseEvent event(QEvent::MouseMove, point, board.mapToGlobal(point), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
      QApplication::sendEvent(&board, &event);
    };
    move(from + QPoint(80, 35));
    QTRY_VERIFY(paints.count > 0);
    board.grab();
    const auto layers = board.layerBuilds(), art = board.artworkRasterizations();
    for (int frame = 0; frame < 12; ++frame) {
      const auto count = paints.count;
      move(from + QPoint(90 + frame * 3, 35));
      QTRY_VERIFY(paints.count > count);
      QCOMPARE(board.layerBuilds(), layers);
      QCOMPARE(board.artworkRasterizations(), art);
      QVERIFY(paints.lastRegion.boundingRect().width() * paints.lastRegion.boundingRect().height() < board.width() * board.height() / 2);
    }
    QTest::mouseRelease(&board, Qt::LeftButton, Qt::NoModifier, QPoint(2, 2));
    QVERIFY(game.state() == before);
    board.grab();
    QVERIFY(board.layerBuilds() > layers);
  }
  void modalAnimationDoesNotRepaintFrozenBoard() {
    QTemporaryDir dir;
    Window window(dir.path(), presetTheme("lofi"));
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QVERIFY(activateTestWindow(window));
    PaintCounter paints; window.board()->installEventFilter(&paints);
    auto *modal = window.findChild<Modal *>();
    modal->openDeal(GameMode::Spider, {});
    auto *motion = modal->findChild<ModalMotion *>();
    auto *timeline = motion->findChild<QTimeLine *>("modalTimeline");
    timeline->setPaused(true);
    const auto captures = paints.count;
    const auto builds = window.board()->layerBuilds();
    for (int time : {24, 48, 72, 96}) {
      timeline->setCurrentTime(time); modal->repaint();
      QCOMPARE(paints.count, captures);
      QCOMPARE(window.board()->layerBuilds(), builds);
    }
    window.resize(700, 580);
    modal->repaint();
    QVERIFY(modal->rect().contains(modal->findChild<QWidget *>("modalCard")->geometry()));
    timeline->setPaused(false);
    QTRY_VERIFY(settled(modal));
    modal->dismiss();
    QTRY_VERIFY(!modal->isVisible());
    QVERIFY(window.board()->updatesEnabled());
    QVERIFY(window.board()->isEnabled());
    const auto count = paints.count;
    window.board()->update();
    QTRY_VERIFY(paints.count > count);
  }
  void stackedRanksStayVisibleAtMinimumSize() {
    QTemporaryDir dir;
    const auto theme = presetTheme("lofi");
    saveTheme(dir.filePath("theme.json"), theme);
    Game game(3);
    auto json = game.toJson();
    QJsonArray stock, run, tableau;
    for (int rank = 13; rank >= 7; --rank)
      run.append(QJsonObject{{"rank", rank}, {"suit", rank % 2 ? 0 : 3}, {"faceUp", true}});
    for (int suit = 0; suit < 4; ++suit)
      for (int rank = 1; rank <= 13; ++rank)
        if (rank < 7 || suit != (rank % 2 ? 0 : 3))
          stock.append(QJsonObject{{"rank", rank}, {"suit", suit}, {"faceUp", false}});
    for (int pile = 0; pile < 7; ++pile) tableau.append(pile == 6 ? run : QJsonArray{});
    json["stock"] = stock;
    json["tableau"] = tableau;
    QVERIFY(game.restore(json));
    QFile save(dir.filePath("game.json"));
    QVERIFY(save.open(QIODevice::WriteOnly));
    save.write(QJsonDocument(QJsonObject{{"game", json}, {"elapsedSeconds", 0}}).toJson());
    save.close();
    Window window(dir.path(), theme);
    window.resize(700, 580);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    auto *board = window.board();
    QVERIFY(window.game().state() == game.state());
    const auto card = board->cardRect({Location::Tableau, 6, 0});
    QFont rankFont("Noto Sans");
    rankFont.setPixelSize(std::max(10, qRound(card.width() * .21)));
    rankFont.setWeight(QFont::DemiBold);
    const double headerHeight = std::max(QFontMetrics(rankFont).tightBoundingRect("KQ109").height() + 3.0,
                                          card.width() * .255 + 2);
    for (int index = 0; index < 6; ++index) {
      const auto current = board->cardRect({Location::Tableau, 6, index});
      const auto next = board->cardRect({Location::Tableau, 6, index + 1});
      QVERIFY2(next.top() - current.top() >= headerHeight, "Stacked rank and suit marks must remain fully visible");
    }
    QVERIFY(QRectF(board->rect()).contains(board->cardRect({Location::Tableau, 6, 6})));
    const auto output = qEnvironmentVariable("SOLITARE_SCREENSHOT_DIR");
    if (!output.isEmpty()) QVERIFY(window.grab().save(output + "/solitare-face-up-run-700.png"));
  }
  void layoutAndScreenshots() {
    QTemporaryDir dir;
    saveTheme(dir.filePath("theme.json"), presetTheme("lofi"));
    Window window(dir.path(), presetTheme("lofi"));
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QVERIFY(activateTestWindow(window));
    const auto output = qEnvironmentVariable("SOLITARE_SCREENSHOT_DIR");
    for (const auto mode : modes) {
      window.newDeal(mode);
      for (const QSize size : {QSize(700, 580), QSize(1120, 800), QSize(1600, 1000)}) {
        window.resize(size);
        QTest::qWait(10);
        for (int i = 0; i < int(window.game().state().tableau.size()); ++i)
          QVERIFY2(QRectF(window.board()->rect()).contains(window.board()->cardRect({Location::Tableau, i, -1})), modeName(mode));
        if (!output.isEmpty())
          QVERIFY(window.grab().save(output + QString("/solitare-%1-%2.png").arg(modeId(mode)).arg(size.width())));
      }
    }
    window.resize(1120, 800);
    window.newDeal(GameMode::Klondike);
    QTest::qWait(10);
    if (!output.isEmpty()) {
      auto *modal = window.findChild<Modal *>();
      QVERIFY(window.grab().save(output + "/solitare.png"));
      QTest::mouseClick(window.findChild<QPushButton *>("newDeal"), Qt::LeftButton);
      QTRY_VERIFY(settled(modal));
      QVERIFY(window.grab().save(output + "/solitare-new-deal.png"));
      QTRY_VERIFY(focusInside(modal));
      QTest::keyClick(QApplication::focusWidget(), Qt::Key_Escape);
      QTRY_VERIFY(!modal->isVisible());
      QTest::mouseClick(window.findChild<QPushButton *>("help"), Qt::LeftButton);
      QTRY_VERIFY(settled(modal));
      QVERIFY(window.grab().save(output + "/solitare-help.png"));
      QTRY_VERIFY(focusInside(modal));
      QTest::keyClick(QApplication::focusWidget(), Qt::Key_Escape);
      QTRY_VERIFY(!modal->isVisible());
      auto light = presetTheme("lofi");
      light.colors["background"] = QColor("#e8dfc8");
      light.colors["card"] = QColor("#f8f0d9");
      light.colors["red_suit"] = QColor("#b32e24");
      light.colors["black_suit"] = QColor("#193e60");
      light.colors["text"] = QColor("#193e60");
      light.colors["muted"] = QColor("#53623a");
      window.board()->setTheme(light);
      QVERIFY(window.grab().save(output + "/solitare-light-cards.png"));
      QImage artwork(720, 250, QImage::Format_ARGB32_Premultiplied);
      artwork.fill(light.color("background"));
      QPainter painter(&artwork);
      drawTableDecoration(painter, artwork.rect(), light);
      for (int suit = 0; suit < 4; ++suit) drawCard(painter, QRectF(20 + suit * 140, 24, 112, 160), {suit == 0 ? 1 : suit == 1 ? 10 : suit == 2 ? 12 : 13, suit, true}, light);
      drawCard(painter, QRectF(580, 24, 112, 160), {1, 0, false}, light);
      painter.end();
      QVERIFY(artwork.save(output + "/solitare-card-art.png"));
    }
  }
  void malformedSaveIsPreserved() {
    QTemporaryDir dir;
    saveTheme(dir.filePath("theme.json"), presetTheme("lofi"));
    QFile save(dir.filePath("game.json"));
    QVERIFY(save.open(QIODevice::WriteOnly));
    save.write("bad saved deal");
    save.close();
    {
      Window window(dir.path(), presetTheme("lofi"));
      window.close();
    }
    QVERIFY(save.open(QIODevice::ReadOnly));
    QCOMPARE(save.readAll(), QByteArray("bad saved deal"));
  }
};
QTEST_MAIN(UiTests)
#include "ui_tests.moc"
