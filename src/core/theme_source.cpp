#include "theme_source.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QStandardPaths>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace solitare {
namespace {

using Palette = QMap<QString, QColor>;
constexpr qint64 maximumInputSize = 1024 * 1024;

[[noreturn]] void fail(const QString &message) {
    throw std::invalid_argument(message.toStdString());
}

QString configHome() {
    const auto path = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    return path.isEmpty() ? QDir::homePath() + "/.config" : path;
}

QString expandedPath(QString path) {
    if (path == "~")
        path = QDir::homePath();
    else if (path.startsWith("~/"))
        path = QDir::homePath() + path.mid(1);
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

QByteArray readBounded(const QString &path) {
    if (!QFileInfo(path).isFile())
        fail("Theme source must be a regular file: " + path);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        fail("Cannot read theme source: " + path + ": " + file.errorString());
    if (file.size() > maximumInputSize)
        fail("Theme input exceeds 1 MiB: " + path);
    const auto bytes = file.read(maximumInputSize + 1);
    if (bytes.size() > maximumInputSize || file.error() != QFileDevice::NoError)
        fail("Cannot read theme source within the 1 MiB limit: " + path);
    return bytes;
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
        const auto parts = rgbaMatch.captured(1).split(',');
        if (parts.size() != 4)
            fail("Qt.rgba needs four channels");
        return QColor::fromRgbF(channel(parts[0]), channel(parts[1]), channel(parts[2]), channel(parts[3]));
    }
    static const QRegularExpression colorPattern(R"(^#?([0-9a-fA-F]{6}|[0-9a-fA-F]{8})$)");
    const auto match = colorPattern.match(value);
    if (!match.hasMatch())
        fail("Unsupported palette color: " + value);
    const QColor color("#" + match.captured(1));
    if (!color.isValid())
        fail("Unsupported palette color: " + value);
    return color;
}

QString normalizedKey(QString key) {
    key = key.toLower();
    key.remove(QRegularExpression("[^a-z0-9]"));
    return key;
}

QMap<QString, QColor> qmlPalette(const QString &path) {
    QString contents = QString::fromUtf8(readBounded(path));
    contents.remove(QRegularExpression("/\\*.*?\\*/", QRegularExpression::DotMatchesEverythingOption));
    contents.remove(QRegularExpression("//[^\\n]*"));
    static const QRegularExpression propertyPattern(
        R"REGEX(property\s+color\s+(\w+)\s*:\s*([^\r\n]+))REGEX");
    QMap<QString, QColor> result;
    auto matches = propertyPattern.globalMatch(contents);
    while (matches.hasNext()) {
        const auto match = matches.next();
        QString value = match.captured(2).trimmed();
        if (value.endsWith(';'))
            value.chop(1);
        if ((value.startsWith('"') && value.endsWith('"'))
            || (value.startsWith('\'') && value.endsWith('\'')))
            value = value.mid(1, value.size() - 2);
        result.insert(match.captured(1), parseColor(value));
    }
    if (result.isEmpty())
        fail("No supported color properties in " + path);
    return result;
}

QJsonObject paletteObject(QJsonObject data) {
    for (int depth = 0; depth < 5; ++depth) {
        if (data.value("colors").isObject()) {
            data = data.value("colors").toObject();
            continue;
        }
        if (data.value("colours").isObject()) {
            data = data.value("colours").toObject();
            continue;
        }
        if (data.value("palette").isObject()) {
            data = data.value("palette").toObject();
            continue;
        }
        if (data.value("dark").isObject() || data.value("light").isObject()) {
            QString mode = data.value("mode").toString("dark").toLower();
            if (mode != "dark" && mode != "light")
                mode = "dark";
            if (!data.value(mode).isObject())
                mode = mode == "dark" ? "light" : "dark";
            data = data.value(mode).toObject();
            continue;
        }
        break;
    }
    return data;
}

QMap<QString, QColor> jsonPalette(const QJsonObject &data) {
    const auto values = paletteObject(data);
    QMap<QString, QColor> result;
    for (auto entry = values.constBegin(); entry != values.constEnd(); ++entry) {
        if (!entry.value().isString())
            continue;
        const auto value = entry.value().toString().trimmed();
        if (value.startsWith("Qt.rgba(") || value.startsWith('#')
            || QRegularExpression(R"(^[0-9a-fA-F]{6}(?:[0-9a-fA-F]{2})?$)").match(value).hasMatch())
            result.insert(entry.key(), parseColor(value));
    }
    if (result.isEmpty())
        fail("No supported colors in palette JSON");
    return result;
}

QMap<QString, QColor> kittyPalette(const QString &path) {
    const auto contents = QString::fromUtf8(readBounded(path));
    static const QRegularExpression linePattern(
        R"(^\s*(background|foreground|cursor|color(?:[0-9]|1[0-5]))\s+(#[0-9a-fA-F]{6}(?:[0-9a-fA-F]{2})?)\s*(?:#.*)?$)",
        QRegularExpression::MultilineOption);
    QMap<QString, QColor> result;
    auto matches = linePattern.globalMatch(contents);
    while (matches.hasNext()) {
        const auto match = matches.next();
        result.insert(match.captured(1), parseColor(match.captured(2)));
    }
    const QList<QPair<QString, QString>> aliases{
        {"red", "color1"}, {"green", "color2"}, {"yellow", "color3"}, {"blue", "color4"},
        {"magenta", "color5"}, {"cyan", "color6"}, {"black", "color0"}
    };
    for (const auto &entry : aliases)
        if (result.contains(entry.second))
            result.insert(entry.first, result.value(entry.second));
    if (result.contains("cursor"))
        result.insert("primary", result.value("cursor"));
    else if (result.contains("color4"))
        result.insert("primary", result.value("color4"));
    if (result.isEmpty())
        fail("No supported kitty colors in " + path);
    return result;
}

QColor find(const Palette &palette, const QStringList &aliases) {
    for (const auto &alias : aliases) {
        const auto expected = normalizedKey(alias);
        for (auto entry = palette.constBegin(); entry != palette.constEnd(); ++entry)
            if (normalizedKey(entry.key()) == expected)
                return entry.value();
    }
    return {};
}

QColor pick(const Palette &palette, const QStringList &aliases, const QColor &fallback = {}) {
    const auto value = find(palette, aliases);
    if (value.isValid())
        return value;
    if (!fallback.isValid())
        fail("Palette is missing " + aliases.join(" / "));
    return fallback;
}

QColor mix(const QColor &base, const QColor &tint, double amount) {
    return QColor::fromRgbF(base.redF() * (1 - amount) + tint.redF() * amount,
                            base.greenF() * (1 - amount) + tint.greenF() * amount,
                            base.blueF() * (1 - amount) + tint.blueF() * amount,
                            base.alphaF() * (1 - amount) + tint.alphaF() * amount);
}

double luminance(const QColor &color) {
    const auto linear = [](double component) {
        return component <= .04045 ? component / 12.92 : std::pow((component + .055) / 1.055, 2.4);
    };
    return .2126 * linear(color.redF()) + .7152 * linear(color.greenF()) + .0722 * linear(color.blueF());
}

QColor composite(const QColor &foreground, const QColor &background) {
    const double alpha = foreground.alphaF();
    return QColor::fromRgbF(foreground.redF() * alpha + background.redF() * (1 - alpha),
                            foreground.greenF() * alpha + background.greenF() * (1 - alpha),
                            foreground.blueF() * alpha + background.blueF() * (1 - alpha));
}

double contrast(const QColor &foreground, const QColor &background) {
    const double first = luminance(composite(foreground, background));
    const double second = luminance(background);
    return (std::max(first, second) + .05) / (std::min(first, second) + .05);
}

QColor readableRedSuit(const QColor &card, const QColor &accent, const QColor &background) {
    const auto visibleCard = composite(card, background);
    const double preferred = luminance(visibleCard) > .45 ? .34 : .68;
    const double saturation = std::clamp(double(accent.hslSaturationF()), .5, .85);
    QColor best = QColor::fromHslF(0.0, saturation, preferred);
    double bestContrast = contrast(best, visibleCard);
    double bestDistance = 2;
    for (int step = 10; step <= 90; ++step) {
        const double lightness = step / 100.0;
        const auto candidate = QColor::fromHslF(0.0, saturation, lightness);
        const double ratio = contrast(candidate, visibleCard);
        const double distance = std::abs(lightness - preferred);
        if ((ratio >= 3.0 && distance < bestDistance)
            || (bestContrast < 3.0 && ratio > bestContrast)) {
            best = candidate;
            bestContrast = ratio;
            if (ratio >= 3.0)
                bestDistance = distance;
        }
    }
    return best;
}

Theme mapPalette(const Palette &palette, const QString &name) {
    const QColor defaultBackground("#182125");
    const QColor defaultText("#e3e9e5");
    const QColor background = pick(palette, {"background", "base", "m3Background", "mBackground", "mSurface", "surface"}, defaultBackground);
    const QColor text = pick(palette, {"foreground", "onBackground", "on_surface", "onSurface", "text", "mOnSurface"}, defaultText);
    const QColor accent = pick(palette, {"primary", "accent", "cursor", "mPrimary", "m3primary", "color4"}, QColor("#a9c7b0"));
    const QColor surface = pick(palette, {"surface_container_low", "surfaceContainerLow", "backgroundAlt", "mantle", "surface0", "mSurface"}, mix(background, text, .035));
    const QColor alternate = pick(palette, {"surface_alt", "surfaceAlt", "surface_container", "surfaceContainer", "backgroundGray", "surface1", "surfaceVariant", "mSurfaceVariant"}, mix(background, text, .07));
    const QColor border = pick(palette, {"outline", "border", "mOutline"}, mix(surface, accent, .25));
    const QColor accentText = pick(palette, {"on_primary", "onPrimary", "mOnPrimary", "onAccent", "accentText"},
                                   luminance(accent) > .4 ? QColor("#182125") : QColor("#e3e9e5"));
    const QColor card = pick(palette, {"card", "cardFront", "paper", "cardSurface"}, surface);
    const QColor redOverride = find(palette, {"red_suit", "redSuit"});
    QColor redSuit = redOverride;
    if (!redSuit.isValid()) {
        redSuit = find(palette, {"danger", "error", "mError", "red", "color1"});
        if (!redSuit.isValid() || contrast(redSuit, composite(card, background)) < 3.0)
            redSuit = readableRedSuit(card, accent, background);
    }
    Theme result;
    result.name = name;
    result.colors = {
        {"background", background}, {"surface", surface}, {"surface_alt", alternate},
        {"text", text}, {"muted", pick(palette, {"foregroundInactive", "on_surface_variant", "onSurfaceVariant", "mOnSurfaceVariant", "muted"}, mix(background, text, .65))},
        {"accent", accent}, {"accent_text", accentText}, {"border", border},
        {"red_suit", redSuit},
        {"black_suit", pick(palette, {"black_suit", "blackSuit", "spade", "club"}, text)},
        {"card", card},
        {"card_back", pick(palette, {"card_back", "cardBack", "back", "backColor"}, mix(surface, accent, .55))},
        {"selection", pick(palette, {"selection", "selected", "highlight"}, mix(surface, accent, .28))}
    };
    for (const auto &key : colorKeys())
        if (!result.colors.contains(key))
            result.colors.insert(key, pick(palette, {key}, result.color(key)));
    // Persisted channels are 8-bit; canonicalize before comparing refreshed palettes.
    return parseTheme(result.toJson());
}

QString directoryPalettePath(const QString &directory, const QString &kind) {
    QStringList names;
    if (kind == "caelestia")
        names = {"palette.json", "Colors.qml", "kitty.conf", "noctalia.json"};
    else if (kind == "noctalia")
        names = {"Colors.qml", "noctalia.json", "palette.json", "kitty.conf"};
    else
        names = {"Colors.qml", "theme.json", "palette.json", "colors.json", "noctalia.json", "kitty.conf"};
    for (const auto &name : names) {
        const auto path = QDir(directory).filePath(name);
        if (QFileInfo(path).isFile())
            return path;
    }
    return QDir(directory).filePath(names.first());
}

QString bindingPath(const QString &kind, QString requested) {
    if (kind == "symlink") {
        if (requested.isEmpty())
            fail("symlink source needs a path");
        return expandedPath(requested);
    }
    if (kind == "desktop")
        return expandedPath(requested.isEmpty() ? configHome() + "/themes/.caelestia-use" : requested);
    if (kind == "static") {
        if (requested.isEmpty())
            requested = "current";
        if (!requested.startsWith("~/") && !QFileInfo(requested).isAbsolute()
            && !requested.startsWith("./") && !requested.startsWith("../"))
            requested = QDir(configHome() + "/themes").filePath(requested);
        return expandedPath(requested);
    }
    if (kind == "file") {
        if (requested.isEmpty())
            fail("file source needs a palette path");
        return expandedPath(requested);
    }
    if (kind == "caelestia") {
        if (requested.isEmpty()) {
            requested = QStandardPaths::writableLocation(QStandardPaths::GenericStateLocation)
                + "/caelestia/shell-theme-palette.json";
            if (requested.startsWith('/'))
                return expandedPath(requested);
            requested = QDir::homePath() + "/.local/state/caelestia/shell-theme-palette.json";
        }
        return expandedPath(requested);
    }
    if (kind == "noctalia")
        return expandedPath(requested.isEmpty() ? configHome() + "/themes/.dynamic/noctalia" : requested);
    fail("Unknown source. Use file, desktop, static, caelestia, or noctalia.");
}

QString paletteFilePath(const QString &kind, const QString &binding) {
    const QFileInfo info(binding);
    if (info.isDir())
        return directoryPalettePath(info.absoluteFilePath(), kind);
    if (info.isFile() || info.isSymLink())
        return info.absoluteFilePath();
    if (binding.endsWith(".qml", Qt::CaseInsensitive) || binding.endsWith(".json", Qt::CaseInsensitive)
        || binding.endsWith(".conf", Qt::CaseInsensitive))
        return binding;
    return directoryPalettePath(binding, kind);
}

QJsonObject readJson(const QString &path) {
    const auto bytes = readBounded(path);
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        fail(path + ": expected a valid JSON object (" + error.errorString() + ")");
    return document.object();
}

Palette readPaletteFile(const QString &path) {
    if (path.endsWith(".qml", Qt::CaseInsensitive))
        return qmlPalette(path);
    if (path.endsWith(".conf", Qt::CaseInsensitive))
        return kittyPalette(path);
    return jsonPalette(readJson(path));
}

QString sourceName(const QString &kind, const QString &binding, const QString &paletteFile) {
    if (kind == "desktop" || kind == "static") {
        const auto canonical = QFileInfo(paletteFile).canonicalPath();
        return kind + ": " + QFileInfo(canonical.isEmpty() ? QFileInfo(binding).fileName() : canonical).fileName();
    }
    return kind + ": " + QFileInfo(paletteFile).completeBaseName();
}

void addPathAndParent(const QString &path, QStringList &result) {
    const QFileInfo info(path);
    const QString absolute = QDir::cleanPath(info.absoluteFilePath());
    if (info.exists() || info.isSymLink()) {
        result.append(absolute);
        if (info.isDir())
            result.append(info.absoluteFilePath());
        const QString parent = info.absoluteDir().absolutePath();
        if (QFileInfo(parent).isDir())
            result.append(parent);
        const auto canonical = info.canonicalFilePath();
        if (!canonical.isEmpty() && canonical != absolute) {
            result.append(canonical);
            const QString canonicalParent = QFileInfo(canonical).absolutePath();
            if (QFileInfo(canonicalParent).isDir())
                result.append(canonicalParent);
        }
        return;
    }
    QDir parent = info.absoluteDir();
    while (!parent.isRoot() && !QFileInfo(parent.absolutePath()).isDir())
        parent.cdUp();
    if (QFileInfo(parent.absolutePath()).isDir())
        result.append(parent.absolutePath());
}

Theme importResolved(const QString &kind, const QString &binding, const QString &paletteFile, const QString &name) {
    if (QFileInfo(binding).isDir()) {
        auto palette = readPaletteFile(paletteFile);
        for (const auto &companionName : {"palette.json", "kitty.conf"}) {
            const auto companion = QDir(binding).filePath(companionName);
            if (QFileInfo(companion).isFile() && QFileInfo(companion).absoluteFilePath() != QFileInfo(paletteFile).absoluteFilePath()) {
                const auto extra = readPaletteFile(companion);
                for (auto entry = extra.constBegin(); entry != extra.constEnd(); ++entry)
                    if (!palette.contains(entry.key()))
                        palette.insert(entry.key(), entry.value());
            }
        }
        return mapPalette(palette, name);
    }
    Q_UNUSED(kind);
    const auto data = paletteFile.endsWith(".json", Qt::CaseInsensitive) ? readJson(paletteFile) : QJsonObject{};
    if (!data.isEmpty() && data.value("version").toInt() == 1 && data.value("colors").isObject()) {
        bool complete = true;
        for (const auto &key : colorKeys())
            complete = complete && (key.startsWith("floral_") || data.value("colors").toObject().contains(key));
        if (complete) {
            auto theme = parseTheme(data);
            theme.name = name;
            return theme;
        }
    }
    if (!data.isEmpty())
        return mapPalette(jsonPalette(data), name);
    return mapPalette(readPaletteFile(paletteFile), name);
}

}

Theme importPalette(const QString &path, const QString &name) {
    const auto binding = expandedPath(path);
    const auto paletteFile = QFileInfo(binding).isDir() ? directoryPalettePath(binding, "file") : binding;
    const QString paletteName = name.isEmpty() ? QFileInfo(paletteFile).completeBaseName() : name;
    auto theme = importResolved("file", binding, paletteFile, paletteName);
    theme.sourceKind.clear();
    theme.sourcePath.clear();
    return theme;
}

Theme followTheme(const QString &kind, const QString &path) {
    if (!QStringList{"file", "symlink", "desktop", "static", "caelestia", "noctalia"}.contains(kind))
        fail("Unknown source: " + kind);
    const auto binding = bindingPath(kind, path);
    const auto paletteFile = paletteFilePath(kind, binding);
    const auto name = sourceName(kind, binding, paletteFile);
    auto theme = importResolved(kind, binding, paletteFile, name);
    theme.sourceKind = kind;
    theme.sourcePath = binding;
    return theme;
}

Theme refreshSource(const Theme &theme) {
    if (theme.sourceKind.isEmpty())
        return theme;
    return followTheme(theme.sourceKind, theme.sourcePath);
}

QStringList sourceWatchPaths(const Theme &theme) {
    if (theme.sourceKind.isEmpty() || theme.sourcePath.isEmpty())
        return {};
    QStringList paths;
    addPathAndParent(theme.sourcePath, paths);
    const auto paletteFile = paletteFilePath(theme.sourceKind, theme.sourcePath);
    addPathAndParent(paletteFile, paths);
    paths.removeDuplicates();
    return paths;
}

bool sourceTargetsFile(const Theme &theme, const QString &path) {
    const auto comparable = [](const QString &value) {
        const QFileInfo info(value);
        const auto canonical = info.canonicalFilePath();
        return QDir::cleanPath(canonical.isEmpty() ? info.absoluteFilePath() : canonical);
    };
    const auto target = comparable(path);
    for (const auto &candidate : sourceWatchPaths(theme)) {
        const QFileInfo info(candidate);
        if (info.isDir())
            continue;
        if ((info.isFile() || info.isSymLink()) && comparable(candidate) == target)
            return true;
    }
    return false;
}

}
