#include "theme_watcher.h"

#include "core/storage.h"
#include "core/theme_source.h"

#include <QDir>
#include <QFileInfo>
#include <QLockFile>
#include <exception>
#include <stdexcept>
#include <utility>

namespace solitare {

ThemeWatcher::ThemeWatcher(QString path, Theme initial, QObject *parent)
    : QObject(parent), path_(QDir::cleanPath(QFileInfo(path).absoluteFilePath())),
      theme_(std::move(initial)), configuredTheme_(theme_) {
    debounce_.setSingleShot(true);
    debounce_.setInterval(60);
    poll_.setInterval(350);
    connect(&watcher_, &QFileSystemWatcher::fileChanged, this, [this] { debounce_.start(); });
    connect(&watcher_, &QFileSystemWatcher::directoryChanged, this, [this] { debounce_.start(); });
    connect(&debounce_, &QTimer::timeout, this, &ThemeWatcher::reload);
    connect(&poll_, &QTimer::timeout, this, &ThemeWatcher::reload);
    updateWatches();
    poll_.start();
    QTimer::singleShot(0, this, &ThemeWatcher::reload);
}

const Theme &ThemeWatcher::theme() const {
    return theme_;
}

void ThemeWatcher::updateWatches() {
    QStringList wanted;
    const auto configDirectory = QFileInfo(path_).absolutePath();
    if (QFileInfo(configDirectory).isDir())
        wanted.append(configDirectory);
    if (QFileInfo::exists(path_) || QFileInfo(path_).isSymLink())
        wanted.append(path_);
    wanted.append(sourceWatchPaths(configuredTheme_));
    wanted.removeDuplicates();

    const auto current = watcher_.files() + watcher_.directories();
    for (const auto &path : current)
        if (!wanted.contains(path) || QFileInfo(path).isSymLink())
            watcher_.removePath(path);
    const auto refreshed = watcher_.files() + watcher_.directories();
    for (const auto &path : wanted)
        if (!refreshed.contains(path) && (QFileInfo::exists(path) || QFileInfo(path).isSymLink()))
            watcher_.addPath(path);
}

void ThemeWatcher::reload() {
    const auto lockPath = themeLockPath(path_);
    if (!QDir().mkpath(QFileInfo(lockPath).absolutePath())) {
        if (lastError_ != "Cannot create theme lock directory") {
            lastError_ = "Cannot create theme lock directory";
            emit rejected(lastError_);
        }
        return;
    }
    QLockFile lock(lockPath);
    if (!lock.tryLock(0))
        return;

    QString errorMessage;
    bool didChange = false;
    try {
        const auto configured = loadTheme(path_);
        configuredTheme_ = configured;
        if (!configured.sourceKind.isEmpty() && sourceTargetsFile(configured, path_))
            throw std::invalid_argument("A theme cannot follow its own output file");
        auto resolved = configured.sourceKind.isEmpty() ? configured : refreshSource(configured);
        if (resolved != configured)
            saveTheme(path_, resolved);
        configuredTheme_ = resolved;
        if (resolved != theme_) {
            theme_ = resolved;
            didChange = true;
        }
    } catch (const std::exception &error) {
        errorMessage = QString::fromUtf8(error.what());
    }
    lock.unlock();

    if (!errorMessage.isEmpty()) {
        if (lastError_ != errorMessage) {
            lastError_ = errorMessage;
            emit rejected(errorMessage);
        }
    } else {
        lastError_.clear();
    }
    updateWatches();
    if (didChange)
        emit changed();
}

}
