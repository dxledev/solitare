#include "cli.h"
#include "core/storage.h"
#include "core/theme.h"
#include "ui/window.h"
#include <QApplication>
#include <QDir>
#include <QLockFile>
#include <QTextStream>
int main(int argc, char **argv) {
  QCoreApplication::setApplicationName("solitare");
  QCoreApplication::setApplicationVersion("1.0.0");
  QStringList args;
  for (int i = 1; i < argc; ++i)
    args.append(QString::fromLocal8Bit(argv[i]));
  try {
    bool command = false;
    for (const auto &arg : args)
      if (arg == "theme" || arg == "--help" || arg == "-h" ||
          arg == "--version")
        command = true;
    if (command) {
      QCoreApplication app(argc, argv);
      return solitare::runCli(args);
    }
    const auto result = solitare::runCli(args);
    if (result != -1)
      return result;
    const auto directory = solitare::configDirectory(args);
    if (!qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY") &&
        qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
      qputenv("QT_QPA_PLATFORM", "wayland");
    QApplication app(argc, argv);
    app.setStyle("Fusion");
    app.setDesktopFileName("io.github.solitare");
    QDir().mkpath(directory);
    QLockFile session(directory + "/session.lock");
    if (!session.tryLock(0)) {
      QTextStream(stderr) << "Solitare is already using this saved deal.\n";
      return 1;
    }
    auto theme = solitare::ensureTheme(directory + "/theme.json");
    solitare::Window window(directory, theme);
    window.show();
    return app.exec();
  } catch (const std::exception &error) {
    QTextStream(stderr) << "solitare: " << error.what() << '\n';
    return 1;
  }
}
