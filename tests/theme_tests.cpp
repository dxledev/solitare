#include "core/storage.h"
#include "core/theme.h"
#include "core/theme_source.h"
#include "ui/theme_watcher.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#include <algorithm>
#include <cmath>
#include <cstdio>

using namespace solitare;

namespace {

bool writeText(const QString &path, const QString &text) {
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return false;
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    const auto bytes = text.toUtf8();
    return file.write(bytes) == bytes.size() && file.commit();
}

QString qmlPalette(const QString &accent, const QString &background = "#1b1a18") {
    return QStringLiteral(R"(pragma Singleton
import QtQuick
Singleton {
    readonly property color background: "%1"
    readonly property color foreground: "#e8e1d8"
    readonly property color foregroundInactive: "#aaa198"
    readonly property color primary: "%2"
    readonly property color red: "#e27676"
})").arg(background, accent);
}

bool replaceSymlink(const QString &target, const QString &linkPath) {
    const QString temporaryPath = linkPath + ".new";
    QFile::remove(temporaryPath);
    if (!QFile::link(target, temporaryPath))
        return false;
    return std::rename(QFile::encodeName(temporaryPath).constData(),
                       QFile::encodeName(linkPath).constData()) == 0;
}

QString noctaliaPalette(const QString &accent, const QString &background = "#15191d") {
    const QJsonObject colors{
        {"mBackground", background}, {"mSurface", "#1d232a"}, {"mOnSurface", "#e7e8eb"},
        {"mPrimary", accent}, {"mSurfaceVariant", "#252d35"}, {"mOnSurfaceVariant", "#aeb4bd"},
        {"mOutline", "#3b424c"}, {"mError", "#ff5555"}, {"color0", "#000000"}
    };
    return QString::fromUtf8(QJsonDocument(QJsonObject{
        {"mode", "dark"}, {"dark", colors}, {"light", colors}
    }).toJson());
}

double relativeLuminance(const QColor &color) {
    const auto linear = [](double component) {
        return component <= .04045 ? component / 12.92 : std::pow((component + .055) / 1.055, 2.4);
    };
    return .2126 * linear(color.redF()) + .7152 * linear(color.greenF()) + .0722 * linear(color.blueF());
}

double contrastRatio(const QColor &left, const QColor &right) {
    const double first = relativeLuminance(left);
    const double second = relativeLuminance(right);
    return (std::max(first, second) + .05) / (std::min(first, second) + .05);
}

}

class ThemeTests : public QObject {
    Q_OBJECT
private slots:
    void lofiRoundTripAndAlphaFirstJson();
    void floralDefaultsAndLegacyThemes();
    void importsJsonQmlAndKittyPalettes();
    void sourceMappingTracksLightAndDarkPalettes();
    void watcherTracksSelectorAndKeepsLastValidTheme();
};

void ThemeTests::lofiRoundTripAndAlphaFirstJson() {
    QCOMPARE(colorKeys().size(), 18);
    const auto lofi = presetTheme("lofi");
    QCOMPARE(lofi.colors.size(), colorKeys().size());
    QCOMPARE(parseTheme(lofi.toJson()), lofi);
    QVERIFY_EXCEPTION_THROWN(presetTheme("forest"), std::invalid_argument);

    const auto translucent = setColors(lofi, {"accent=#80402010"});
    QCOMPARE(translucent.color("accent"), QColor::fromString("#80402010"));
    QCOMPARE(translucent.toJson().value("colors").toObject().value("accent").toString(), QString("#80402010"));
    QCOMPARE(parseTheme(translucent.toJson()), translucent);

    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto path = temporary.filePath("config/theme.json");
    saveTheme(path, translucent);
    QCOMPARE(loadTheme(path), translucent);
    QCOMPARE(ensureTheme(temporary.filePath("new/theme.json")).name, QString("lofi"));
}

void ThemeTests::floralDefaultsAndLegacyThemes() {
    const auto theme = presetTheme("lofi");
    auto legacy = theme.toJson();
    auto colors = legacy.value("colors").toObject();
    for (const auto &key : colorKeys())
        if (key.startsWith("floral_"))
            colors.remove(key);
    legacy.insert("colors", colors);
    QCOMPARE(parseTheme(legacy), theme);
    QCOMPARE(theme.color("floral_red"), QColor("#d94528"));
    QCOMPARE(theme.color("floral_blue"), QColor("#193e60"));
    const auto custom = setColors(theme, {"floral_red=#884422", "floral_gold=#e0ba60"});
    QCOMPARE(parseTheme(custom.toJson()), custom);
    QCOMPARE(custom.color("floral_red"), QColor("#884422"));
    colors.insert("floral_red", "invalid");
    legacy.insert("colors", colors);
    QVERIFY_EXCEPTION_THROWN(parseTheme(legacy), std::invalid_argument);

    QTemporaryDir temporary;
    const auto path = temporary.filePath("legacy.json");
    colors.remove("floral_red");
    colors.insert("card", "#f4e6be");
    legacy.insert("colors", colors);
    QVERIFY(writeText(path, QString::fromUtf8(QJsonDocument(legacy).toJson())));
    const auto imported = importPalette(path);
    QCOMPARE(imported.color("card"), QColor("#f4e6be"));
    QCOMPARE(imported.color("floral_red"), QColor("#d94528"));
    const auto partialPath = temporary.filePath("partial.json");
    const QJsonObject partial{{"background", "#20282e"}, {"foreground", "#e6e6dd"},
                              {"floral_red", "#b03020"}};
    QVERIFY(writeText(partialPath, QString::fromUtf8(QJsonDocument(partial).toJson())));
    QCOMPARE(importPalette(partialPath).color("floral_red"), QColor("#b03020"));
}

void ThemeTests::importsJsonQmlAndKittyPalettes() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());

    const auto jsonPath = temporary.filePath("palette.json");
    const QJsonObject jsonPalette{
        {"background", "#80402010"}, {"foreground", "#e8e1d8"}, {"primary", "#c7a77a"},
        {"red_suit", "#e27676"}, {"black_suit", "#252525"}, {"card", "#f2ece1"},
        {"card_back", "#465d66"}, {"selection", "#d7b77d"}
    };
    QVERIFY(writeText(jsonPath, QString::fromUtf8(QJsonDocument(QJsonObject{{"colors", jsonPalette}}).toJson())));
    const auto jsonTheme = importPalette(jsonPath);
    QCOMPARE(jsonTheme.color("background"), QColor::fromString("#80402010"));
    QCOMPARE(jsonTheme.color("text"), QColor("#e8e1d8"));
    QCOMPARE(jsonTheme.color("red_suit"), QColor("#e27676"));

    const auto qmlPath = temporary.filePath("Colors.qml");
    QVERIFY(writeText(qmlPath, QStringLiteral(R"(pragma Singleton
import QtQuick
Singleton {
    property color background: Qt.rgba(244/255, 224/255, 180/255, 128/255)
    property color foreground: "#e8e1d8"
    property color primary: "#c7a77a"
})")));
    const auto qmlTheme = importPalette(qmlPath);
    QCOMPARE(qmlTheme.color("background").red(), 244);
    QCOMPARE(qmlTheme.color("background").green(), 224);
    QCOMPARE(qmlTheme.color("background").blue(), 180);
    QCOMPARE(qmlTheme.color("background").alpha(), 128);
    QCOMPARE(qmlTheme.toJson().value("colors").toObject().value("background").toString(), QString("#80f4e0b4"));

    const auto kittyPath = temporary.filePath("kitty.conf");
    QVERIFY(writeText(kittyPath, "background #182028\nforeground #e5e0d8\ncursor #ccaa77\ncolor1 #e27676\ncolor0 #252525\n"));
    const auto kittyTheme = importPalette(kittyPath);
    QCOMPARE(kittyTheme.color("background"), QColor("#182028"));
    QCOMPARE(kittyTheme.color("accent"), QColor("#ccaa77"));
    QCOMPARE(kittyTheme.color("red_suit"), QColor("#e27676"));
}

void ThemeTests::sourceMappingTracksLightAndDarkPalettes() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto lightPath = temporary.filePath("light.json");
    const QJsonObject lightColors{
        {"background", "#f5f2ec"}, {"foreground", "#222522"}, {"primary", "#315c48"},
        {"surfaceAlt", "#e8e3d9"}, {"danger", "#f5dddd"}
    };
    QVERIFY(writeText(lightPath, QString::fromUtf8(QJsonDocument(QJsonObject{{"colors", lightColors}}).toJson())));
    const auto light = importPalette(lightPath);
    QVERIFY(light.color("card").lightnessF() > .8);
    QVERIFY(light.color("black_suit").lightnessF() < .2);
    QVERIFY(light.color("card_back") != presetTheme("lofi").color("card_back"));
    QVERIFY(light.color("selection") != presetTheme("lofi").color("selection"));
    QVERIFY(light.color("red_suit") != QColor("#f5dddd"));
    QVERIFY(contrastRatio(light.color("red_suit"), light.color("card")) >= 3.0);

    const auto darkPath = temporary.filePath("dark.json");
    QVERIFY(writeText(darkPath, noctaliaPalette("#55aa88", "#101820")));
    const auto dark = importPalette(darkPath);
    QCOMPARE(dark.color("background"), QColor("#101820"));
    QCOMPARE(dark.color("surface"), QColor("#1d232a"));
    QCOMPARE(dark.color("surface_alt"), QColor("#252d35"));
    QCOMPARE(dark.color("black_suit"), QColor("#e7e8eb"));
    QVERIFY(dark.color("card").lightnessF() < .2);
    QCOMPARE(dark.color("red_suit"), QColor("#ff5555"));

    const auto explicitRedPath = temporary.filePath("explicit-red.json");
    const QJsonObject explicitColors{
        {"background", "#101820"}, {"foreground", "#e7e8eb"}, {"primary", "#55aa88"},
        {"red_suit", "#102030"}, {"color0", "#000000"}
    };
    QVERIFY(writeText(explicitRedPath, QString::fromUtf8(QJsonDocument(QJsonObject{{"colors", explicitColors}}).toJson())));
    const auto explicitRed = importPalette(explicitRedPath);
    QCOMPARE(explicitRed.color("red_suit"), QColor("#102030"));
    QCOMPARE(explicitRed.color("black_suit"), QColor("#e7e8eb"));
}

void ThemeTests::watcherTracksSelectorAndKeepsLastValidTheme() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto firstDirectory = temporary.filePath("themes/one");
    const auto secondDirectory = temporary.filePath("themes/two");
    const auto staticDirectory = temporary.filePath("themes/static");
    const auto caelestiaDirectory = temporary.filePath("themes/.dynamic/caelestia");
    const auto noctaliaDirectory = temporary.filePath("themes/.dynamic/noctalia");
    QVERIFY(QDir().mkpath(firstDirectory));
    QVERIFY(QDir().mkpath(secondDirectory));
    QVERIFY(QDir().mkpath(staticDirectory));
    QVERIFY(QDir().mkpath(caelestiaDirectory));
    QVERIFY(QDir().mkpath(noctaliaDirectory));
    QVERIFY(writeText(QDir(firstDirectory).filePath("Colors.qml"), qmlPalette("#aa1122")));
    QVERIFY(writeText(QDir(secondDirectory).filePath("Colors.qml"), qmlPalette("#bb2233")));
    QVERIFY(writeText(QDir(staticDirectory).filePath("Colors.qml"), qmlPalette("#557755")));
    QVERIFY(writeText(QDir(caelestiaDirectory).filePath("Colors.qml"), qmlPalette("#668866")));
    QVERIFY(writeText(QDir(noctaliaDirectory).filePath("noctalia.json"), noctaliaPalette("#779977")));

    const auto selector = temporary.filePath("themes/.caelestia-use");
    QVERIFY(QFile::link(firstDirectory, selector));
    const auto configPath = temporary.filePath("app/theme.json");
    const auto followed = followTheme("desktop", selector);
    saveTheme(configPath, followed);

    ThemeWatcher watcher(configPath, loadTheme(configPath));
    QSignalSpy changes(&watcher, &ThemeWatcher::changed);
    QSignalSpy rejections(&watcher, &ThemeWatcher::rejected);
    QTRY_COMPARE_WITH_TIMEOUT(watcher.theme().color("accent"), QColor("#aa1122"), 2500);

    QVERIFY(writeText(QDir(firstDirectory).filePath("Colors.qml"), qmlPalette("#112233")));
    QTRY_COMPARE_WITH_TIMEOUT(watcher.theme().color("accent"), QColor("#112233"), 2500);

    QVERIFY(replaceSymlink(secondDirectory, selector));
    QTRY_COMPARE_WITH_TIMEOUT(watcher.theme().color("accent"), QColor("#bb2233"), 2500);
    const auto lastValid = watcher.theme();
    QTest::qWait(200);
    const auto stableModified = QFileInfo(configPath).lastModified();
    QTest::qWait(800);
    QCOMPARE(QFileInfo(configPath).lastModified(), stableModified);

    QVERIFY(writeText(QDir(secondDirectory).filePath("Colors.qml"), "not a palette"));
    QTRY_VERIFY_WITH_TIMEOUT(!rejections.isEmpty(), 2500);
    QCOMPARE(watcher.theme(), lastValid);

    QVERIFY(writeText(QDir(secondDirectory).filePath("Colors.qml"), qmlPalette("#445566")));
    QTRY_COMPARE_WITH_TIMEOUT(watcher.theme().color("accent"), QColor("#445566"), 2500);
    QCOMPARE(loadTheme(configPath).color("accent"), QColor("#445566"));
    QVERIFY(changes.count() >= 3);

    const QList<QPair<QString, QString>> transitions{
        {"static", staticDirectory}, {"caelestia", caelestiaDirectory}, {"noctalia", noctaliaDirectory},
        {"static", staticDirectory}, {"noctalia", noctaliaDirectory}, {"caelestia", caelestiaDirectory}
    };
    for (const auto &transition : transitions) {
        const auto configured = followTheme(transition.first, transition.second);
        updateThemeLocked(configPath, [configured](const Theme &) { return configured; });
        QTRY_COMPARE_WITH_TIMEOUT(watcher.theme().sourceKind, transition.first, 2500);
        QCOMPARE(watcher.theme().color("accent"), transition.first == "static" ? QColor("#557755")
                 : transition.first == "caelestia" ? QColor("#668866") : QColor("#779977"));
    }
    QVERIFY(writeText(QDir(caelestiaDirectory).filePath("Colors.qml"), qmlPalette("#99aabb")));
    QTRY_COMPARE_WITH_TIMEOUT(watcher.theme().color("accent"), QColor("#99aabb"), 2500);

    const auto manual = setColors(presetTheme("lofi"), {"accent=#987654"});
    QVERIFY(!sourceTargetsFile(followed, configPath));
    QVERIFY(sourceTargetsFile(followTheme("file", configPath), configPath));
    saveTheme(configPath, manual);
    QTRY_COMPARE_WITH_TIMEOUT(watcher.theme(), manual, 2500);
}

QTEST_GUILESS_MAIN(ThemeTests)
#include "theme_tests.moc"
