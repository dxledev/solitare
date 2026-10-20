#pragma once
#include "core/theme.h"
#include <QComboBox>
namespace solitare {
class ThemedCombo : public QComboBox {
  Q_OBJECT
public:
  explicit ThemedCombo(QWidget *parent = nullptr);
  void setTheme(const Theme &theme);
  void showPopup() override;
  void hidePopup() override;
  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;
protected:
  void paintEvent(QPaintEvent *) override;
  bool eventFilter(QObject *object, QEvent *event) override;
private:
  Theme theme_;
  int rowHeight() const;
  int contentWidth() const;
  void stylePopup();
  void positionPopup();
};
}
