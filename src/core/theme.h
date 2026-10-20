#pragma once

#include <QColor>
#include <QJsonObject>
#include <QMap>
#include <QStringList>

namespace solitare {

struct Theme {
    QString name;
    QMap<QString, QColor> colors;
    QString sourceKind;
    QString sourcePath;

    QColor color(QString key) const;
    QJsonObject toJson() const;
    bool operator==(const Theme &) const = default;
};

QStringList colorKeys();
Theme presetTheme(const QString &name);
Theme parseTheme(const QJsonObject &data);
Theme loadTheme(const QString &path);
void saveTheme(const QString &path, const Theme &theme);
Theme ensureTheme(const QString &path);
Theme setColors(Theme theme, const QStringList &assignments);

}
