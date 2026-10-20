#pragma once
#include "board.h"
#include "modal.h"
#include "theme_watcher.h"
#include <QTimer>
#include <QWidget>
class QLabel;
class QPushButton;
namespace solitare {
class Window : public QWidget {
  Q_OBJECT
public:
  Window(QString configDirectory, Theme theme, QWidget *parent = nullptr);
  const Game &game() const { return game_; }
  Board *board() const { return board_; }
  void newDeal(int drawCount);
  void newDeal(GameMode mode, const GameOptions &options = {});

protected:
  void resizeEvent(QResizeEvent *) override;
  void closeEvent(QCloseEvent *) override;

private:
  QString configDirectory_;
  Game game_;
  QWidget *content_;
  Board *board_;
  Modal *modal_ = nullptr;
  ThemeWatcher *watcher_;
  QLabel *details_, *status_;
  QPushButton *undo_, *hint_;
  bool hintEarned_ = false;
  QTimer timer_;
  qint64 seconds_ = 0;
  bool gameFileInvalid_ = false;
  enum class Dialog { None, NewGame, Help, Won };
  Dialog dialog_ = Dialog::None;
  void buildContent();
  void connectActions();
  void applyTheme();
  void refresh();
  void save();
  void load();
  void openDialog(Dialog dialog);
  void undo();
};
} // namespace solitare
