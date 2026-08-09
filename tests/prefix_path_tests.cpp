#include <QtTest>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>

#include <utility>

#include "core/game/GameVersion.hpp"
#include "core/wine/GameSession.hpp"
#include "core/wine/MacWineRuntime.hpp"
#include "core/wine/PrefixInspector.hpp"
#include "core/wine/ProcessRunner.hpp"
#include "core/wine/RuntimeLocator.hpp"
#include "core/wine/WineProcess.hpp"
#include "util/DesktopEntry.hpp"
#include "util/LaunchArguments.hpp"

class PrefixPathTests final : public QObject
{
    Q_OBJECT

private slots:
    void maps_prefix_file_to_windows_c_path()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString prefix = directory.filePath(QStringLiteral("prefix"));
        const QString gameDirectory = QDir(prefix).filePath(
            QStringLiteral("drive_c/users/test/Story Of Alicia"));
        QVERIFY(QDir().mkpath(gameDirectory));

        const QString executable = QDir(gameDirectory).filePath(QStringLiteral("Alicia.exe"));
        QFile file(executable);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.write("MZ") > 0);
        file.close();

        QString error;
        QCOMPARE(core::wine::windows_path_for_prefix_file(prefix, executable, &error),
                 QStringLiteral("C:\\users\\test\\Story Of Alicia\\Alicia.exe"));
        QVERIFY(error.isEmpty());
    }

    void rejects_executable_outside_wine_c_drive()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString prefix = directory.filePath(QStringLiteral("prefix"));
        QVERIFY(QDir().mkpath(QDir(prefix).filePath(QStringLiteral("drive_c"))));

        const QString executable = directory.filePath(QStringLiteral("Alicia.exe"));
        QFile file(executable);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.write("MZ") > 0);
        file.close();

        QString error;
        QVERIFY(core::wine::windows_path_for_prefix_file(prefix, executable, &error).isEmpty());
        QVERIFY(error.contains(QStringLiteral("outside"), Qt::CaseInsensitive));
    }

    void rejects_symlink_escape_from_wine_c_drive()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString prefix = directory.filePath(QStringLiteral("prefix"));
        const QString gameDirectory = QDir(prefix).filePath(QStringLiteral("drive_c/game"));
        QVERIFY(QDir().mkpath(gameDirectory));

        const QString outside = directory.filePath(QStringLiteral("outside-Alicia.exe"));
        QFile file(outside);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.write("MZ") > 0);
        file.close();

        const QString linked = QDir(gameDirectory).filePath(QStringLiteral("Alicia.exe"));
        QVERIFY(QFile::link(outside, linked));

        QString error;
        QVERIFY(core::wine::windows_path_for_prefix_file(prefix, linked, &error).isEmpty());
        QVERIFY(error.contains(QStringLiteral("outside"), Qt::CaseInsensitive));
    }

};

QTEST_MAIN(PrefixPathTests)
#include "prefix_path_tests.moc"
