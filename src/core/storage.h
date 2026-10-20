#pragma once

#include "theme.h"

#include <QStringList>
#include <functional>

namespace solitare {

QString defaultConfigDirectory();
QString configDirectory(const QStringList &arguments);
QString themeLockPath(const QString &outputPath);
Theme updateThemeLocked(const QString &path, const std::function<Theme(const Theme &)> &update,
                        int timeoutMs = 2000);

}
