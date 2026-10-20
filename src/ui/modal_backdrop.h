#pragma once
#include <QOpenGLWidget>
#include <QPixmap>
namespace solitare {
class ModalBackdrop : public QOpenGLWidget {
  Q_OBJECT
public:
  explicit ModalBackdrop(QWidget *parent);
  static bool available();
  void prewarm();
  void setFrames(const QPixmap &base, const QPixmap &overlay);
  void setProgress(qreal progress);
  void clearFrames();
  bool hasFrames() const { return !base_.isNull() && !overlay_.isNull(); }
protected:
  void paintGL() override;
private:
  QPixmap base_, overlay_;
  qreal progress_ = 0;
};
}
