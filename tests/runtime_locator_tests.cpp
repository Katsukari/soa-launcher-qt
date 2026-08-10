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

class RuntimeLocatorTests final : public QObject
{
    Q_OBJECT

private slots:
    void resolves_runtime_folder_to_wine_entry_point()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString runtime = directory.filePath(QStringLiteral("runtime"));
        const QString bin = QDir(runtime).filePath(QStringLiteral("bin"));
        QVERIFY(QDir().mkpath(bin));

        const QString winePath = QDir(bin).filePath(QStringLiteral("wine"));
        QFile wine(winePath);
        QVERIFY(wine.open(QIODevice::WriteOnly));
        QVERIFY(wine.write("#!/bin/sh\nexit 0\n") > 0);
        wine.close();
        QVERIFY(wine.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                    | QFileDevice::ExeOwner));

        QCOMPARE(core::wine::macos::resolve_wine_executable(runtime), winePath);
        QCOMPARE(core::wine::macos::runtime_root_for_executable(winePath), runtime);
    }

    void prefers_wine_over_legacy_wine64()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString bin = directory.filePath(QStringLiteral("bin"));
        QVERIFY(QDir().mkpath(bin));
        for (const QString& name : {QStringLiteral("wine64"), QStringLiteral("wine")})
        {
            QFile file(QDir(bin).filePath(name));
            QVERIFY(file.open(QIODevice::WriteOnly));
            QVERIFY(file.write("#!/bin/sh\nexit 0\n") > 0);
            file.close();
            QVERIFY(file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                        | QFileDevice::ExeOwner));
        }
        QCOMPARE(core::wine::macos::resolve_wine_executable(directory.path()),
                 QDir(bin).filePath(QStringLiteral("wine")));
    }

    void probes_script_runtime_without_creating_a_prefix()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString bin = directory.filePath(QStringLiteral("bin"));
        QVERIFY(QDir().mkpath(bin));
        const QString winePath = QDir(bin).filePath(QStringLiteral("wine"));
        QFile wine(winePath);
        QVERIFY(wine.open(QIODevice::WriteOnly));
        QVERIFY(wine.write("#!/bin/sh\necho wine-test-11.0\n") > 0);
        wine.close();
        QVERIFY(wine.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                    | QFileDevice::ExeOwner));

        const auto probe = core::wine::macos::probe_runtime(directory.path());
        QVERIFY2(probe.usable, qPrintable(probe.failure));
        QCOMPARE(probe.executable, winePath);
        QCOMPARE(probe.version, QStringLiteral("wine-test-11.0"));
    }

#if defined(Q_OS_MACOS)
    void configures_crossover_runtime_root()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString cxRoot = directory.filePath(
            QStringLiteral("CrossOver.app/Contents/SharedSupport/CrossOver"));
        const QString bin = QDir(cxRoot).filePath(QStringLiteral("bin"));
        QVERIFY(QDir().mkpath(bin));
        const QString winePath = QDir(bin).filePath(QStringLiteral("wine"));
        QFile wine(winePath);
        QVERIFY(wine.open(QIODevice::WriteOnly));
        QVERIFY(wine.write("#!/bin/sh\nexit 0\n") > 0);
        wine.close();
        QVERIFY(wine.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                    | QFileDevice::ExeOwner));

        QProcessEnvironment environment;
        core::wine::macos::apply_runtime_environment(environment, winePath);
        QCOMPARE(environment.value(QStringLiteral("CX_ROOT")), cxRoot);
    }
#endif

};

QTEST_MAIN(RuntimeLocatorTests)
#include "runtime_locator_tests.moc"
