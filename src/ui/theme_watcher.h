#pragma once

#include "core/theme.h"

#include <QFileSystemWatcher>
#include <QObject>
#include <QTimer>

namespace solitare {

class ThemeWatcher : public QObject {
    Q_OBJECT
public:
    ThemeWatcher(QString path, Theme initial, QObject *parent = nullptr);

    const Theme &theme() const;
    void reload();

signals:
    void changed();
    void rejected(const QString &message);

private:
    QString path_;
    Theme theme_;
    Theme configuredTheme_;
    QFileSystemWatcher watcher_;
    QTimer debounce_;
    QTimer poll_;
    QString lastError_;

    void updateWatches();
};

}
