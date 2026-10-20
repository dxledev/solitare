#include "theme.h"

#include "storage.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QLockFile>
#include <QRegularExpression>
#include <QSaveFile>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace solitare {
namespace {

const QStringList keys{
    "background", "surface", "surface_alt", "text", "muted", "accent", "accent_text",
    "border", "red_suit", "black_suit", "card", "card_back", "selection",
    "floral_red", "floral_green", "floral_blue", "floral_gold", "floral_cream"
};

const QMap<QString, QColor> floralDefaults{
    {"floral_red", QColor("#d94528")}, {"floral_green", QColor("#53623a")},
    {"floral_blue", QColor("#193e60")}, {"floral_gold", QColor("#e3a533")},
    {"floral_cream", QColor("#f4e6be")}
};

[[noreturn]] void fail(const QString &message) {
    throw std::invalid_argument(message.toStdString());
}

double channel(QString expression) {
    const auto parts = expression.trimmed().split('/');
    if (parts.size() > 2)
        fail("Unsupported Qt.rgba channel: " + expression);
    bool valid = false;
    double value = parts.first().trimmed().toDouble(&valid);
    if (!valid)
        fail("Invalid Qt.rgba channel: " + expression);
    if (parts.size() == 2) {
        bool denominatorValid = false;
        const double denominator = parts.last().trimmed().toDouble(&denominatorValid);
        if (!denominatorValid || denominator <= 0)
            fail("Invalid Qt.rgba divisor: " + expression);
        value /= denominator;
    }
    if (!std::isfinite(value) || value < 0 || value > 1)
        fail("Qt.rgba channels must be between 0 and 1");
    return value;
}

QColor parseColor(QString value) {
    value = value.trimmed();
    static const QRegularExpression rgbaPattern(R"(^Qt\.rgba\((.*)\)$)");
    const auto rgbaMatch = rgbaPattern.match(value);
    if (rgbaMatch.hasMatch()) {
        const auto channels = rgbaMatch.captured(1).split(',');
        if (channels.size() != 4)
            fail("Qt.rgba needs four channels");
        return QColor::fromRgbF(channel(channels[0]), channel(channels[1]),
                                channel(channels[2]), channel(channels[3]));
    }

    static const QRegularExpression hexPattern(R"(^#?(?:[0-9a-fA-F]{6}|[0-9a-fA-F]{8})$)");
    if (!hexPattern.match(value).hasMatch())
        fail("Expected #RRGGBB, alpha-first #AARRGGBB, or Qt.rgba(r, g, b, a): " + value);
    if (!value.startsWith('#'))
        value.prepend('#');
    const QColor color = QColor::fromString(value);
    if (!color.isValid())
        fail("Invalid color: " + value);
    return color;
}

QString colorString(const QColor &color) {
    if (!color.isValid())
        fail("Theme contains an invalid color");
    return color.alpha() == 255 ? color.name(QColor::HexRgb) : color.name(QColor::HexArgb);
}

QJsonObject readObject(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        fail(path + ": " + file.errorString());
    if (file.size() > 1024 * 1024)
        fail(path + ": theme file exceeds 1 MiB");
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        fail(path + ": expected a valid JSON object (" + error.errorString() + ")");
    return document.object();
}

}

QColor Theme::color(QString key) const {
    return colors.value(key, floralDefaults.value(key));
}

QJsonObject Theme::toJson() const {
    QJsonObject palette;
    for (const auto &key : keys) {
        const auto value = color(key);
        if (!value.isValid())
            fail("Missing or invalid theme color: " + key);
        palette.insert(key, colorString(value));
    }
    QJsonObject result{{"version", 1}, {"name", name}, {"colors", palette}};
    if (!sourceKind.isEmpty())
        result.insert("source", QJsonObject{{"kind", sourceKind}, {"path", sourcePath}});
    return result;
}

QStringList colorKeys() {
    return keys;
}

Theme presetTheme(const QString &name) {
    if (name != "lofi")
        fail("Unknown preset: " + name + ". Available: lofi");
    const QStringList values{
        "#182125", "#1e2a2e", "#2d3d42", "#e3e9e5", "#9baaa7", "#a9c7b0", "#182125",
        "#465954", "#dfa5a5", "#e3e9e5", "#253238", "#203d38", "#3b5547"
    };
    Theme theme;
    theme.name = name;
    for (qsizetype index = 0; index < values.size(); ++index)
        theme.colors.insert(keys[index], QColor(values[index]));
    for (auto entry = floralDefaults.constBegin(); entry != floralDefaults.constEnd(); ++entry)
        theme.colors.insert(entry.key(), entry.value());
    return theme;
}

Theme parseTheme(const QJsonObject &data) {
    if (data.value("version").toInt(-1) != 1)
        fail("Theme version must be 1");
    const QString name = data.value("name").toString().trimmed();
    if (name.isEmpty() || name.size() > 60)
        fail("Theme name must contain 1–60 characters");
    if (!data.value("colors").isObject())
        fail("Theme must contain a colors object");

    const auto palette = data.value("colors").toObject();
    for (auto entry = palette.constBegin(); entry != palette.constEnd(); ++entry)
        if (!keys.contains(entry.key()))
            fail("Unknown color: " + entry.key());

    Theme theme;
    theme.name = name;
    for (const auto &key : keys) {
        if (!palette.contains(key) && floralDefaults.contains(key)) {
            theme.colors.insert(key, floralDefaults.value(key));
            continue;
        }
        if (!palette.value(key).isString())
            fail(key + ": expected a color string");
        theme.colors.insert(key, parseColor(palette.value(key).toString()));
    }

    if (data.contains("source")) {
        if (!data.value("source").isObject())
            fail("Theme source must be an object");
        const auto source = data.value("source").toObject();
        theme.sourceKind = source.value("kind").toString();
        theme.sourcePath = source.value("path").toString();
        if (!QStringList{"file", "symlink", "desktop", "static", "caelestia", "noctalia"}.contains(theme.sourceKind)
            || theme.sourcePath.isEmpty() || !QFileInfo(theme.sourcePath).isAbsolute())
            fail("Theme source needs a known kind and an absolute path");
    }
    return theme;
}

Theme loadTheme(const QString &path) {
    return parseTheme(readObject(path));
}

void saveTheme(const QString &path, const Theme &theme) {
    const auto validated = parseTheme(theme.toJson());
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        fail("Cannot create " + QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        fail(path + ": " + file.errorString());
    const auto bytes = QJsonDocument(validated.toJson()).toJson(QJsonDocument::Indented);
    if (file.write(bytes) != bytes.size() || !file.commit())
        fail(path + ": " + file.errorString());
}

Theme ensureTheme(const QString &path) {
    const auto lockPath = themeLockPath(path);
    if (!QDir().mkpath(QFileInfo(lockPath).absolutePath()))
        fail("Cannot create theme lock directory");
    QLockFile lock(lockPath);
    if (!lock.tryLock(2000))
        fail("Theme is being edited by another process");
    if (QFileInfo::exists(path))
        return loadTheme(path);
    const auto theme = presetTheme("lofi");
    saveTheme(path, theme);
    return theme;
}

Theme setColors(Theme theme, const QStringList &assignments) {
    if (assignments.isEmpty())
        fail("Provide at least one COLOR=VALUE assignment");
    auto data = theme.toJson();
    auto palette = data.value("colors").toObject();
    for (const auto &assignment : assignments) {
        const auto separator = assignment.indexOf('=');
        if (separator < 1)
            fail("Expected COLOR=VALUE. Colors: " + keys.join(", "));
        const auto key = assignment.left(separator);
        if (!keys.contains(key))
            fail("Unknown color: " + key);
        palette.insert(key, assignment.mid(separator + 1));
    }
    data.insert("name", "custom");
    data.remove("source");
    data.insert("colors", palette);
    return parseTheme(data);
}

}
