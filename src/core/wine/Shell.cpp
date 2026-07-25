#include "core/wine/Shell.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QSaveFile>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTimer>

#include <utility>
#include <chrono>
#include <thread>

#include "core/Log.hpp"
#include "core/game/GameVersion.hpp"
#include "core/runtime/RuntimeManager.hpp"
#include "core/runtime/RuntimeProvider.hpp"
#include "core/wine/PrefixInspector.hpp"
#include "core/wine/MacWineRuntime.hpp"
#include "core/wine/WineRegistry.hpp"
#include "util/Config.hpp"
#include "util/LaunchArguments.hpp"
#include "util/LanguageManager.hpp"
#include <spdlog/spdlog.h>

namespace core::wine
{
    using util::config::Config;

    namespace
    {
        constexpr qsizetype k_max_captured_output = 1024 * 1024;
        constexpr int k_runtime_probe_timeout_ms = 15 * 1000;
        constexpr int k_registry_timeout_ms = 45 * 1000;
        constexpr int k_game_probe_timeout_ms = 5 * 1000;
#if defined(Q_OS_MACOS)



        constexpr int k_game_initial_probe_delay_ms = 8000;
        constexpr int k_game_start_probe_interval_ms = 3000;
        constexpr int k_game_running_probe_interval_ms = 10000;
#else
        constexpr int k_game_initial_probe_delay_ms = 100;
        constexpr int k_game_start_probe_interval_ms = 350;
        constexpr int k_game_running_probe_interval_ms = 2000;
#endif
        constexpr int k_game_start_max_attempts = 60;
        constexpr int k_game_wrapper_exit_grace_ms = 8000;
        constexpr int k_game_absolute_start_timeout_ms = 120 * 1000;
        constexpr int k_game_probe_failure_limit = 4;
        constexpr int k_game_attached_probe_failure_limit = 8;
        constexpr int k_game_missing_confirmation_limit = 3;
#if defined(Q_OS_MACOS)
        constexpr int k_deep_diagnostic_sample_delay_ms = 40 * 1000;
#endif



        constexpr int k_prefix_validation_retries = 20;
        constexpr int k_prefix_validation_retry_ms = 500;
        const QString k_installing_message = QStringLiteral("Installing components (this can take a while)...");

        void prune_old_diagnostics(const QString& directory, const int keep = 12)
        {
            QDir dir(directory);
            for (const QString& pattern : {
                     QStringLiteral("alicia-*.log"),
                     QStringLiteral("alicia-*.timeline.jsonl"),
                     QStringLiteral("alicia-*.log.sample.txt"),
                     QStringLiteral("alicia-*.log.sample-status.txt")})
            {
                const QFileInfoList files = dir.entryInfoList(
                    {pattern}, QDir::Files, QDir::Time);
                for (int index = keep; index < files.size(); ++index)
                {
                    if (!QFile::remove(files[index].absoluteFilePath()))
                        SPDLOG_DEBUG("diagnostics: could not remove old artifact {}",
                                     files[index].absoluteFilePath().toStdString());
                }
            }
        }

        QStringList redacted_command_args(const QStringList& arguments)
        {
            QStringList redacted = arguments;
            for (qsizetype index = 0; index < redacted.size(); ++index)
            {
                if (redacted[index].compare(QStringLiteral("-OP"), Qt::CaseInsensitive) == 0
                    && index + 1 < redacted.size())
                {
                    redacted[index + 1] = QStringLiteral("[REDACTED]");
                }
            }
            return redacted;
        }


        QString redact_sensitive_text(QString text)
        {
            const QString token = Config::instance().token();
            if (!token.isEmpty())
                text.replace(token, QStringLiteral("[REDACTED]"));

            static const QRegularExpression operationToken(
                QStringLiteral(R"((?i)(-OP\s+)\[[^\]]*\])"));
            text.replace(operationToken, QStringLiteral("\\1[REDACTED]"));
            return text;
        }



#if defined(Q_OS_MACOS)
        quint16 pe_u16(const QByteArray& data, const qsizetype offset)
        {
            if (offset < 0 || offset + 2 > data.size())
                return 0;
            const auto* p = reinterpret_cast<const unsigned char*>(data.constData() + offset);
            return quint16(p[0]) | (quint16(p[1]) << 8);
        }

        quint32 pe_u32(const QByteArray& data, const qsizetype offset)
        {
            if (offset < 0 || offset + 4 > data.size())
                return 0;
            const auto* p = reinterpret_cast<const unsigned char*>(data.constData() + offset);
            return quint32(p[0]) | (quint32(p[1]) << 8) | (quint32(p[2]) << 16) | (quint32(p[3]) << 24);
        }

        struct PeImageInfo
        {
            bool valid {};
            bool pe32 {};
            quint64 image_base {};
            quint32 entry_point_rva {};
        };

        PeImageInfo inspect_pe_image(const QString& path)
        {
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly))
                return {};
            const QByteArray data = file.read(4096);
            if (data.size() < 0x100 || data[0] != 'M' || data[1] != 'Z')
                return {};
            const quint32 peOffset = pe_u32(data, 0x3c);
            if (peOffset + 0x80u > quint32(data.size())
                || data.mid(peOffset, 4) != QByteArray("PE\0\0", 4))
                return {};
            const qsizetype optional = qsizetype(peOffset) + 24;
            const quint16 magic = pe_u16(data, optional);
            PeImageInfo info;
            info.pe32 = magic == 0x10b;
            if (!info.pe32 && magic != 0x20b)
                return {};
            info.entry_point_rva = pe_u32(data, optional + 16);
            info.image_base = info.pe32 ? pe_u32(data, optional + 28)
                                        : quint64(pe_u32(data, optional + 24))
                                            | (quint64(pe_u32(data, optional + 28)) << 32);
            info.valid = info.image_base != 0;
            return info;
        }

        void append_macos_crash_analysis(const QString& diagnosticPath,
                                         const QString& executablePath)
        {
            if (diagnosticPath.isEmpty() || executablePath.isEmpty())
                return;

            QFile input(diagnosticPath);
            if (!input.open(QIODevice::ReadOnly | QIODevice::Text))
                return;
            const QString log = QString::fromUtf8(input.readAll());
            input.close();

            static const QRegularExpression exceptionPattern(
                QStringLiteral(R"(Unhandled exception code\s+([0-9a-fA-F]+).*?addr\s+0x([0-9a-fA-F]+))"));
            QRegularExpressionMatch exceptionMatch;
            auto iterator = exceptionPattern.globalMatch(log);
            while (iterator.hasNext())
                exceptionMatch = iterator.next();

            QFile output(diagnosticPath);
            if (!output.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
                return;
            QTextStream trace(&output);
            trace << "\n=== AUTOMATIC FIRST-FAULT ANALYSIS ===\n";

            const PeImageInfo pe = inspect_pe_image(executablePath);
            trace << "Executable: " << executablePath << "\n"
                  << "PE valid=" << pe.valid << " pe32=" << pe.pe32
                  << " preferred_image_base=0x" << QString::number(pe.image_base, 16)
                  << " entry_rva=0x" << QString::number(pe.entry_point_rva, 16) << "\n";

            if (!exceptionMatch.hasMatch())
            {
                trace << "No final unhandled exception record was found. The process may have exited voluntarily or been terminated externally.\n";
                trace.flush();
                return;
            }

            bool addressOk = false;
            const quint64 fault = exceptionMatch.captured(2).toULongLong(&addressOk, 16);
            trace << "Exception code=0x" << exceptionMatch.captured(1).toLower()
                  << " fault_address=0x" << QString::number(fault, 16) << "\n";

            static const QRegularExpression moduleBasePattern(
                QStringLiteral(R"(LdrGetDllHandleEx[^\n]*Alicia\.exe[^\n]*->\s*([0-9a-fA-F]+))"),
                QRegularExpression::CaseInsensitiveOption);
            quint64 observedBase = 0;
            auto baseIterator = moduleBasePattern.globalMatch(log);
            while (baseIterator.hasNext())
            {
                const auto match = baseIterator.next();
                observedBase = match.captured(1).toULongLong(nullptr, 16);
            }
            const quint64 imageBase = observedBase != 0 ? observedBase : pe.image_base;
            if (addressOk && imageBase != 0 && fault >= imageBase)
            {
                const quint64 rva = fault - imageBase;
                trace << "Observed image base=0x" << QString::number(imageBase, 16)
                      << " fault_rva=0x" << QString::number(rva, 16) << "\n";
                trace << "Classification: faulting instruction belongs to Alicia.exe itself. "
                         "This identifies where an invalid engine state was consumed, not necessarily where it was created.\n";

                const qsizetype exceptionOffset = exceptionMatch.capturedStart();
                const qsizetype contextStart = qMax<qsizetype>(0, exceptionOffset - 5000);
                const qsizetype contextLength = qMin<qsizetype>(12000, log.size() - contextStart);
                QString context = log.mid(contextStart, contextLength);
                QStringList selected;
                for (const QString& line : context.split(QLatin1Char('\n')))
                {
                    if (line.contains(QStringLiteral("Unhandled exception"), Qt::CaseInsensitive)
                        || line.contains(QStringLiteral("Register dump"), Qt::CaseInsensitive)
                        || line.contains(QRegularExpression(QStringLiteral(R"(\b(EIP|ESP|EBP|EAX|EBX|ECX|EDX|ESI|EDI)\b)"), QRegularExpression::CaseInsensitiveOption))
                        || line.contains(QStringLiteral("Backtrace"), Qt::CaseInsensitive)
                        || line.contains(QStringLiteral("Stack dump"), Qt::CaseInsensitive))
                        selected.append(line);
                }
                trace << "\n--- register/stack evidence ---\n"
                      << selected.join(QLatin1Char('\n'))
                      << "\n--- end register/stack evidence ---\n";

                QString disassembler;
                for (const QString& candidate : {QStringLiteral("llvm-objdump"), QStringLiteral("objdump"), QStringLiteral("gobjdump")})
                {
                    disassembler = QStandardPaths::findExecutable(candidate);
                    if (!disassembler.isEmpty())
                        break;
                }
                if (!disassembler.isEmpty())
                {
                    const quint64 start = fault > 96 ? fault - 96 : imageBase;
                    const quint64 stop = fault + 128;
                    QStringList arguments;
                    if (QFileInfo(disassembler).fileName().contains(QStringLiteral("llvm"), Qt::CaseInsensitive))
                        arguments << QStringLiteral("--x86-asm-syntax=intel");
                    arguments << QStringLiteral("-d")
                              << QStringLiteral("--start-address=0x%1").arg(QString::number(start, 16))
                              << QStringLiteral("--stop-address=0x%1").arg(QString::number(stop, 16))
                              << executablePath;
                    QProcess disassembly;
                    disassembly.setProcessChannelMode(QProcess::MergedChannels);
                    disassembly.start(disassembler, arguments);
                    const bool finished = disassembly.waitForStarted(5000)
                        && disassembly.waitForFinished(20000);
                    if (!finished && disassembly.state() != QProcess::NotRunning)
                    {
                        disassembly.kill();
                        disassembly.waitForFinished(2000);
                    }
                    trace << "\n--- disassembly around fault ---\n"
                          << "Command: " << disassembler << " " << arguments.join(QLatin1Char(' ')) << "\n"
                          << QString::fromUtf8(disassembly.readAll())
                          << "\n--- end disassembly ---\n";
                }
                else
                {
                    trace << "No llvm-objdump/objdump was found. Install Xcode Command Line Tools to append automatic PE disassembly.\n";
                }

                const bool sawD3dCreate = log.contains(QStringLiteral("Direct3DCreate9"), Qt::CaseInsensitive);
                const bool sawCreateDevice = log.contains(QStringLiteral("CreateDevice"), Qt::CaseInsensitive);
                const bool sawPhysx = log.contains(QStringLiteral("PhysXLoader.dll"), Qt::CaseInsensitive);
                const bool missingDll = log.contains(QRegularExpression(
                    QStringLiteral(R"((Library .* not found|failed to load .*\.dll|status c0000135))"),
                    QRegularExpression::CaseInsensitiveOption));
                trace << "\n--- subsystem evidence ---\n"
                      << "Direct3DCreate9 observed=" << sawD3dCreate << "\n"
                      << "CreateDevice text observed=" << sawCreateDevice << "\n"
                      << "PhysXLoader observed=" << sawPhysx << "\n"
                      << "Missing-DLL signature observed=" << missingDll << "\n";
                if (missingDll)
                    trace << "Classification: loader failure evidence exists; inspect the first missing-DLL line above.\n";
                else if (sawD3dCreate && !sawCreateDevice)
                    trace << "Classification: crash follows D3D9 entry but no CreateDevice trace was captured; graphics initialization remains the leading boundary.\n";
                else if (sawCreateDevice)
                    trace << "Classification: D3D device creation was reached; inspect HRESULT/return evidence and the disassembly before blaming the renderer.\n";
                else
                    trace << "Classification: no conclusive graphics call boundary was captured; use the disassembly and register evidence as source of truth.\n";
            }
            else
            {
                trace << "The fault address could not be mapped into Alicia.exe using the observed/preferred image base.\n";
            }
            trace << "=== END AUTOMATIC FIRST-FAULT ANALYSIS ===\n";
            trace.flush();
        }
#endif

        QString outcome_name(const CommandOutcome outcome)
        {
            switch (outcome)
            {
                case CommandOutcome::Success: return QStringLiteral("success");
                case CommandOutcome::NonZeroExit: return QStringLiteral("non-zero exit");
                case CommandOutcome::FailedToStart: return QStringLiteral("failed to start");
                case CommandOutcome::Crashed: return QStringLiteral("crashed");
                case CommandOutcome::TimedOut: return QStringLiteral("timed out");
                case CommandOutcome::Cancelled: return QStringLiteral("cancelled");
            }
            return QStringLiteral("unknown");
        }

        QString command_failure_message(const QString& action,
                                        const command_result& result)
        {
            QString message;
            if (result.exit_code >= 0)
            {
                message = QStringLiteral("%1 (%2, exit %3).")
                    .arg(action, outcome_name(result.outcome))
                    .arg(result.exit_code);
            }
            else
            {
                message = QStringLiteral("%1 (%2).")
                    .arg(action, outcome_name(result.outcome));
            }

            QStringList details;
            const QString processError =
                redact_sensitive_text(result.error_message).trimmed();
            if (!processError.isEmpty())
                details.append(processError);

            for (const QString& rawLine :
                 redact_sensitive_text(result.output)
                     .split(QLatin1Char('\n'), Qt::SkipEmptyParts))
            {
                const QString line = rawLine.trimmed();
                if (line.isEmpty()
                    || QRegularExpression(QStringLiteral(R"(^[-=]{8,}$)"))
                           .match(line)
                           .hasMatch())
                {
                    continue;
                }
                details.append(line.left(512));
            }

            constexpr qsizetype k_detail_lines = 8;
            if (details.size() > k_detail_lines)
                details = details.mid(details.size() - k_detail_lines);
            if (result.output_truncated)
                details.prepend(QStringLiteral("[earlier command output omitted]"));

            QString detail = details.join(QLatin1Char('\n'));
            constexpr qsizetype k_detail_characters = 1800;
            if (detail.size() > k_detail_characters)
            {
                detail = QStringLiteral("[earlier detail omitted]\n")
                    + detail.right(k_detail_characters);
            }
            if (!detail.isEmpty())
            {
                message += QStringLiteral("\n\nLast command output:\n")
                    + detail;
            }
            message += QStringLiteral(
                "\n\nSee launcher.log for the complete command output.");
            return message;
        }

#if defined(Q_OS_MACOS)
        bool patch_existing_alice_config(const QString& game_directory, QString& message)
        {
            const QString config_path = QDir(game_directory).filePath(QStringLiteral("alice.cfg"));
            QFile source(config_path);
            if (!source.exists())
            {
                message = QStringLiteral("alice.cfg does not exist yet. This launch will still use the safe-display profile; select Low Graphics again after the game creates the file.");
                return true;
            }
            if (!source.open(QIODevice::ReadOnly))
            {
                message = QStringLiteral("The existing alice.cfg could not be read.");
                return false;
            }

            const QByteArray original = source.readAll();
            source.close();
            QString text = QString::fromLatin1(original);
            bool changed = false;
            const QMap<QString, QString> replacements {
                {QStringLiteral("r_motionBlur"), QStringLiteral("0")},
                {QStringLiteral("r_mrt"), QStringLiteral("0")},
                {QStringLiteral("r_radialBlur"), QStringLiteral("0")},
                {QStringLiteral("cmd_force_simpleShadow"), QStringLiteral("1")}
            };
            for (auto it = replacements.cbegin(); it != replacements.cend(); ++it)
            {
                const QRegularExpression expression(
                    QStringLiteral(R"((?m)^(\s*seta\s+%1\s+")[^"]*("))")
                        .arg(QRegularExpression::escape(it.key())));
                const QRegularExpressionMatch match = expression.match(text);
                if (!match.hasMatch())
                    continue;
                const QString replacement = match.captured(1) + it.value() + match.captured(2);
                text.replace(match.capturedStart(), match.capturedLength(), replacement);
                changed = true;
            }

            if (!changed)
            {
                message = QStringLiteral("alice.cfg was found, but none of the verified compatibility keys were present.");
                return true;
            }

            const QString backup_path = config_path + QStringLiteral(".soa-macos-backup");
            if (!QFileInfo::exists(backup_path))
            {
                QSaveFile backup(backup_path);
                if (!backup.open(QIODevice::WriteOnly) || backup.write(original) != original.size()
                    || !backup.commit())
                {
                    message = QStringLiteral("The launcher could not create an alice.cfg backup.");
                    return false;
                }
            }

            const QByteArray updated = text.toLatin1();
            QSaveFile output(config_path);
            if (!output.open(QIODevice::WriteOnly) || output.write(updated) != updated.size()
                || !output.commit())
            {
                message = QStringLiteral("The launcher could not apply the conservative alice.cfg profile.");
                return false;
            }
            message = QStringLiteral("Applied the conservative macOS graphics profile and saved alice.cfg.soa-macos-backup.");
            return true;
        }

        bool macos_profile_uses_virtual_desktop(const QString& profile)
        {
            return profile == QStringLiteral("safe-display")
                || profile == QStringLiteral("low-graphics")
                || profile == QStringLiteral("gl-behind");
        }

        QString macos_wine_debug_value(const bool deepDiagnostics)
        {



            return deepDiagnostics
                ? QStringLiteral(
                      "+timestamp,+pid,+tid,+winediag,+seh,+loaddll,+module")
                : QStringLiteral("-all");
        }

        QString macos_opengl_surface_mode(const QString& profile)
        {
            return profile == QStringLiteral("gl-behind")
                ? QStringLiteral("behind") : QString();
        }

        bool macos_graphics_retina_enabled(const QString& profile)
        {
            Q_UNUSED(profile);
            return false;
        }

        QString macos_registry_diagnostic_summary(const QString& prefix)
        {
            QFile registry(QDir(prefix).filePath(QStringLiteral("user.reg")));
            if (!registry.open(QIODevice::ReadOnly | QIODevice::Text))
                return QStringLiteral("user.reg unavailable");

            QStringList selected;
            QString section;
            const QString text = QString::fromUtf8(registry.readAll());
            for (const QString& raw : text.split(QLatin1Char('\n')))
            {
                const QString line = raw.trimmed();
                if (line.startsWith(QLatin1Char('[')))
                    section = line;
                const bool relevant = line.contains(
                        QStringLiteral("\"renderer\""), Qt::CaseInsensitive)
                    || line.contains(QStringLiteral("\"OffscreenRenderingMode\""),
                                     Qt::CaseInsensitive)
                    || line.contains(QStringLiteral("\"VideoMemorySize\""),
                                     Qt::CaseInsensitive)
                    || line.contains(QStringLiteral("\"OpenGLSurfaceMode\""),
                                     Qt::CaseInsensitive)
                    || line.contains(QStringLiteral("\"RetinaMode\""),
                                     Qt::CaseInsensitive)
                    || line.contains(QStringLiteral("\"CaptureDisplaysForFullscreen\""),
                                     Qt::CaseInsensitive);
                if (!relevant)
                    continue;
                selected.append(section + QStringLiteral(" ") + line);
            }
            return selected.isEmpty()
                ? QStringLiteral("no explicit graphics overrides")
                : selected.join(QStringLiteral(" | "));
        }
#endif


    }

    Shell::Shell(QObject* parent) : StatusReporter(QStringLiteral("wine"), parent)
    {
        process = new QProcess(this);
        process->setProcessChannelMode(QProcess::MergedChannels);
        game_probe_process = new QProcess(this);
        game_probe_process->setProcessChannelMode(QProcess::MergedChannels);
        command_timer = new QTimer(this);
        command_timer->setSingleShot(true);
        force_kill_timer = new QTimer(this);
        force_kill_timer->setSingleShot(true);
        game_monitor_timer = new QTimer(this);
        game_monitor_timer->setSingleShot(true);
        game_probe_timeout_timer = new QTimer(this);
        game_probe_timeout_timer->setSingleShot(true);
#if defined(Q_OS_MACOS)
        game_diagnostic_file = new QFile(this);
        game_snapshot_generation = std::make_shared<std::atomic_uint64_t>(0);
#endif

        connect(process, &QProcess::started, this, [this]()
        {
            current.started = true;
            if (operation_kind == OperationKind::Game && has_pending_launch)
            {
#if defined(Q_OS_MACOS)
                append_macos_launch_timeline(
                    QStringLiteral("wine_wrapper_started"),
                    QStringLiteral("host_pid=%1").arg(process->processId()));
#endif
                working(QStringLiteral("game-launching"), -1.0, true);
                begin_game_verification();
            }
        });
        connect(process, &QProcess::readyReadStandardOutput, this, &Shell::handle_output);
        connect(process, &QProcess::errorOccurred, this, [this](const QProcess::ProcessError error)
        {
            if (error == QProcess::FailedToStart)
            {
                if (operation_kind == OperationKind::Game)
                {
#if defined(Q_OS_MACOS)
                    const QString message = QStringLiteral(
                        "The runtime could not start the game launch process: %1");
#else
                    const QString message = QStringLiteral(
                        "Wine could not start the game launch process: %1");
#endif
                    fail_game_launch(message
                                         .arg(process->errorString()));
                }
                else
                {
                    finish_process(CommandOutcome::FailedToStart, -1, QProcess::NormalExit,
                                   process->errorString());
                }
            }
            else
            {
                SPDLOG_WARN("process error: {}", process->errorString().toStdString());
            }
        });
        connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
                [this](const int exitCode, const QProcess::ExitStatus exitStatus)
        {
            if (operation_kind == OperationKind::Game)
            {
                handle_game_launcher_finished(exitCode, exitStatus);
                return;
            }
            if (operation_kind == OperationKind::None)
            {
                command_timer->stop();
                force_kill_timer->stop();
                force_kill_process_id = -1;
                handle_output();
                return;
            }

            CommandOutcome outcome = forced_outcome;
            if (outcome == CommandOutcome::Success)
            {
                if (exitStatus == QProcess::CrashExit)
                    outcome = CommandOutcome::Crashed;
                else if (exitCode != 0)
                    outcome = CommandOutcome::NonZeroExit;
            }
            finish_process(outcome, exitCode, exitStatus);
        });
        connect(command_timer, &QTimer::timeout, this, [this]()
        {
            if (operation_kind == OperationKind::None || terminal_emitted)
                return;
            forced_outcome = CommandOutcome::TimedOut;
            SPDLOG_ERROR("command timed out: {}", process->program().toStdString());
            force_kill_process_id = process->processId();
            process->terminate();
            force_kill_timer->start(3000);
        });
        connect(force_kill_timer, &QTimer::timeout, this, [this]()
        {
            const qint64 expectedPid = force_kill_process_id;
            force_kill_process_id = -1;
            if (expectedPid > 0 && process->state() != QProcess::NotRunning
                && process->processId() == expectedPid)
            {
                process->kill();
            }
        });

        connect(game_probe_process, &QProcess::readyReadStandardOutput,
                this, &Shell::handle_game_probe_output);
        connect(game_probe_process, &QProcess::errorOccurred, this,
                [this](const QProcess::ProcessError error)
        {
            if (error != QProcess::FailedToStart || !game_probe_completion)
                return;
            const auto completion = std::move(game_probe_completion);
            game_probe_completion = {};
            game_probe_timeout_timer->stop();
            const bool hostProbe = game_probe_host_mode;
            game_probe_output.clear();
            game_probe_host_mode = false;
            if (hostProbe)
            {
                SPDLOG_WARN("Alicia host process probe failed to start: {}",
                            game_probe_process->errorString().toStdString());
            }
            else
            {
                SPDLOG_DEBUG(
                    "Windows-side Alicia process probe failed to start; "
                    "the host probe will be used: {}",
                    game_probe_process->errorString().toStdString());
            }
            completion(false, std::nullopt);
        });
        connect(game_probe_process,
                QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
                [this](const int exitCode, const QProcess::ExitStatus exitStatus)
        {
            handle_game_probe_output();
            if (!game_probe_completion)
                return;
            const auto completion = std::move(game_probe_completion);
            game_probe_completion = {};
            game_probe_timeout_timer->stop();
            const bool hostProbe = game_probe_host_mode;
            const QString output = hostProbe
                ? QString::fromUtf8(game_probe_output)
                : decode_windows_process_output(game_probe_output);
            game_probe_output.clear();
            game_probe_host_mode = false;
            const bool queryOk = exitStatus == QProcess::NormalExit && exitCode == 0;
            const auto info = hostProbe
                ? find_host_process(output, QStringLiteral("Alicia.exe"))
                : find_windows_process(output, QStringLiteral("Alicia.exe"));
            if (!queryOk)
            {
                if (hostProbe)
                {
                    SPDLOG_WARN("Alicia host process probe failed (exit {}): {}",
                                exitCode, output.left(2048).toStdString());
                }
                else
                {
                    SPDLOG_DEBUG(
                        "Windows-side Alicia process probe failed (exit {}); "
                        "the host probe will be used: {}",
                        exitCode, output.left(2048).toStdString());
                }
            }
            completion(queryOk, info);
        });
        connect(game_monitor_timer, &QTimer::timeout, this, &Shell::poll_game_process);
        connect(game_probe_timeout_timer, &QTimer::timeout, this, [this]()
        {
            if (!game_probe_completion)
                return;
            const auto completion = std::move(game_probe_completion);
            game_probe_completion = {};
            const bool hostProbe = game_probe_host_mode;
            game_probe_output.clear();
            game_probe_host_mode = false;
            if (hostProbe)
            {
                SPDLOG_WARN("Alicia host process probe timed out");
            }
            else
            {
                SPDLOG_DEBUG(
                    "Windows-side Alicia process probe timed out; "
                    "the host probe will be used");
            }
            game_probe_process->kill();
            completion(false, std::nullopt);
        });
    }

    bool Shell::is_busy() const
    {
        return game_running_confirmed || game_lifecycle_uncertain
            || game_preflight_active || game_probe_completion
            || game_probe_process->state() != QProcess::NotRunning
            || operation_kind != OperationKind::None
            || process->state() != QProcess::NotRunning;
    }

    bool Shell::is_game_running() const
    {
        return game_running_confirmed || game_lifecycle_uncertain;
    }

    QString Shell::wine_binary() const
    {
        const QString configured = Config::instance().wine_binary();
        if (!configured.isEmpty()
            && WineRegistry::identify(configured) == RuntimeType::Proton)
        {
            return configured;
        }
        const QString resolved = WineRegistry::resolve_wine_executable(configured);
#if defined(Q_OS_MACOS)
        return resolved;
#else
        return resolved.isEmpty()
            ? (configured.isEmpty() ? QStringLiteral("wine") : configured)
            : resolved;
#endif
    }

    QString Shell::proton_root() const
    {
        const QFileInfo info(wine_binary());
        return info.isFile() ? info.dir().absolutePath() : info.absoluteFilePath();
    }

    QString Shell::proton_binary() const
    {
        const QFileInfo info(wine_binary());
        if (info.isFile() && info.fileName().compare(QStringLiteral("proton"), Qt::CaseInsensitive) == 0)
            return info.absoluteFilePath();
        return QDir(info.absoluteFilePath()).filePath(QStringLiteral("proton"));
    }

    QString Shell::proton_wine_binary() const
    {
        const QString root = proton_root();
        for (const QString& relative : {
                 QStringLiteral("files/bin/wine"),
                 QStringLiteral("dist/bin/wine"),
                 QStringLiteral("bin/wine")})
        {
            const QString candidate = QDir(root).filePath(relative);
            if (QFileInfo(candidate).isExecutable())
                return candidate;
        }
        return {};
    }

    QString Shell::proton_wineserver_binary() const
    {
        const QString wine = proton_wine_binary();
        return wine.isEmpty() ? QString() : QFileInfo(wine).dir().filePath(QStringLiteral("wineserver"));
    }

    QString Shell::wineserver_binary() const
    {
        if (runtime_is_proton())
        {
            const QString proton_server = proton_wineserver_binary();
            return QFileInfo(proton_server).isExecutable() ? proton_server : QString();
        }

        if (core::runtime::RuntimeManager::is_managed_selector(
                Config::instance().wine_binary()))
        {
            return core::runtime::RuntimeManager().resolve_active_entrypoint(
                QStringLiteral("wineserver"));
        }

        const QString wine = resolved_executable(wine_binary());
        if (!wine.isEmpty())
        {
            const QString sibling = QFileInfo(wine).dir().filePath(QStringLiteral("wineserver"));
            if (QFileInfo(sibling).isExecutable())
                return sibling;
        }
        return QStandardPaths::findExecutable(QStringLiteral("wineserver"));
    }

    QString Shell::wineboot_binary() const
    {
        if (core::runtime::RuntimeManager::is_managed_selector(
                Config::instance().wine_binary()))
        {
            const QString managed =
                core::runtime::RuntimeManager().resolve_active_entrypoint(
                    QStringLiteral("wineboot"));
            if (!managed.isEmpty())
                return managed;
        }

        const QString wine = resolved_executable(wine_binary());
        if (QFileInfo(wine).isAbsolute())
        {
            const QString sibling = QFileInfo(wine).dir().filePath(QStringLiteral("wineboot"));
            if (QFileInfo(sibling).isExecutable())
                return sibling;
        }



        return wine;
    }

    QString Shell::runtime_self_test_binary() const
    {
        if (core::runtime::RuntimeManager::is_managed_selector(
                Config::instance().wine_binary()))
        {
            return core::runtime::RuntimeManager().resolve_active_entrypoint(
                QStringLiteral("self_test"));
        }

#if defined(Q_OS_MACOS)
        const QString runtimeRoot =
            macos::runtime_root_for_executable(wine_binary());
        for (const QString& relative : {
                 QStringLiteral("Contents/Resources/tools/self-test-macos.sh"),
                 QStringLiteral("tools/self-test-macos.sh")})
        {
            const QString candidate = QDir(runtimeRoot).filePath(relative);
            if (QFileInfo(candidate).isFile() && QFileInfo(candidate).isExecutable())
                return candidate;
        }
#endif
        return {};
    }

    bool Shell::runtime_is_proton() const
    {
#if defined(Q_OS_MACOS)
        return false;
#else
        return WineRegistry::identify(wine_binary()) == RuntimeType::Proton;
#endif
    }

    QString Shell::steam_root() const
    {
        const QString home = QDir::homePath();
        for (const QString& root : {
                 home + QStringLiteral("/.local/share/Steam"),
                 home + QStringLiteral("/.steam/steam"),
                 home + QStringLiteral("/.steam/root"),
                 home + QStringLiteral("/.var/app/com.valvesoftware.Steam/data/Steam")})
        {
            if (QFileInfo::exists(root))
                return root;
        }
        return home + QStringLiteral("/.steam");
    }

    QProcessEnvironment Shell::proton_env() const
    {
        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        environment.insert(QStringLiteral("STEAM_COMPAT_DATA_PATH"),
                           Config::instance().proton_compat_data_root());
        environment.insert(QStringLiteral("STEAM_COMPAT_CLIENT_INSTALL_PATH"), steam_root());
        environment.insert(QStringLiteral("WINEDLLOVERRIDES"), QStringLiteral("winegstreamer="));
        if (!Config::instance().use_dxvk())
            environment.insert(QStringLiteral("PROTON_USE_WINED3D"), QStringLiteral("1"));
        apply_wine_environment(environment);
        return environment;
    }

    QProcessEnvironment Shell::winetricks_environment() const
    {
        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        environment.insert(QStringLiteral("WINEPREFIX"), Config::instance().prefix_root());
        if (runtime_is_proton())
        {
            const QString wine = proton_wine_binary();
            const QString wineserver = proton_wineserver_binary();
            if (!wine.isEmpty()) environment.insert(QStringLiteral("WINE"), wine);
            if (!wineserver.isEmpty()) environment.insert(QStringLiteral("WINESERVER"), wineserver);
        }
        else
        {
            const QString wine = resolved_executable(wine_binary());
            const QString wineserver = wineserver_binary();
            environment.insert(QStringLiteral("WINE"), wine);
            if (!wineserver.isEmpty())
                environment.insert(QStringLiteral("WINESERVER"), wineserver);
            environment.insert(QStringLiteral("WINEARCH"), Config::instance().wine_arch());
        }
        apply_wine_environment(environment);
        return environment;
    }

    void Shell::apply_wine_environment(QProcessEnvironment& environment) const
    {
        static const QRegularExpression keyPattern(QStringLiteral(R"(^[A-Za-z_][A-Za-z0-9_]*$)"));
        const QStringList tokens = QProcess::splitCommand(Config::instance().wine_args());
        for (const QString& token : tokens)
        {
            const int equals = token.indexOf(QLatin1Char('='));
            if (equals <= 0)
            {
                SPDLOG_WARN("ignoring Wine environment entry (expected KEY=VALUE): {}",
                            token.toStdString());
                continue;
            }
            const QString key = token.left(equals);
            const QString value = token.mid(equals + 1);
            if (!keyPattern.match(key).hasMatch())
            {
                SPDLOG_WARN("ignoring invalid Wine environment key: {}", key.toStdString());
                continue;
            }
            const QString upper = key.toUpper();
            const bool protectedKey = upper == QStringLiteral("PATH")
                || upper == QStringLiteral("HOME")
                || upper == QStringLiteral("WINE")
                || upper == QStringLiteral("WINESERVER")
                || upper == QStringLiteral("WINEPREFIX")
                || upper == QStringLiteral("WINEARCH")
                || upper == QStringLiteral("WINEDEBUG")
                || upper == QStringLiteral("WINEDLLOVERRIDES")
                || upper == QStringLiteral("LD_PRELOAD")
                || upper == QStringLiteral("GAMEID")
                || upper == QStringLiteral("STORE")
                || upper.startsWith(QStringLiteral("DYLD_"))
                || upper.startsWith(QStringLiteral("CX_"))
                || upper.startsWith(QStringLiteral("PROTON_"))
                || upper.startsWith(QStringLiteral("STEAM_COMPAT_"));
            if (protectedKey)
            {
                SPDLOG_WARN("ignoring launcher-owned Wine environment key: {}", key.toStdString());
                continue;
            }
            environment.insert(key, value);
            const bool sensitive = key.contains(QStringLiteral("TOKEN"), Qt::CaseInsensitive)
                || key.contains(QStringLiteral("PASSWORD"), Qt::CaseInsensitive)
                || key.contains(QStringLiteral("SECRET"), Qt::CaseInsensitive);
            SPDLOG_DEBUG("wine env: {}={}", key.toStdString(),
                         sensitive ? "[REDACTED]" : value.toStdString());
        }
    }

    QString Shell::resolved_executable(const QString& program) const
    {
        const QFileInfo info(program);
        if (info.isAbsolute())
            return info.absoluteFilePath();
        return QStandardPaths::findExecutable(program);
    }

    bool Shell::is_wine_installed() const
    {
        const QString program = runtime_is_proton() ? proton_binary() : resolved_executable(wine_binary());
        const QFileInfo info(program);
        bool valid = info.isFile() && info.isExecutable();
#if defined(Q_OS_MACOS)
        const QString configured = Config::instance().wine_binary();
        if (valid && core::runtime::RuntimeManager::is_managed_selector(configured))
        {
            const auto package = core::runtime::RuntimeManager().active();
            valid = package.usable
                && package.manifest.graphics_backends.contains(
                    QStringLiteral("wined3d-opengl"), Qt::CaseInsensitive)
                && QFileInfo(package.wine_executable).canonicalFilePath()
                    == info.canonicalFilePath();
            if (!valid)
            {
                SPDLOG_ERROR("managed runtime validation failed: {}",
                             package.failure.toStdString());
            }
        }
        else if (valid && QFileInfo(QDir(configured).filePath(
                QStringLiteral("runtime.json"))).isFile())
        {
            const auto package =
                core::runtime::RuntimeManager::inspect_package(configured);
            valid = package.usable
                && package.manifest.graphics_backends.contains(
                    QStringLiteral("wined3d-opengl"), Qt::CaseInsensitive)
                && QFileInfo(package.wine_executable).canonicalFilePath()
                    == info.canonicalFilePath();
            if (!valid)
            {
                SPDLOG_ERROR("runtime package validation failed: {}",
                             package.failure.toStdString());
            }
        }
        if (valid && macos::executable_requires_rosetta(program)
            && !macos::rosetta_is_available())
        {
            valid = false;
        }
#endif
        SPDLOG_DEBUG("runtime {}: {}", program.toStdString(), valid ? "usable" : "not executable");
        return valid;
    }

    bool Shell::prepare_host_invocation(const QString& program,
                                        const QStringList& arguments,
                                        const QProcessEnvironment& environment,
                                        QString& executable,
                                        QStringList& launch_arguments,
                                        QProcessEnvironment& launch_environment) const
    {
        executable = resolved_executable(program);
        if (executable.isEmpty() || !QFileInfo(executable).isFile()
            || !QFileInfo(executable).isExecutable())
        {
            SPDLOG_ERROR("executable not found or not runnable: {}", program.toStdString());
            return false;
        }

        const QString runtime_executable = executable;
        launch_arguments = arguments;
        launch_environment = environment;
#if defined(Q_OS_MACOS)
        if (macos::executable_requires_rosetta(runtime_executable)
            && !macos::rosetta_is_available())
        {
            SPDLOG_ERROR("Rosetta is required to launch {}", runtime_executable.toStdString());
            return false;
        }
        macos::apply_runtime_environment(launch_environment, runtime_executable);
        executable = macos::prepare_host_launch(runtime_executable, launch_arguments);
        if (executable.isEmpty())
            return false;
#endif
        return true;
    }

    bool Shell::start_process(const QString& program, const QStringList& arguments,
                              const QProcessEnvironment& environment, const int timeoutMs,
                              const OperationKind kind,
                              std::function<void(const command_result&)> completion)
    {
        if (is_busy())
        {
            SPDLOG_WARN("attempted to start {} while another process is active", program.toStdString());
            return false;
        }

        QString executable;
        QStringList launch_arguments;
        QProcessEnvironment launch_environment;
        if (!prepare_host_invocation(program, arguments, environment,
                                     executable, launch_arguments, launch_environment))
        {
            return false;
        }

#if defined(Q_OS_MACOS)



        if (kind == OperationKind::Game
            && QFileInfo(QStringLiteral("/usr/bin/caffeinate")).isExecutable())
        {
            launch_arguments.prepend(executable);
            launch_arguments.prepend(QStringLiteral("-dimsu"));
            executable = QStringLiteral("/usr/bin/caffeinate");
            append_macos_launch_timeline(
                QStringLiteral("app_nap_guard_enabled"),
                QStringLiteral("program=/usr/bin/caffeinate flags=-dimsu"));
        }
#endif

        force_kill_timer->stop();
        force_kill_process_id = -1;
        current = command_result{};
        terminal_emitted = false;
        forced_outcome = CommandOutcome::Success;
        operation_kind = kind;
        completion_handler = std::move(completion);
        if (kind == OperationKind::Game)
        {
            game_launcher_finished = false;
            game_launcher_exit_code = -1;
            game_launcher_crashed = false;
        }
        process->setProcessEnvironment(launch_environment);
        if (kind != OperationKind::Game)
            process->setWorkingDirectory(QDir::homePath());

        const QStringList logged = redacted_command_args(launch_arguments);
        SPDLOG_INFO("running: {} {}", executable.toStdString(), logged.join(QLatin1Char(' ')).toStdString());
        process->start(executable, launch_arguments);
        if (timeoutMs > 0)
            command_timer->start(timeoutMs);
        return true;
    }

    void Shell::run_command(const QString& program, const QStringList& arguments,
                            const QProcessEnvironment& environment)
    {
        if (!start_process(program, arguments, environment, 15 * 60 * 1000,
                           OperationKind::Generic))
        {
            fail_user(QStringLiteral("Could Not Start Command"),
                      QStringLiteral("The selected executable could not be started."));
        }
    }

    void Shell::cancel_current()
    {
        if (!is_busy())
            return;
        if (game_running_confirmed)
        {
            emit user_notice(QStringLiteral(
                "Alicia.exe is running. Exit the game normally instead of cancelling its monitor."));
            return;
        }
        if (game_preflight_active || has_pending_launch || operation_kind == OperationKind::Game)
        {
            fail_game_launch(QStringLiteral("The game launch was cancelled."));
            return;
        }
        forced_outcome = CommandOutcome::Cancelled;
        force_kill_process_id = process->processId();
        process->terminate();
        force_kill_timer->start(3000);
    }

    void Shell::handle_output()
    {
        const QByteArray bytes = process->readAllStandardOutput();
        QString decoded = QString::fromUtf8(bytes);
        if (decoded.contains(QChar::ReplacementCharacter))
            decoded = QString::fromLocal8Bit(bytes);
        QString chunk = redact_sensitive_text(decoded);
        if (chunk.isEmpty())
            return;
#if defined(Q_OS_MACOS)
        if (operation_kind == OperationKind::Game)
        {
            if (!game_timeline_seen_create_device
                && chunk.contains(QStringLiteral("CreateDevice"), Qt::CaseInsensitive))
            {
                game_timeline_seen_create_device = true;
                append_macos_launch_timeline(QStringLiteral("d3d9_create_device_observed"));
            }
            if (!game_timeline_seen_present
                && QRegularExpression(QStringLiteral(R"(\bPresent\s*\()"),
                                      QRegularExpression::CaseInsensitiveOption)
                       .match(chunk).hasMatch())
            {
                game_timeline_seen_present = true;
                append_macos_launch_timeline(QStringLiteral("d3d9_present_observed"));
            }
            if (!game_timeline_seen_draw
                && QRegularExpression(QStringLiteral(R"(\bDraw(?:Indexed)?Primitive(?:UP)?\s*\()"),
                                      QRegularExpression::CaseInsensitiveOption)
                       .match(chunk).hasMatch())
            {
                game_timeline_seen_draw = true;
                append_macos_launch_timeline(QStringLiteral("d3d9_draw_observed"));
            }
            if (!game_timeline_seen_exception
                && (chunk.contains(QStringLiteral("Unhandled exception"), Qt::CaseInsensitive)
                    || chunk.contains(QStringLiteral("Exception frame is not in stack limits"),
                                      Qt::CaseInsensitive)))
            {
                game_timeline_seen_exception = true;
                append_macos_launch_timeline(
                    QStringLiteral("fatal_exception_observed"),
                    chunk.left(512).simplified());
            }
        }
#endif
#if defined(Q_OS_MACOS)
        if (operation_kind == OperationKind::Game && game_diagnostic_file
            && game_diagnostic_file->isOpen())
        {
            game_diagnostic_file->write(chunk.toUtf8());
            game_diagnostic_file->flush();
        }
#endif

        if (current.output.size() + chunk.size() > k_max_captured_output)
        {
            const qsizetype keep = qMax<qsizetype>(0, k_max_captured_output - chunk.size());
            if (keep > 0)
                current.output = current.output.right(keep);
            else
                current.output.clear();
            if (chunk.size() > k_max_captured_output)
                chunk = chunk.right(k_max_captured_output);
            current.output_truncated = true;
        }
        current.output += chunk;

        int logged = 0;
        for (const QString& line : chunk.split(QLatin1Char('\n'), Qt::SkipEmptyParts))
        {
            if (logged++ >= 100)
            {
                SPDLOG_DEBUG("[cmd] additional output omitted from this batch");
                break;
            }
            SPDLOG_DEBUG("[cmd] {}", line.left(4096).toStdString());
        }
    }

    void Shell::finish_process(const CommandOutcome outcome, const int exitCode,
                               const QProcess::ExitStatus exitStatus,
                               const QString& errorMessage)
    {
        if (terminal_emitted || operation_kind == OperationKind::None)
            return;
        terminal_emitted = true;
        command_timer->stop();
        force_kill_timer->stop();
        force_kill_process_id = -1;
        handle_output();

        const OperationKind finishedKind = operation_kind;
        const auto completion = std::move(completion_handler);
        current.outcome = outcome;
        current.exit_code = exitCode;
        current.crashed = exitStatus == QProcess::CrashExit || outcome == CommandOutcome::Crashed;
        current.error_message = errorMessage;
        operation_kind = OperationKind::None;
        completion_handler = {};

        SPDLOG_INFO("command completed: {} (exit {})", outcome_name(outcome).toStdString(), exitCode);
        emit command_finished(current);

        if (finishedKind == OperationKind::Game && has_pending_launch)
        {
            const auto version = pending_launch.version;
            emit game_exited(version, exitCode, current.crashed);
            if (current.ok())
                done(QStringLiteral("Game exited."));
            else
                fail(QStringLiteral("The game exited unexpectedly."));
            has_pending_launch = false;
        }

        if (completion)
            completion(current);
    }

    void Shell::fail_user(const QString& title, const QString& message)
    {
        SPDLOG_ERROR("{}: {}", title.toStdString(), message.toStdString());
        const QString translated_title = util::i18n::translate(title);
        const QString translated_message = util::i18n::translate(message);
        fail(translated_message);
        emit user_error(translated_title, translated_message);
    }

    QStringList Shell::missing_component_packages() const
    {


        return PrefixInspector::missing_packages(
            Config::instance().prefix_root(), runtime_is_proton(), false);
    }

    bool Shell::queue_missing_components()
    {
        const QStringList packages = missing_component_packages();
#if defined(Q_OS_MACOS)
        const bool installOptionalDxvk = false;
#else
        const bool installOptionalDxvk =
            Config::instance().use_dxvk()
            && !runtime_is_proton()
            && !PrefixInspector::dxvk_installed(
                Config::instance().prefix_root());
#endif
        if (packages.isEmpty() && !installOptionalDxvk)
            return true;

        const QString winetricks = winetricks_path();
        if (winetricks.isEmpty() || !QFileInfo(winetricks).isExecutable())
        {
            if (!packages.isEmpty())
            {
                SPDLOG_ERROR(
                    "missing components require winetricks: {}",
                    packages.join(QStringLiteral(", ")).toStdString());
                return false;
            }

            Config::instance().set_use_dxvk(false);
            SPDLOG_WARN(
                "optional DXVK setup skipped because winetricks is unavailable");
            emit user_notice(QStringLiteral(
                "Optional DXVK setup was skipped because Winetricks is unavailable. "
                "DXVK has been turned off and prefix setup will continue."));
            return true;
        }
        if (runtime_is_proton() && proton_wine_binary().isEmpty())
        {
            SPDLOG_ERROR("selected Proton runtime does not expose a Wine binary for winetricks");
            return false;
        }

        int insertionIndex = setup_index + 1;
        if (!packages.isEmpty())
        {
            QStringList arguments {QStringLiteral("-q")};
            arguments.append(packages);
            setup_queue.insert(
                insertionIndex++,
                SetupCommand{k_installing_message, winetricks, arguments,
                             winetricks_environment(), 30 * 60 * 1000,
                             false, false, false});
        }
        if (installOptionalDxvk)
        {
            setup_queue.insert(
                insertionIndex,
                SetupCommand{
                    QStringLiteral("Installing optional DXVK..."),
                    winetricks,
                    {QStringLiteral("-q"),
                     PrefixInspector::dxvk_winetricks_verb()},
                    winetricks_environment(),
                    30 * 60 * 1000,
                    false,
                    false,
                    true
                });
        }
        return true;
    }

    void Shell::finish_setup_failure(const QString& message)
    {
        if (setup_kind == OperationKind::Dxvk)
        {
            finish_optional_dxvk_failure(message);
            return;
        }

        setup_index = -1;
        setup_queue.clear();
        setup_kind = OperationKind::None;
        marker_invalidated = false;
        emit setup_status(message);
#if defined(Q_OS_MACOS)
        fail_user(QStringLiteral("Runtime Setup Failed"), message);
#else
        fail_user(QStringLiteral("Wine Setup Failed"), message);
#endif
        emit wine_setup_finished(false);
    }

    void Shell::finish_optional_dxvk_failure(const QString& message)
    {
        setup_index = -1;
        setup_queue.clear();
        setup_kind = OperationKind::None;
        marker_invalidated = false;
        Config::instance().set_use_dxvk(false);

        const QString userMessage = message + QStringLiteral(
            "\n\nDXVK has been turned off. The existing prefix remains usable "
            "with the built-in Direct3D backend.");
        SPDLOG_ERROR("optional DXVK setup failed: {}",
                     message.toStdString());
        emit setup_status(QStringLiteral(
            "DXVK could not be enabled; the prefix was left unchanged."));
        emit user_error(QStringLiteral("DXVK Setup Failed"), userMessage);
        done(QStringLiteral(
            "DXVK is off; the existing prefix remains ready."));
        emit wine_setup_finished(true);
    }

    void Shell::skip_optional_setup_command(const QString& message)
    {
        Config::instance().set_use_dxvk(false);
        SPDLOG_WARN("optional DXVK setup skipped: {}",
                    message.toStdString());
        emit setup_status(QStringLiteral(
            "Optional DXVK setup failed; continuing prefix setup..."));
        emit user_notice(QStringLiteral(
            "Optional DXVK setup failed and DXVK was turned off. "
            "Required prefix setup will continue; details are in launcher.log."));
        ++setup_index;
        advance_setup();
    }

    void Shell::run_setup(QVector<SetupCommand> commands, const OperationKind kind)
    {
        if (is_busy())
        {
#if defined(Q_OS_MACOS)
            const QString busyMessage = QStringLiteral(
                "Another runtime or game process is already running.");
#else
            const QString busyMessage = QStringLiteral(
                "Another Wine or game process is already running.");
#endif
            fail_user(QStringLiteral("Launcher Busy"), busyMessage);
            emit wine_setup_finished(false);
            return;
        }
        setup_queue = std::move(commands);
        setup_index = 0;
        setup_kind = kind;
        marker_invalidated = false;
        advance_setup();
    }

    void Shell::advance_setup()
    {
        if (setup_index < 0)
            return;
        if (setup_index >= setup_queue.size())
        {
            setup_index = -1;
            setup_queue.clear();
            const OperationKind completed_kind = setup_kind;
            setup_kind = OperationKind::None;
            finalize_setup(completed_kind, k_prefix_validation_retries);
            return;
        }

        const SetupCommand command = setup_queue[setup_index];
        if (command.invalidates_marker && !marker_invalidated)
        {
            if (!PrefixInspector::remove_marker(Config::instance().prefix_root()))
            {
                finish_setup_failure(QStringLiteral("Could not invalidate the old prefix setup marker."));
                return;
            }
            marker_invalidated = true;
        }

        emit setup_status(command.message);
        working(QStringLiteral("prefix-setup"));
        if (!start_process(command.program, command.args, command.env, command.timeout_ms,
                           setup_kind,
                           [this, command](const command_result& result)
        {
            if (!result.ok())
            {
                const QString message =
                    command_failure_message(command.message, result);
                if (command.optional_failure)
                {
                    skip_optional_setup_command(message);
                    return;
                }
                finish_setup_failure(message);
                return;
            }
            if (command.inspect_components_after && !queue_missing_components())
            {
                finish_setup_failure(QStringLiteral(
                    "Required components are missing, but Winetricks could not install them."));
                return;
            }
            ++setup_index;
            advance_setup();
        }))
        {
            if (command.optional_failure)
            {
                skip_optional_setup_command(
                    QStringLiteral(
                        "%1 could not be started. See launcher.log for details.")
                        .arg(command.message));
                return;
            }
            finish_setup_failure(QStringLiteral("Could not start %1").arg(command.message));
        }
    }


    void Shell::finalize_setup(const OperationKind completed_kind, const int attempts_remaining)
    {
        const QString runtimeIdentity = runtime_is_proton()
            ? Config::instance().wine_binary()
            : wine_binary();
        const auto inspection = PrefixInspector::inspect(
            Config::instance().prefix_root(), runtimeIdentity, runtime_is_proton());
        const bool dxvkRequested = Config::instance().use_dxvk()
            && !runtime_is_proton();
        if (dxvkRequested && !inspection.dxvk_installed)
        {
            if (attempts_remaining > 0)
            {
                SPDLOG_DEBUG(
                    "DXVK validation pending: files_present={}, "
                    "overrides_present={}, retries_remaining={}",
                    inspection.dxvk_files_present,
                    inspection.dxvk_overrides_present,
                    attempts_remaining);
                QTimer::singleShot(k_prefix_validation_retry_ms, this,
                                   [this, completed_kind, attempts_remaining]()
                {
                    finalize_setup(completed_kind, attempts_remaining - 1);
                });
                return;
            }

            SPDLOG_ERROR(
                "DXVK validation failed: structure_valid={}, "
                "files_present={}, overrides_present={}",
                inspection.structure_valid,
                inspection.dxvk_files_present,
                inspection.dxvk_overrides_present);
            QString reason;
            if (!inspection.dxvk_files_present)
            {
                reason = QStringLiteral(
                    "The 32-bit d3d9/dxgi files are missing.");
            }
            else
            {
                reason = QStringLiteral(
                    "The DLL files are present, but their native overrides did "
                    "not become visible in the persisted prefix registry.");
            }

            if (completed_kind == OperationKind::Dxvk)
            {
                finish_optional_dxvk_failure(QStringLiteral(
                    "Winetricks finished, but DXVK could not be verified. %1 "
                    "Check launcher.log for the installer output.")
                    .arg(reason));
                return;
            }

            Config::instance().set_use_dxvk(false);
            SPDLOG_WARN(
                "optional DXVK command completed without a verifiable "
                "installation; DXVK was turned off");
            emit user_notice(QStringLiteral(
                "DXVK could not be verified and was turned off. %1 "
                "The prefix remains ready with the built-in Direct3D backend.")
                .arg(reason));
        }
        if (!inspection.required_components_present(runtime_is_proton()))
        {
            if (attempts_remaining > 0)
            {
                QTimer::singleShot(k_prefix_validation_retry_ms, this,
                                   [this, completed_kind, attempts_remaining]()
                {
                    finalize_setup(completed_kind, attempts_remaining - 1);
                });
                return;
            }

            const char* architecture = "unknown";
            if (inspection.architecture == PrefixArchitecture::Win32)
                architecture = "win32";
            else if (inspection.architecture == PrefixArchitecture::Win64)
                architecture = "win64";
            SPDLOG_ERROR(
                "prefix validation failed: exists={}, structure_valid={}, architecture={}, d3dx9_31={}, d3dx9_42={}, d3dcompiler_42={}, d3dcompiler_47={}, msvc2010_runtime={}, physx_runtime={}, d3dx9_43={}, msvc_runtime={}, dxvk_files_present={}, dxvk_overrides_present={}, dxvk_installed={}",
                inspection.exists,
                inspection.structure_valid,
                architecture,
                inspection.d3dx9_31,
                inspection.d3dx9_42,
                inspection.d3dcompiler_42,
                inspection.d3dcompiler_47,
                inspection.msvc2010_runtime,
                inspection.physx_runtime,
                inspection.d3dx9_43,
                inspection.msvc_runtime,
                inspection.dxvk_files_present,
                inspection.dxvk_overrides_present,
                inspection.dxvk_installed);
#if defined(Q_OS_MACOS)
            if (!inspection.exists || !inspection.structure_valid
                || inspection.architecture == PrefixArchitecture::Win32)
            {
                finish_setup_failure(QStringLiteral(
                    "Prefix setup finished, but the runtime prefix structure is incomplete or incompatible."));
            }
            else
            {
                finish_setup_failure(QStringLiteral(
                    "Prefix setup finished, but Alicia's required DirectX or VC++ components are still missing."));
            }
#else
            if (completed_kind == OperationKind::Dxvk)
            {
                finish_optional_dxvk_failure(QStringLiteral(
                    "DXVK setup finished, but the prefix is not ready. "
                    "Run prefix setup before enabling DXVK."));
            }
            else
            {
                finish_setup_failure(QStringLiteral(
                    "Prefix setup finished, but required components are still missing."));
            }
#endif
            return;
        }

        if (!PrefixInspector::write_marker(
                Config::instance().prefix_root(), runtimeIdentity))
        {
            const QString message = QStringLiteral(
                "Setup completed, but the prefix could not be marked ready.");
            if (completed_kind == OperationKind::Dxvk)
                finish_optional_dxvk_failure(message);
            else
                finish_setup_failure(message);
            return;
        }

        emit setup_status(QStringLiteral("Done!"));
        done(completed_kind == OperationKind::Dxvk
            ? QStringLiteral("DXVK setup complete.")
            : QStringLiteral("Prefix setup complete."));
        emit wine_setup_finished(true);
    }

    void Shell::setup()
    {
        if (runtime_is_proton())
            setup_proton();
        else
            setup_wine();
    }

    void Shell::setup_wine()
    {
#if defined(Q_OS_MACOS)
        core::runtime::RuntimeInstallation managedRuntime;
        if (core::runtime::RuntimeManager::is_managed_selector(
                Config::instance().wine_binary()))
        {
            managedRuntime = core::runtime::RuntimeManager().active();
            if (!managedRuntime.usable)
            {
                fail_user(QStringLiteral("Runtime Not Ready"),
                          managedRuntime.failure.isEmpty()
                              ? QStringLiteral("The managed runtime is incomplete.")
                              : managedRuntime.failure);
                emit wine_setup_finished(false);
                return;
            }
            if (managedRuntime.manifest.platform.compare(
                    QStringLiteral("macos"), Qt::CaseInsensitive) != 0
                || !managedRuntime.manifest.graphics_backends.contains(
                    QStringLiteral("wined3d-opengl"), Qt::CaseInsensitive))
            {
                fail_user(QStringLiteral("Incompatible Runtime"),
                          QStringLiteral(
                              "This runtime does not declare the supported macOS graphics backend."));
                emit wine_setup_finished(false);
                return;
            }
            SPDLOG_INFO("prefix setup using managed runtime: identity={} contract={} prefix_schema={} wine={}",
                        managedRuntime.manifest.identity().toStdString(),
                        managedRuntime.manifest.launcher_contract,
                        managedRuntime.manifest.prefix_schema,
                        managedRuntime.wine_executable.toStdString());
        }
        const QString configuredRuntime = Config::instance().wine_binary();
        if (QFileInfo(QDir(configuredRuntime).filePath(
                QStringLiteral("runtime.json"))).isFile())
        {
            const auto package =
                core::runtime::RuntimeManager::inspect_package(configuredRuntime);
            if (!package.usable
                || !package.manifest.graphics_backends.contains(
                    QStringLiteral("wined3d-opengl"),
                    Qt::CaseInsensitive))
            {
                fail_user(
                    QStringLiteral("Invalid Runtime Package"),
                    package.failure.isEmpty()
                        ? QStringLiteral(
                            "The runtime package does not declare the supported macOS graphics backend.")
                        : package.failure);
                emit wine_setup_finished(false);
                return;
            }
            SPDLOG_INFO(
                "prefix setup using packaged runtime: identity={} contract={} prefix_schema={} executable={}",
                package.manifest.identity().toStdString(),
                package.manifest.launcher_contract,
                package.manifest.prefix_schema,
                package.wine_executable.toStdString());
        }
        const QString runtimeExecutable = macos::resolve_wine_executable(
            configuredRuntime);
        if (runtimeExecutable.isEmpty() || !QFileInfo(runtimeExecutable).isExecutable())
        {
            fail_user(QStringLiteral("Invalid macOS Runtime"),
                      QStringLiteral("No executable runtime entry point was found in the selected app or folder."));
            emit wine_setup_finished(false);
            return;
        }
        if (macos::executable_requires_rosetta(runtimeExecutable)
            && !macos::rosetta_is_available())
        {
            fail_user(QStringLiteral("Rosetta Required"),
                      QStringLiteral("The selected runtime is Intel-only. Request Rosetta in the runtime chooser, complete the macOS prompt, then retry."));
            emit wine_setup_finished(false);
            return;
        }
#else
        if (!is_wine_installed())
        {
            fail_user(QStringLiteral("Wine Not Available"),
                      QStringLiteral("The selected Wine runtime is missing or not executable."));
            emit wine_setup_finished(false);
            return;
        }
#endif
        const QString prefix = Config::instance().prefix_root();
        if (prefix.isEmpty() || !QDir().mkpath(prefix))
        {
#if defined(Q_OS_MACOS)
            const QString prefixMessage = QStringLiteral(
                "The runtime prefix directory could not be created.");
#else
            const QString prefixMessage = QStringLiteral(
                "The Wine prefix directory could not be created.");
#endif
            fail_user(QStringLiteral("Invalid Prefix"), prefixMessage);
            emit wine_setup_finished(false);
            return;
        }

        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        environment.insert(QStringLiteral("WINEPREFIX"), prefix);
#if defined(Q_OS_MACOS)


        environment.insert(QStringLiteral("WINEARCH"), QStringLiteral("win64"));
#else
        environment.insert(QStringLiteral("WINEARCH"), Config::instance().wine_arch());
#endif
        environment.insert(QStringLiteral("WINE"), resolved_executable(wine_binary()));
        apply_wine_environment(environment);

        QVector<SetupCommand> commands;
#if defined(Q_OS_MACOS)
        const QString selfTest = runtime_self_test_binary();
        if (!selfTest.isEmpty())
        {
            commands.push_back({
                QStringLiteral("Running the runtime integrity self-test..."),
                selfTest,
                {macos::runtime_root_for_executable(runtimeExecutable)},
                QProcessEnvironment::systemEnvironment(),
                5 * 60 * 1000, false, false
            });
        }
        const QString validateRuntimeMessage =
            QStringLiteral("Validating the macOS runtime...");
        const QString createPrefixMessage =
            QStringLiteral("Creating the runtime prefix...");
#else
        const QString validateRuntimeMessage =
            QStringLiteral("Validating Wine runtime...");
        const QString createPrefixMessage =
            QStringLiteral("Creating Wine prefix...");
#endif
        commands.push_back({validateRuntimeMessage, wine_binary(),
                            {QStringLiteral("--version")}, environment,
                            k_runtime_probe_timeout_ms, false, false});
        const QString wineboot = wineboot_binary();
        QStringList winebootArguments;
        if (QFileInfo(wineboot).canonicalFilePath()
            == QFileInfo(resolved_executable(wine_binary())).canonicalFilePath())
        {
            winebootArguments << QStringLiteral("wineboot");
        }
        winebootArguments << QStringLiteral("--init");
        commands.push_back({createPrefixMessage, wineboot,
                            winebootArguments, environment,
                            5 * 60 * 1000, true, true});
#if defined(Q_OS_MACOS)


        const QString d3dKey = QStringLiteral("HKCU\\Software\\Wine\\Direct3D");
        commands.push_back({QStringLiteral("Clearing stale graphics overrides..."), wine_binary(),
                            {QStringLiteral("cmd.exe"), QStringLiteral("/d"),
                             QStringLiteral("/s"), QStringLiteral("/c"),
                             QStringLiteral(
                                 "reg.exe delete \"%1\" /v renderer /f >nul 2>&1 & exit /b 0")
                                 .arg(d3dKey)},
                            environment, k_registry_timeout_ms, false, true});
        commands.push_back({QStringLiteral("Configuring the built-in graphics framebuffer..."), wine_binary(),
                            {QStringLiteral("reg.exe"), QStringLiteral("add"), d3dKey,
                             QStringLiteral("/v"), QStringLiteral("OffscreenRenderingMode"),
                             QStringLiteral("/t"), QStringLiteral("REG_SZ"),
                             QStringLiteral("/d"), QStringLiteral("fbo"), QStringLiteral("/f")},
                            environment, k_registry_timeout_ms, false, true});
        commands.push_back({QStringLiteral("Configuring graphics memory..."), wine_binary(),
                            {QStringLiteral("reg.exe"), QStringLiteral("add"), d3dKey,
                             QStringLiteral("/v"), QStringLiteral("VideoMemorySize"),
                             QStringLiteral("/t"), QStringLiteral("REG_SZ"),
                             QStringLiteral("/d"), QStringLiteral("4096"), QStringLiteral("/f")},
                            environment, k_registry_timeout_ms, false, true});
#endif
        run_setup(std::move(commands), OperationKind::Setup);
    }

    void Shell::setup_proton()
    {
        if (!is_wine_installed())
        {
            fail_user(QStringLiteral("Proton Not Available"),
                      QStringLiteral("The selected Proton runtime is missing or not executable."));
            emit wine_setup_finished(false);
            return;
        }
        const QString compat = Config::instance().proton_compat_data_root();
        if (compat.isEmpty() || !QDir().mkpath(compat))
        {
            fail_user(QStringLiteral("Invalid Compatibility Data Path"),
                      QStringLiteral("The Proton compatibility-data directory could not be created."));
            emit wine_setup_finished(false);
            return;
        }

        const QString protonWine = proton_wine_binary();
        if (protonWine.isEmpty())
        {
            fail_user(QStringLiteral("Invalid Proton Runtime"),
                      QStringLiteral("The selected Proton installation does not contain a runnable Wine binary."));
            emit wine_setup_finished(false);
            return;
        }

        QVector<SetupCommand> commands;





        commands.push_back({QStringLiteral("Creating Proton prefix..."), proton_binary(),
                            {QStringLiteral("runinprefix"), QStringLiteral("wineboot"),
                             QStringLiteral("--init")},
                            proton_env(), 5 * 60 * 1000, true, true});
        run_setup(std::move(commands), OperationKind::Setup);
    }

    void Shell::sync_dxvk()
    {
#if defined(Q_OS_MACOS)
        Config::instance().set_use_dxvk(false);
        emit user_notice(QStringLiteral(
            "macOS uses the runtime's built-in Direct3D 9 backend in this version. "
            "DXVK is intentionally unavailable until a tested Metal/Vulkan path exists."));
        return;
#endif
        if (!Config::instance().use_dxvk())
        {
            emit user_notice(QStringLiteral("DXVK is disabled; WineD3D will be used."));
            return;
        }
        if (runtime_is_proton())
        {
            emit user_notice(QStringLiteral(
                "DXVK is enabled. Proton provides it internally, so no separate installation is needed."));
            return;
        }

        const QString prefix = Config::instance().prefix_root();
        const QString runtimeIdentity = wine_binary();
        const PrefixInspection inspection =
            PrefixInspector::inspect(prefix, runtimeIdentity, false);
        if (!inspection.marker_valid
            || !inspection.required_components_present(false))
        {
            finish_optional_dxvk_failure(QStringLiteral(
                "The prefix is not ready for an optional graphics-layer change. "
                "Create or repair the prefix first."));
            return;
        }
        if (inspection.dxvk_installed)
        {
            emit user_notice(QStringLiteral("DXVK is already installed in this prefix."));
            return;
        }

        const QString winetricks = winetricks_path();
        if (winetricks.isEmpty() || !QFileInfo(winetricks).isExecutable())
        {
            finish_optional_dxvk_failure(QStringLiteral(
                "DXVK cannot be installed because Winetricks is unavailable."));
            return;
        }

        QVector<SetupCommand> commands;
        commands.push_back({QStringLiteral("Installing DXVK..."), winetricks,
                            {QStringLiteral("-q"),
                             PrefixInspector::dxvk_winetricks_verb()},
                            winetricks_environment(), 30 * 60 * 1000,
                            false, false, false});
        run_setup(std::move(commands), OperationKind::Dxvk);
    }

    bool Shell::start_game_probe(
        std::function<void(bool, std::optional<WindowsProcessInfo>)> completion)
    {
        if (!completion || game_probe_completion
            || game_probe_process->state() != QProcess::NotRunning)
        {
            return false;
        }

        const bool proton = game_probe_context_valid
            ? false : runtime_is_proton();
        const QString program = game_probe_context_valid
            ? game_probe_program_snapshot
            : (proton ? proton_wine_binary() : wine_binary());
        if (program.isEmpty())
            return false;

        QProcessEnvironment environment = game_probe_context_valid
            ? game_probe_environment_snapshot
            : (proton ? proton_env() : QProcessEnvironment::systemEnvironment());
        environment.insert(QStringLiteral("WINEPREFIX"),
                           game_probe_context_valid
                               ? game_probe_prefix_snapshot
                               : Config::instance().prefix_root());
        environment.insert(QStringLiteral("WINEDEBUG"), QStringLiteral("-all"));
#if defined(Q_OS_MACOS)
        environment.insert(QStringLiteral("WINEARCH"), QStringLiteral("win64"));
#endif
        if (!game_probe_context_valid && !proton)
            apply_wine_environment(environment);

        QString executable;
        QStringList arguments;
        QProcessEnvironment preparedEnvironment;
        const QStringList wineArguments {
            QStringLiteral("tasklist.exe"),
            QStringLiteral("/FO"), QStringLiteral("CSV"),
            QStringLiteral("/NH")
        };
        if (!prepare_host_invocation(program, wineArguments, environment,
                                     executable, arguments, preparedEnvironment))
        {
            return false;
        }

        game_probe_output.clear();
        game_probe_host_mode = false;
        game_probe_completion = std::move(completion);
        game_probe_process->setProcessEnvironment(preparedEnvironment);
        game_probe_process->setWorkingDirectory(QDir::homePath());
        game_probe_process->start(executable, arguments);
        game_probe_timeout_timer->start(k_game_probe_timeout_ms);
        return true;
    }

    void Shell::capture_game_probe_context()
    {
        const bool proton = runtime_is_proton();
        game_probe_program_snapshot =
            proton ? proton_wine_binary() : wine_binary();
        game_probe_prefix_snapshot = Config::instance().prefix_root();
        game_probe_environment_snapshot = proton
            ? proton_env() : QProcessEnvironment::systemEnvironment();
        game_probe_environment_snapshot.insert(
            QStringLiteral("WINEPREFIX"), game_probe_prefix_snapshot);
        game_probe_environment_snapshot.insert(
            QStringLiteral("WINEDEBUG"), QStringLiteral("-all"));
#if defined(Q_OS_MACOS)
        game_probe_environment_snapshot.insert(
            QStringLiteral("WINEARCH"), QStringLiteral("win64"));
#endif
        if (!proton)
            apply_wine_environment(game_probe_environment_snapshot);
        game_probe_context_valid = !game_probe_program_snapshot.isEmpty()
            && !game_probe_prefix_snapshot.isEmpty();
    }

    void Shell::clear_game_probe_context()
    {
        game_probe_context_valid = false;
        game_probe_program_snapshot.clear();
        game_probe_prefix_snapshot.clear();
        game_probe_environment_snapshot = {};
    }

    bool Shell::start_host_game_probe(
        std::function<void(bool, std::optional<WindowsProcessInfo>)> completion)
    {
        if (!completion || game_probe_completion
            || game_probe_process->state() != QProcess::NotRunning)
        {
            return false;
        }

        const QString ps = QStandardPaths::findExecutable(
            QStringLiteral("ps"));
        if (ps.isEmpty())
        {
            SPDLOG_WARN("Alicia host process probe cannot run because ps was not found");
            return false;
        }

        game_probe_output.clear();
        game_probe_host_mode = true;
        game_probe_completion = std::move(completion);
        game_probe_process->setProcessEnvironment(QProcessEnvironment::systemEnvironment());
        game_probe_process->setWorkingDirectory(QDir::homePath());
        game_probe_process->start(ps,
                                  {QStringLiteral("-axo"), QStringLiteral("pid=,command=")});
        game_probe_timeout_timer->start(k_game_probe_timeout_ms);
        return true;
    }

    bool Shell::start_game_lifecycle_probe(
        std::function<void(bool, std::optional<WindowsProcessInfo>)> completion)
    {
        if (!completion)
            return false;

        auto finalCompletion = std::make_shared<
            std::function<void(bool, std::optional<WindowsProcessInfo>)>>(
                std::move(completion));
#if defined(Q_OS_LINUX)




        return start_host_game_probe(
            [finalCompletion](const bool hostOk,
                              const std::optional<WindowsProcessInfo> hostInfo)
        {
            (*finalCompletion)(hostOk, hostInfo);
        });
#else
        auto fallback = [this, finalCompletion](
                            const bool tasklistOk,
                            const std::optional<WindowsProcessInfo> tasklistInfo)
        {
            if (tasklistInfo)
            {
                (*finalCompletion)(true, tasklistInfo);
                return;
            }

            if (!start_host_game_probe(
                    [finalCompletion, tasklistOk](
                        const bool hostOk,
                        const std::optional<WindowsProcessInfo> hostInfo)
            {
                (*finalCompletion)(tasklistOk || hostOk, hostInfo);
            }))
            {
                (*finalCompletion)(tasklistOk, std::nullopt);
            }
        };
        if (start_game_probe(fallback))
            return true;




        return start_host_game_probe(
            [finalCompletion](const bool hostOk,
                              const std::optional<WindowsProcessInfo> hostInfo)
        {
            (*finalCompletion)(hostOk, hostInfo);
        });
#endif
    }

    void Shell::handle_game_probe_output()
    {
        constexpr qsizetype k_max_probe_output = 512 * 1024;
        const QByteArray chunk = game_probe_process->readAllStandardOutput();
        if (chunk.isEmpty())
            return;
        if (game_probe_output.size() + chunk.size() > k_max_probe_output)
        {
            const qsizetype keep = qMax<qsizetype>(0, k_max_probe_output - chunk.size());
            game_probe_output = keep > 0 ? game_probe_output.right(keep) : QByteArray();
        }
        game_probe_output += chunk.right(k_max_probe_output);
    }

    void Shell::detect_existing_game()
    {
        if (game_running_confirmed || game_preflight_active
            || operation_kind != OperationKind::None || !is_wine_installed())
        {
            return;
        }
        const QString prefix = Config::instance().prefix_root();
        if (!QFileInfo(QDir(prefix).filePath(QStringLiteral("drive_c"))).isDir())
            return;



        if (!start_host_game_probe([this](const bool ok,
                                         const std::optional<WindowsProcessInfo> info)
        {
            if (!ok || !info || game_running_confirmed
                || game_preflight_active || operation_kind != OperationKind::None)
            {
                return;
            }
            tracked_game_version = Config::instance().game_version();
            attach_to_game_process(*info, true);
        }))
        {
            SPDLOG_DEBUG("existing Alicia process probe was already active");
        }
    }

    void Shell::begin_launch_preflight(PendingLaunch launch)
    {
        pending_launch = std::move(launch);
        has_pending_launch = true;
        game_preflight_active = true;
        tracked_game_version = pending_launch.version;
        emit game_starting(pending_launch.version);
        working(QStringLiteral("game-preflight"), -1.0, true);

        if (!start_game_lifecycle_probe(
                [this](const bool ok,
                       const std::optional<WindowsProcessInfo> info)
        {
            game_preflight_active = false;
            if (!has_pending_launch)
                return;
            if (!ok)
            {
                fail_game_launch(QStringLiteral(
                    "The launcher could not verify whether Alicia.exe is already running. "
                    "This safety check prevents terminating an active game session."));
                return;
            }
            if (info)
            {
                attach_to_game_process(*info, true);
                return;
            }
            clean_stale_prefix_processes();
        }))
        {
            game_preflight_active = false;
            fail_game_launch(QStringLiteral("The Alicia process safety check could not be started."));
        }
    }

    void Shell::clean_stale_prefix_processes()
    {
        const QString server = wineserver_binary();
        if (server.isEmpty() || !QFileInfo(server).isExecutable())
        {
            SPDLOG_WARN("wineserver was not found; stale-prefix cleanup is unavailable");
            emit user_notice(QStringLiteral(
                "The selected runtime does not expose its process-control helper, "
                "so stale prefix processes could not be cleaned before launch."));
            begin_first_launch_setup(pending_launch);
            return;
        }

        const bool proton = runtime_is_proton();
        QProcessEnvironment environment = proton ? proton_env() : QProcessEnvironment::systemEnvironment();
        environment.insert(QStringLiteral("WINEPREFIX"), Config::instance().prefix_root());
        environment.insert(QStringLiteral("WINEDEBUG"), QStringLiteral("-all"));
#if defined(Q_OS_MACOS)
        environment.insert(QStringLiteral("WINEARCH"), QStringLiteral("win64"));
#endif
        if (!proton)
            apply_wine_environment(environment);

        working(QStringLiteral("game-prefix-cleanup"), -1.0, true);
        SPDLOG_INFO("cleaning stale Wine processes for prefix {}",
                    Config::instance().prefix_root().toStdString());
        if (!start_process(server, {QStringLiteral("-k")}, environment, 15 * 1000,
                           OperationKind::PrefixCleanup,
                           [this](const command_result& result)
        {
            if (result.outcome == CommandOutcome::FailedToStart
                || result.outcome == CommandOutcome::TimedOut
                || result.outcome == CommandOutcome::Crashed
                || result.outcome == CommandOutcome::Cancelled)
            {
                fail_game_launch(QStringLiteral(
                    "The launcher could not stop stale runtime processes in the game prefix."));
                return;
            }
            wait_for_prefix_shutdown();
        }))
        {
            fail_game_launch(QStringLiteral(
                "The launcher could not start stale runtime-process cleanup."));
        }
    }

    void Shell::wait_for_prefix_shutdown()
    {
        const QString server = wineserver_binary();
        const bool proton = runtime_is_proton();
        QProcessEnvironment environment = proton ? proton_env() : QProcessEnvironment::systemEnvironment();
        environment.insert(QStringLiteral("WINEPREFIX"), Config::instance().prefix_root());
        environment.insert(QStringLiteral("WINEDEBUG"), QStringLiteral("-all"));
#if defined(Q_OS_MACOS)
        environment.insert(QStringLiteral("WINEARCH"), QStringLiteral("win64"));
#endif
        if (!proton)
            apply_wine_environment(environment);

        if (!start_process(server, {QStringLiteral("-w")}, environment, 15 * 1000,
                           OperationKind::PrefixCleanup,
                           [this](const command_result& result)
        {
            if (result.outcome == CommandOutcome::FailedToStart
                || result.outcome == CommandOutcome::TimedOut
                || result.outcome == CommandOutcome::Crashed
                || result.outcome == CommandOutcome::Cancelled)
            {
                fail_game_launch(QStringLiteral(
                    "The runtime did not finish shutting down stale prefix processes."));
                return;
            }
            SPDLOG_INFO("stale Wine prefix processes cleaned successfully");
            begin_first_launch_setup(pending_launch);
        }))
        {
            fail_game_launch(QStringLiteral(
                "The launcher could not wait for stale runtime processes to stop."));
        }
    }

    void Shell::run_game(const QString& user, const QString& token)
    {
        if (is_busy())
        {
            fail_user(QStringLiteral("Launcher Busy"),
                      is_game_running()
                        ? QStringLiteral("Alicia is already running.")
                        : QStringLiteral("Wait for the current runtime operation to finish."));
            return;
        }
        if (!is_wine_installed())
        {
            fail_user(QStringLiteral("Runtime Missing"),
                      QStringLiteral("The selected runtime is unavailable."));
            return;
        }
        if (user.isEmpty() || token.isEmpty())
        {
            fail_user(QStringLiteral("Sign In Required"),
                      QStringLiteral("Your login session is missing or expired. Sign in again."));
            return;
        }

        const auto version = Config::instance().game_version();
        const auto& profile = core::game::profile(version);
        const QString gameDirectory = Config::instance().game_install_path();
        const QString executable = QDir(gameDirectory).filePath(QString::fromLatin1(profile.executable_name));
        if (!QFileInfo(executable).isFile())
        {
            fail_user(QStringLiteral("Game Not Found"),
                      QStringLiteral("%1 was not found in the selected game folder.")
                          .arg(QString::fromLatin1(profile.executable_name)));
            return;
        }
        if (!Config::instance().path_inside_prefix(gameDirectory))
        {
            fail_user(QStringLiteral("Invalid Game Path"),
                      QStringLiteral("The configured game folder is outside the active compatibility prefix."));
            return;
        }

        PendingLaunch launch;
        launch.version = version;
        launch.user = user;
        launch.token = token;
        launch.game_directory = gameDirectory;
        launch.executable_path = executable;
#if defined(Q_OS_MACOS)
        game_diagnostic_path.clear();
        game_timeline_path.clear();
#endif
        capture_game_probe_context();
        if (!game_probe_context_valid)
        {
            fail_user(QStringLiteral("Runtime Not Available"),
                      QStringLiteral(
                          "The launcher could not prepare the Alicia process monitor "
                          "for the selected runtime."));
            return;
        }
        begin_launch_preflight(std::move(launch));
    }

    void Shell::begin_first_launch_setup(PendingLaunch launch)
    {
        pending_launch = std::move(launch);
        has_pending_launch = true;
        working(QStringLiteral("game-first-launch"));

        const bool proton = runtime_is_proton();
        QProcessEnvironment environment = proton ? proton_env() : QProcessEnvironment::systemEnvironment();
        if (!proton)
        {
            environment.insert(QStringLiteral("WINEPREFIX"), Config::instance().prefix_root());
            apply_wine_environment(environment);
        }

        QStringList arguments;
        if (proton) arguments << QStringLiteral("run");
        arguments << QStringLiteral("reg.exe") << QStringLiteral("query")
                  << QStringLiteral("HKCU\\Software\\Story of Alicia\\Launcher");
        const QString program = proton ? proton_binary() : wine_binary();
        if (!start_process(program, arguments, environment, k_registry_timeout_ms,
                           OperationKind::FirstLaunchQuery,
                           [this](const command_result& result) { handle_first_launch_query(result); }))
        {
            fail_game_launch(QStringLiteral(
                "First-launch setup could not inspect the compatibility registry."));
        }
    }

    bool Shell::write_first_launch_registry(const PendingLaunch& launch,
                                            const QString& queryOutput,
                                            QString& registryFile,
                                            bool& needsImport) const
    {
        const auto& profile = core::game::profile(launch.version);
        const QString baseFlag = QString::fromLatin1(profile.first_launch_registry_value);
        const QString audioFlag = QString::fromLatin1(profile.audio_settings_setup_registry_value);
        const bool querySucceeded = !queryOutput.isNull();
        const bool baseDone = querySucceeded && queryOutput.contains(baseFlag, Qt::CaseInsensitive);
        const bool audioDone = audioFlag.isEmpty()
            || (querySucceeded && queryOutput.contains(audioFlag, Qt::CaseInsensitive));
#if defined(Q_OS_MACOS)
        const QString macosProfile = Config::instance().macos_compatibility_profile();
        const bool macosProfileDone = querySucceeded
            && queryOutput.contains(QStringLiteral("MacCompatibilityProfile"), Qt::CaseInsensitive)
            && queryOutput.contains(macosProfile, Qt::CaseInsensitive);
#else
        const bool macosProfileDone = true;
#endif
        needsImport = !baseDone || !audioDone || !macosProfileDone;
#if defined(Q_OS_MACOS)


        needsImport = true;
#endif
        if (!needsImport)
            return true;

        QString registry = QStringLiteral("REGEDIT4\r\n\r\n");
#if !defined(Q_OS_MACOS)
        if (!baseDone && profile.video_settings_registry_key[0] != '\0')
        {
            registry += QStringLiteral("[HKEY_CURRENT_USER\\%1]\r\n")
                .arg(QString::fromLatin1(profile.video_settings_registry_key));
            registry += QStringLiteral("\"screenResolutionID\"=\"0\"\r\n");
            registry += QStringLiteral("\"screenWindowType\"=\"1\"\r\n");
            registry += QStringLiteral("\"Width\"=\"0\"\r\n");
            registry += QStringLiteral("\"Height\"=\"0\"\r\n\r\n");
        }
#else
        registry += QStringLiteral("[HKEY_CURRENT_USER\\Software\\Wine\\Direct3D]\r\n");
        registry += QStringLiteral("\"renderer\"=-\r\n\r\n");
        registry += QStringLiteral("[HKEY_CURRENT_USER\\Software\\Wine\\Mac Driver]\r\n");
        registry += QStringLiteral("\"RetinaMode\"=\"n\"\r\n\r\n");
        registry += QStringLiteral("[HKEY_CURRENT_USER\\Software\\Wine\\AppDefaults\\Alicia.exe\\Mac Driver]\r\n");
        if (macos_profile_uses_virtual_desktop(macosProfile))
        {
            registry += QStringLiteral("\"CaptureDisplaysForFullscreen\"=\"n\"\r\n");
            registry += QStringLiteral("\"AllowSetGamma\"=\"n\"\r\n");
        }
        else
        {
            registry += QStringLiteral("\"CaptureDisplaysForFullscreen\"=-\r\n");
            registry += QStringLiteral("\"AllowSetGamma\"=-\r\n");
        }
        const QString surfaceMode = macos_opengl_surface_mode(macosProfile);
        if (surfaceMode.isEmpty())
            registry += QStringLiteral("\"OpenGLSurfaceMode\"=-\r\n\r\n");
        else
            registry += QStringLiteral("\"OpenGLSurfaceMode\"=\"%1\"\r\n\r\n").arg(surfaceMode);
#endif
        if (!audioDone && profile.audio_settings_registry_key[0] != '\0')
        {
            registry += QStringLiteral("[HKEY_CURRENT_USER\\%1]\r\n")
                .arg(QString::fromLatin1(profile.audio_settings_registry_key));
            registry += QStringLiteral("\"VolBGM\"=\"30\"\r\n");
            registry += QStringLiteral("\"VolSFX\"=\"30\"\r\n\r\n");
        }
        registry += QStringLiteral("[HKEY_CURRENT_USER\\Software\\Story of Alicia\\Launcher]\r\n");
        if (!baseDone)
            registry += QStringLiteral("\"%1\"=dword:00000001\r\n").arg(baseFlag);
        if (!audioDone && !audioFlag.isEmpty())
            registry += QStringLiteral("\"%1\"=dword:00000001\r\n").arg(audioFlag);
#if defined(Q_OS_MACOS)
        registry += QStringLiteral("\"MacCompatibilityProfile\"=\"%1\"\r\n")
            .arg(macosProfile);
#endif

        registryFile = QDir(Config::instance().prefix_root())
            .filePath(QStringLiteral("drive_c/.soa-first-launch.reg"));
        QSaveFile file(registryFile);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
            return false;
        if (file.write(registry.toLatin1()) < 0)
            return false;
        return file.commit();
    }

    void Shell::handle_first_launch_query(const command_result& result)
    {
        if (!has_pending_launch)
            return;
        if (!result.started || result.outcome == CommandOutcome::TimedOut
            || result.outcome == CommandOutcome::Cancelled
            || result.outcome == CommandOutcome::FailedToStart
            || result.outcome == CommandOutcome::Crashed)
        {
            fail_game_launch(QStringLiteral(
                "First-launch setup could not inspect the compatibility registry."));
            return;
        }

        bool needsImport = false;
        QString registryFile;
        const QString queryOutput = result.exit_code == 0 ? result.output : QString();
        if (!write_first_launch_registry(pending_launch, queryOutput, registryFile, needsImport))
        {
            fail_game_launch(QStringLiteral(
                "First-launch setup could not prepare the default game settings."));
            return;
        }
        if (!needsImport)
        {
            launch_pending_game();
            return;
        }

        pending_registry_file = registryFile;
        const bool proton = runtime_is_proton();
        QProcessEnvironment environment = proton ? proton_env() : QProcessEnvironment::systemEnvironment();
        if (!proton)
        {
            environment.insert(QStringLiteral("WINEPREFIX"), Config::instance().prefix_root());
            apply_wine_environment(environment);
        }
        QStringList arguments;
        if (proton) arguments << QStringLiteral("run");
        arguments << QStringLiteral("regedit.exe") << QStringLiteral("/S")
                  << QStringLiteral("C:\\.soa-first-launch.reg");
        const QString program = proton ? proton_binary() : wine_binary();
        if (!start_process(program, arguments, environment, k_registry_timeout_ms,
                           OperationKind::FirstLaunchImport,
                           [this](const command_result& importResult)
        {
            QFile::remove(pending_registry_file);
            pending_registry_file.clear();
            if (!importResult.ok())
            {
                fail_game_launch(QStringLiteral(
                    "First-launch setup could not apply the default video and audio settings."));
                return;
            }
            launch_pending_game();
        }))
        {
            QFile::remove(pending_registry_file);
            pending_registry_file.clear();
            fail_game_launch(QStringLiteral(
                "First-launch setup could not start the compatibility-registry import."));
        }
    }

    void Shell::launch_pending_game()
    {
        if (!has_pending_launch)
            return;
        launch_game_process(pending_launch);
    }


    void Shell::begin_game_verification()
    {
        game_verification_attempts = 0;
        game_probe_failures = 0;
        missing_game_probes = 0;
        game_running_confirmed = false;
        game_wrapper_finished_elapsed_ms = -1;
        restored_game_session = false;
        tracked_game_process = {};
        tracked_game_host_process = {};
#if defined(Q_OS_MACOS)
        game_snapshot_armed = false;
        append_macos_launch_timeline(
            QStringLiteral("process_monitor_armed"),
            QStringLiteral("initial_delay_ms=%1 interval_ms=%2")
                .arg(k_game_initial_probe_delay_ms)
                .arg(k_game_start_probe_interval_ms));
#endif
        game_monitor_timer->start(k_game_initial_probe_delay_ms);
    }

    void Shell::poll_game_process()
    {
        if (operation_kind != OperationKind::Game || terminal_emitted)
            return;

        if (!start_game_lifecycle_probe(
                [this](const bool ok,
                       const std::optional<WindowsProcessInfo> info)
        {
            if (operation_kind != OperationKind::Game || terminal_emitted)
                return;

            if (!ok)
            {
                ++game_probe_failures;
                SPDLOG_WARN("Alicia process verification failed ({}/{})",
                            game_probe_failures, k_game_probe_failure_limit);
                if (game_probe_failures >= k_game_probe_failure_limit)
                {
                    if (!game_running_confirmed)
                    {
                        fail_game_launch(QStringLiteral(
                            "The runtime started, but the launcher could not verify whether Alicia.exe started."));
                        return;
                    }

                    if (game_probe_failures
                        >= k_game_attached_probe_failure_limit)
                    {
#if defined(Q_OS_LINUX)
                        stop_game_monitoring_uncertain(QStringLiteral(
                            "The Linux host process check failed repeatedly. "
                            "Monitoring has stopped without terminating Alicia. "
                            "The game may still be running; close it normally, then restart "
                            "the launcher before starting another session."));
#else
                        stop_game_monitoring_uncertain(QStringLiteral(
                            "Both Windows and host process checks failed repeatedly. "
                            "Monitoring has stopped without terminating Alicia. "
                            "The game may still be running; close it normally, then restart "
                            "the launcher before starting another session."));
#endif
                        return;
                    }
                }

                game_monitor_timer->start(game_running_confirmed
                    ? k_game_running_probe_interval_ms
                    : k_game_start_probe_interval_ms);
                return;
            }

            game_probe_failures = 0;
            if (info)
            {
                missing_game_probes = 0;
                if (!game_running_confirmed)
                {
                    attach_to_game_process(*info, false);
                }
                else
                {
                    const bool processChanged = tracked_game_process.pid != info->pid;
                    if (processChanged)
                    {
#if defined(Q_OS_LINUX)
                        SPDLOG_INFO("Alicia host process changed from PID {} to PID {}",
                                    tracked_game_process.pid, info->pid);
#else
                        SPDLOG_INFO("Alicia Windows process changed from PID {} to PID {}",
                                    tracked_game_process.pid, info->pid);
#endif
                    }
                    tracked_game_process = *info;
                    tracked_game_process.source_line = redact_sensitive_text(
                        tracked_game_process.source_line);
                    if (processChanged)
                        tracked_game_host_process = {};
#if defined(Q_OS_LINUX)
                    tracked_game_host_process = tracked_game_process;
#else
                    if (processChanged || tracked_game_host_process.pid <= 0)
                        refresh_game_host_diagnostics();
#endif
                    game_monitor_timer->start(k_game_running_probe_interval_ms);
                }
                return;
            }

            if (game_running_confirmed)
            {
                ++missing_game_probes;
                if (missing_game_probes >= k_game_missing_confirmation_limit)
                {



                    finish_game_session(0, false);
                    return;
                }
                game_monitor_timer->start(k_game_running_probe_interval_ms);
                return;
            }

            ++game_verification_attempts;
#if defined(Q_OS_MACOS)
            if (game_launch_clock.isValid() && game_launch_clock.elapsed() >= k_game_absolute_start_timeout_ms)
            {
                append_macos_launch_timeline(QStringLiteral("launcher_start_timeout"),
                    QStringLiteral("timeout_ms=%1 attempts=%2 wrapper_finished=%3 wrapper_exit=%4")
                        .arg(k_game_absolute_start_timeout_ms).arg(game_verification_attempts)
                        .arg(game_launcher_finished).arg(game_launcher_exit_code));
                fail_game_launch(QStringLiteral(
                    "Alicia.exe was not observed within %1 seconds. The launcher ended monitoring instead of waiting forever. "
                    "The launch timeline records whether the runtime host was still running or had already exited.")
                    .arg(k_game_absolute_start_timeout_ms / 1000));
                return;
            }
#endif
            if (game_launcher_finished && game_wrapper_finished_elapsed_ms >= 0
                && QDateTime::currentMSecsSinceEpoch() - game_wrapper_finished_elapsed_ms
                    >= k_game_wrapper_exit_grace_ms)
            {
#if defined(Q_OS_MACOS)
                append_macos_launch_timeline(
                    QStringLiteral("wrapper_exit_grace_expired"),
                    QStringLiteral("grace_ms=%1 attempts=%2 wrapper_exit=%3 crashed=%4")
                        .arg(k_game_wrapper_exit_grace_ms)
                        .arg(game_verification_attempts)
                        .arg(game_launcher_exit_code)
                        .arg(game_launcher_crashed));
#endif
                fail_game_launch(QStringLiteral(
                    "The runtime launch process exited and Alicia.exe was not observed within %1 seconds. "
                    "The launcher stopped monitoring instead of leaving the runtime host in the background. "
                    "Open the generated runtime-doctor and launch timeline reports for the first missing library or graphics error.")
                    .arg(k_game_wrapper_exit_grace_ms / 1000));
                return;
            }
            if (game_verification_attempts >= k_game_start_max_attempts)
            {
                QString detail = QStringLiteral(
                    "The runtime started, but Alicia.exe was never observed in the process list.");
                if (game_launcher_finished)
                {
                    detail += QStringLiteral(" The runtime launch process exited with code %1%2.")
                        .arg(game_launcher_exit_code)
                        .arg(game_launcher_crashed ? QStringLiteral(" after a crash") : QString());
                }
                detail += QStringLiteral(
                    " The launcher stayed in Launching instead of falsely reporting Running; "
                    "check the diagnostic log for the first runtime or DLL error.");
                fail_game_launch(detail);
                return;
            }

            game_monitor_timer->start(k_game_start_probe_interval_ms);
        }))
        {


            game_monitor_timer->start(100);
        }
    }

    void Shell::attach_to_game_process(const WindowsProcessInfo& info,
                                       const bool restoredSession)
    {
        if (info.pid <= 0 || game_running_confirmed)
            return;

        game_running_confirmed = true;
        game_preflight_active = false;
        restored_game_session = restoredSession;
        tracked_game_process = info;
        tracked_game_process.source_line = redact_sensitive_text(
            tracked_game_process.source_line);
#if defined(Q_OS_LINUX)
        tracked_game_host_process = tracked_game_process;
#else
        tracked_game_host_process = {};
#endif
        if (restoredSession)
        {
            if (!game_probe_context_valid)
                capture_game_probe_context();
            operation_kind = OperationKind::Game;
            terminal_emitted = false;
            current = command_result{};
            current.started = true;
            tracked_game_version = Config::instance().game_version();
        }
        else if (has_pending_launch)
        {
            tracked_game_version = pending_launch.version;
        }



        pending_launch.user.clear();
        pending_launch.token.clear();

#if defined(Q_OS_LINUX)
        SPDLOG_INFO("attached lifecycle monitor to Alicia.exe host PID {} (restored={}): {}",
                    tracked_game_process.pid, restoredSession,
                    tracked_game_process.source_line.toStdString());
#else
        SPDLOG_INFO("attached lifecycle monitor to Alicia.exe Windows PID {} (restored={}): {}",
                    tracked_game_process.pid, restoredSession,
                    tracked_game_process.source_line.toStdString());
#endif
#if defined(Q_OS_MACOS)
        append_macos_launch_timeline(
            QStringLiteral("alicia_process_observed"),
            QStringLiteral("windows_pid=%1 restored=%2")
                .arg(tracked_game_process.pid)
                .arg(restoredSession));
#endif
        if (!restoredSession && process->processId() > 0)
        {
            SPDLOG_INFO("Wine launch wrapper host PID {}; Alicia.exe is tracked independently",
                        process->processId());
        }

        working(QStringLiteral("game-running"), -1.0, true);
        emit game_started(tracked_game_version);
        game_monitor_timer->start(k_game_running_probe_interval_ms);
#if !defined(Q_OS_LINUX)
        refresh_game_host_diagnostics();
#endif
    }


#if defined(Q_OS_MACOS)
    void Shell::arm_macos_deep_diagnostic_sample()
    {
        if (!game_snapshot_generation)
            return;

        const auto generationToken = game_snapshot_generation;
        const std::uint64_t generation = generationToken->fetch_add(1) + 1;
        const QString path = game_diagnostic_path;
        const qint64 windowsPid = tracked_game_process.pid;
        const qint64 hostPid = tracked_game_host_process.pid;

        if (game_diagnostic_file && game_diagnostic_file->isOpen())
        {
            QTextStream trace(game_diagnostic_file);
            trace << "\n=== DEEP DIAGNOSTIC HOST SAMPLE ARMED ===\n"
                  << "UTC: " << QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs) << "\n"
                  << "Delay milliseconds: " << k_deep_diagnostic_sample_delay_ms << "\n"
                  << "Generation: " << generation << "\n"
                  << "Alicia Windows PID: " << windowsPid << "\n"
                  << "=== END HOST SAMPLE ARM RECORD ===\n";
            trace.flush();
            game_diagnostic_file->flush();
        }

        SPDLOG_INFO("armed Alicia deep-diagnostic host sample generation {} for {} ms",
                    generation, k_deep_diagnostic_sample_delay_ms);

        std::thread([generationToken, generation, path, windowsPid, hostPid]()
        {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(k_deep_diagnostic_sample_delay_ms));
            if (generationToken->load() != generation)
                return;

            const QString samplePath = path + QStringLiteral(".sample.txt");
            bool sampleStarted = false;
            bool sampleFinished = false;
            int sampleExitCode = -1;
            if (hostPid > 0 && QFileInfo(QStringLiteral("/usr/bin/sample")).isExecutable())
            {
                QProcess sampler;
                sampler.setProcessChannelMode(QProcess::MergedChannels);
                sampler.start(QStringLiteral("/usr/bin/sample"),
                              {QString::number(hostPid), QStringLiteral("8"),
                               QStringLiteral("-file"), samplePath});
                sampleStarted = sampler.waitForStarted(3000);
                sampleFinished = sampleStarted && sampler.waitForFinished(15000);
                if (!sampleFinished && sampler.state() != QProcess::NotRunning)
                {
                    sampler.kill();
                    sampler.waitForFinished(2000);
                }
                if (sampleFinished)
                    sampleExitCode = sampler.exitCode();
            }




            if (generationToken->load() != generation)
                return;
            const QString statusPath =
                path + QStringLiteral(".sample-status.txt");
            QFile file(statusPath);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate
                           | QIODevice::Text))
                return;

            QTextStream trace(&file);
            trace << "=== DEEP DIAGNOSTIC HOST SAMPLE ===\n"
                  << "UTC: " << QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs) << "\n"
                  << "Reason: opt-in delayed live-process sample\n"
                  << "Alicia Windows PID: " << windowsPid << "\n"
                  << "Alicia host PID at arm time: " << hostPid << "\n"
                  << "sample_started=" << sampleStarted
                  << " sample_finished=" << sampleFinished
                  << " sample_exit=" << sampleExitCode << "\n"
                  << "sample_path=" << samplePath << "\n"
                  << "Writer: isolated worker; main diagnostic stream was not modified\n"
                  << "=== END HOST SAMPLE ===\n";
            trace.flush();
            file.flush();
        }).detach();
    }

    bool Shell::macos_trace_has_fatal_game_failure() const
    {
        if (game_diagnostic_path.isEmpty())
            return false;
        QFile input(game_diagnostic_path);
        if (!input.open(QIODevice::ReadOnly))
            return false;
        constexpr qint64 tailBytes = 512 * 1024;
        if (input.size() > tailBytes)
            input.seek(input.size() - tailBytes);
        const QByteArray tail = input.readAll();
        return tail.contains("Unhandled exception")
            || tail.contains("Exception frame is not in stack limits => unable to dispatch exception")
            || tail.contains("wine: Unhandled")
            || tail.contains("stack overflow")
            || tail.contains("page fault on read access");
    }
#endif

    void Shell::refresh_game_host_diagnostics()
    {
        if (!game_running_confirmed)
            return;

        const qint64 expectedWindowsPid = tracked_game_process.pid;
        if (!start_host_game_probe([this, expectedWindowsPid](const bool ok,
                                                              const std::optional<WindowsProcessInfo> info)
        {
            if (!game_running_confirmed || tracked_game_process.pid != expectedWindowsPid)
                return;
            if (!ok || !info)
            {
                SPDLOG_DEBUG(
                    "Alicia.exe Windows PID {} is confirmed, but its host PID was not resolved",
                    tracked_game_process.pid);
                return;
            }

            tracked_game_host_process = *info;
            tracked_game_host_process.source_line = redact_sensitive_text(
                tracked_game_host_process.source_line);
            SPDLOG_INFO("attached diagnostics to Alicia.exe host PID {} for Windows PID {}: {}",
                        tracked_game_host_process.pid, tracked_game_process.pid,
                        tracked_game_host_process.source_line.toStdString());
#if defined(Q_OS_MACOS)
            append_macos_launch_timeline(
                QStringLiteral("alicia_host_process_observed"),
                QStringLiteral("host_pid=%1 windows_pid=%2")
                    .arg(tracked_game_host_process.pid)
                    .arg(tracked_game_process.pid));
            if (Config::instance().macos_deep_diagnostics()
                && !game_snapshot_armed)
            {
                game_snapshot_armed = true;
                arm_macos_deep_diagnostic_sample();
            }
#endif
        }))
        {
            SPDLOG_DEBUG("host PID diagnostics probe deferred because another probe is active");
        }
    }

#if defined(Q_OS_MACOS)
    void Shell::begin_macos_launch_timeline(const QString& profile,
                                            const PendingLaunch& launch)
    {
        ++game_launch_generation;
        game_launch_clock.restart();
        game_timeline_finished = false;
        game_timeline_seen_create_device = false;
        game_timeline_seen_present = false;
        game_timeline_seen_draw = false;
        game_timeline_seen_exception = false;
        game_launch_session_id = QDateTime::currentDateTimeUtc().toString(
            QStringLiteral("yyyyMMdd-HHmmss-zzz"));
        game_timeline_path = game_diagnostic_path;
        if (game_timeline_path.endsWith(QStringLiteral(".log")))
            game_timeline_path.chop(4);
        game_timeline_path += QStringLiteral(".timeline.jsonl");
        QFile::remove(game_timeline_path);

        append_macos_launch_timeline(
            QStringLiteral("session_started"),
            QStringLiteral("profile=%1 game_version=%2 prefix=%3 executable=%4")
                .arg(profile,
                     core::game::to_string(launch.version),
                     Config::instance().prefix_root(), launch.executable_path));

        const quint64 generation = game_launch_generation;
        for (const int seconds : {10, 30, 60, 120})
        {
            QTimer::singleShot(seconds * 1000, this, [this, generation, seconds]()
            {
                if (generation != game_launch_generation || game_timeline_finished
                    || operation_kind != OperationKind::Game)
                    return;
                append_macos_launch_timeline(
                    QStringLiteral("process_still_active"),
                    QStringLiteral("milestone_seconds=%1 game_confirmed=%2 wrapper_running=%3")
                        .arg(seconds).arg(game_running_confirmed)
                        .arg(process->state() != QProcess::NotRunning));
            });
        }
    }

    void Shell::append_macos_launch_timeline(const QString& event,
                                             const QString& details)
    {
        if (game_timeline_path.isEmpty() || game_timeline_finished)
            return;

        QJsonObject object;
        object.insert(QStringLiteral("schema"), 1);
        object.insert(QStringLiteral("session"), game_launch_session_id);
        object.insert(QStringLiteral("utc"),
                      QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
        object.insert(QStringLiteral("elapsed_ms"),
                      game_launch_clock.isValid() ? game_launch_clock.elapsed() : 0);
        object.insert(QStringLiteral("event"), event);
        if (!details.isEmpty())
            object.insert(QStringLiteral("details"), redact_sensitive_text(details));

        QFile file(game_timeline_path);
        if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        {
            file.write(QJsonDocument(object).toJson(QJsonDocument::Compact));
            file.write("\n");
            file.flush();
        }
        SPDLOG_INFO("Alicia timeline T+{}ms {} {}",
                    game_launch_clock.isValid() ? game_launch_clock.elapsed() : 0,
                    event.toStdString(), redact_sensitive_text(details).toStdString());
    }

    void Shell::finish_macos_launch_timeline(const QString& outcome,
                                             const int exitCode,
                                             const bool crashed,
                                             const QString& details)
    {
        if (game_timeline_finished || game_timeline_path.isEmpty())
            return;
        append_macos_launch_timeline(
            QStringLiteral("session_finished"),
            QStringLiteral("outcome=%1 exit_code=%2 crashed=%3 duration_ms=%4 %5")
                .arg(outcome).arg(exitCode).arg(crashed)
                .arg(game_launch_clock.isValid() ? game_launch_clock.elapsed() : 0)
                .arg(details));
        game_timeline_finished = true;
        SPDLOG_INFO("Alicia launch timeline complete: {} outcome={}",
                    game_timeline_path.toStdString(), outcome.toStdString());
    }
#endif

    void Shell::handle_game_launcher_finished(const int exitCode,
                                              const QProcess::ExitStatus exitStatus)
    {
        if (terminal_emitted || operation_kind != OperationKind::Game)
            return;

        command_timer->stop();
        force_kill_timer->stop();
        force_kill_process_id = -1;
        handle_output();
        game_launcher_finished = true;
        game_wrapper_finished_elapsed_ms = QDateTime::currentMSecsSinceEpoch();
        game_launcher_exit_code = exitCode;
        game_launcher_crashed = exitStatus == QProcess::CrashExit;
        current.exit_code = exitCode;
        current.crashed = game_launcher_crashed;
        current.outcome = game_launcher_crashed
            ? CommandOutcome::Crashed
            : (exitCode == 0 ? CommandOutcome::Success : CommandOutcome::NonZeroExit);

        SPDLOG_INFO(
            "Wine launch wrapper finished (exit {}, crashed={}); Alicia.exe process monitoring remains authoritative",
            exitCode, game_launcher_crashed);
#if defined(Q_OS_MACOS)
        append_macos_launch_timeline(
            QStringLiteral("wine_wrapper_finished"),
            QStringLiteral("exit_code=%1 crashed=%2 game_confirmed=%3")
                .arg(exitCode).arg(game_launcher_crashed).arg(game_running_confirmed));
#endif



        game_monitor_timer->start(0);
    }

    void Shell::fail_game_launch(const QString& message)
    {
        if (terminal_emitted && operation_kind == OperationKind::None
            && !game_preflight_active && !has_pending_launch)
        {
            return;
        }

        QString userMessage = util::i18n::translate(message);
#if defined(Q_OS_MACOS)
        if (!game_diagnostic_path.isEmpty())
        {
            userMessage += util::i18n::translate(
                "\n\nDiagnostic log: %1\nLaunch timeline: %2")
                .arg(game_diagnostic_path, game_timeline_path);
        }
#endif

        game_monitor_timer->stop();
        game_probe_timeout_timer->stop();
#if defined(Q_OS_MACOS)
        if (game_snapshot_generation)
            game_snapshot_generation->fetch_add(1);
#endif
        force_kill_timer->stop();
        force_kill_process_id = -1;
        game_probe_completion = {};
        game_probe_output.clear();
        if (game_probe_process->state() != QProcess::NotRunning)
            game_probe_process->kill();

        const bool hadConfirmedGame = game_running_confirmed;
        const auto version = tracked_game_version;
        const int launcherExitCode = game_launcher_exit_code;
        const bool launcherCrashed = game_launcher_crashed;
        if (process->state() != QProcess::NotRunning)
        {
            force_kill_process_id = process->processId();
            process->terminate();
            force_kill_timer->start(3000);
        }

        terminal_emitted = true;
        command_timer->stop();
        operation_kind = OperationKind::None;
        completion_handler = {};
        game_running_confirmed = false;
        game_preflight_active = false;
        game_launcher_finished = false;
        game_wrapper_finished_elapsed_ms = -1;
        restored_game_session = false;
        has_pending_launch = false;
        tracked_game_process = {};
        tracked_game_host_process = {};
        pending_launch = {};
        clear_game_probe_context();

#if defined(Q_OS_MACOS)
        finish_macos_launch_timeline(
            hadConfirmedGame ? QStringLiteral("monitor_failure_after_process_seen")
                             : QStringLiteral("launch_failed"),
            launcherExitCode, launcherCrashed, message);
        if (game_diagnostic_file && game_diagnostic_file->isOpen())
            game_diagnostic_file->close();
#endif
        current.outcome = CommandOutcome::FailedToStart;
        current.error_message = userMessage;
        current.crashed = launcherCrashed;
        current.exit_code = launcherExitCode;
        emit command_finished(current);
        if (hadConfirmedGame)
            emit game_exited(version, launcherExitCode, launcherCrashed);
        fail_user(QStringLiteral("Game Launch Failed"), userMessage);
    }

    void Shell::stop_game_monitoring_uncertain(const QString& message)
    {
        if (!game_running_confirmed)
            return;

        QString userMessage = util::i18n::translate(message);
#if defined(Q_OS_MACOS)
        if (!game_diagnostic_path.isEmpty())
        {
            userMessage += util::i18n::translate(
                "\n\nDiagnostic log: %1\nLaunch timeline: %2")
                .arg(game_diagnostic_path, game_timeline_path);
        }
#endif

        game_monitor_timer->stop();
        game_probe_timeout_timer->stop();
#if defined(Q_OS_MACOS)
        if (game_snapshot_generation)
            game_snapshot_generation->fetch_add(1);
#endif
        game_probe_completion = {};
        game_probe_output.clear();
        if (game_probe_process->state() != QProcess::NotRunning)
            game_probe_process->kill();

        const int lastWrapperExit = game_launcher_exit_code;
        game_lifecycle_uncertain = true;
        game_running_confirmed = false;
        game_preflight_active = false;
        game_launcher_finished = false;
        game_wrapper_finished_elapsed_ms = -1;
        restored_game_session = false;
        has_pending_launch = false;
        terminal_emitted = true;
        command_timer->stop();
        operation_kind = OperationKind::None;
        completion_handler = {};

#if defined(Q_OS_MACOS)
        finish_macos_launch_timeline(
            QStringLiteral("monitoring_lost"), lastWrapperExit, false, message);
        if (game_diagnostic_file && game_diagnostic_file->isOpen())
            game_diagnostic_file->close();
#endif

        current.outcome = CommandOutcome::FailedToStart;
        current.exit_code = -1;
        current.crashed = false;
        current.error_message = userMessage;
        emit command_finished(current);
        fail_user(QStringLiteral("Game Monitoring Stopped"), userMessage);



        working(QStringLiteral("game-monitoring-uncertain"), -1.0, true);

        tracked_game_process = {};
        tracked_game_host_process = {};
        pending_launch = {};
        clear_game_probe_context();
    }

    void Shell::finish_game_session(const int exitCode, const bool crashed)
    {
        if (!game_running_confirmed || operation_kind != OperationKind::Game)
            return;

#if defined(Q_OS_MACOS)
        const bool traceFatalFailure = macos_trace_has_fatal_game_failure();
#else
        const bool traceFatalFailure = false;
#endif


        const bool effectiveCrashed = crashed || traceFatalFailure;

        game_monitor_timer->stop();
        game_probe_timeout_timer->stop();
#if defined(Q_OS_MACOS)
        if (game_snapshot_generation)
            game_snapshot_generation->fetch_add(1);
#endif
        const auto version = tracked_game_version;
        const bool restored = restored_game_session;

        terminal_emitted = true;
        command_timer->stop();
        force_kill_timer->stop();
        force_kill_process_id = -1;
        handle_output();
#if defined(Q_OS_MACOS)
        const qint64 sessionDurationMs = game_launch_clock.isValid()
            ? game_launch_clock.elapsed() : 0;
        QString timelineOutcome;
        if (effectiveCrashed)
            timelineOutcome = QStringLiteral("crash");
        else if (sessionDurationMs < 15000)
            timelineOutcome = QStringLiteral("early_exit");
        else if (sessionDurationMs >= 60000)
            timelineOutcome = QStringLiteral("long_session_exit");
        else
            timelineOutcome = QStringLiteral("short_session_exit");
        finish_macos_launch_timeline(
            timelineOutcome, exitCode, effectiveCrashed,
            QStringLiteral("windows_pid=%1 host_pid=%2 wrapper_exit=%3 fatal_trace=%4 present_seen=%5 draw_seen=%6")
                .arg(tracked_game_process.pid).arg(tracked_game_host_process.pid)
                .arg(game_launcher_exit_code).arg(traceFatalFailure)
                .arg(game_timeline_seen_present).arg(game_timeline_seen_draw));
        if (game_diagnostic_file && game_diagnostic_file->isOpen())
        {
            QTextStream trace(game_diagnostic_file);
            trace << "--- Alicia process disappeared; wrapper exit=" << game_launcher_exit_code
                  << " wrapper_crashed=" << game_launcher_crashed
                  << " fatal_trace=" << traceFatalFailure
                  << " effective_crashed=" << effectiveCrashed << " ---\n";
            trace.flush();
            const QString crashedExecutable = pending_launch.executable_path;
            game_diagnostic_file->close();
            append_macos_crash_analysis(game_diagnostic_path, crashedExecutable);
        }
#endif
        current.started = true;
        current.exit_code = exitCode;
        current.crashed = effectiveCrashed;
        current.outcome = effectiveCrashed
            ? CommandOutcome::Crashed
            : (exitCode == 0 ? CommandOutcome::Success : CommandOutcome::NonZeroExit);

#if defined(Q_OS_LINUX)
        SPDLOG_INFO(
            "Alicia.exe host PID {} is no longer present (restored={}, wrapper_exit={})",
            tracked_game_process.pid, restored, game_launcher_exit_code);
#else
        SPDLOG_INFO(
            "Alicia.exe Windows PID {} is no longer present (host_pid={}, restored={}, wrapper_exit={})",
            tracked_game_process.pid, tracked_game_host_process.pid, restored,
            game_launcher_exit_code);
#endif

        operation_kind = OperationKind::None;
        completion_handler = {};
        game_running_confirmed = false;
        game_preflight_active = false;
        game_launcher_finished = false;
        game_wrapper_finished_elapsed_ms = -1;
        restored_game_session = false;
        has_pending_launch = false;
        tracked_game_process = {};
        tracked_game_host_process = {};
        pending_launch = {};
        clear_game_probe_context();

        emit command_finished(current);
        emit game_exited(version, exitCode, effectiveCrashed);
        if (effectiveCrashed || exitCode != 0)
        {
#if defined(Q_OS_MACOS)
            fail(QStringLiteral(
                "The game exited unexpectedly. Diagnostic log: %1")
                .arg(game_diagnostic_path));
#else
            fail(QStringLiteral("The game exited unexpectedly."));
#endif
        }
        else
            done(QStringLiteral("Game exited."));



        if (process->state() != QProcess::NotRunning)
        {
            force_kill_process_id = process->processId();
            process->terminate();
            force_kill_timer->start(3000);
        }
    }

    void Shell::launch_game_process(const PendingLaunch& launch)
    {
        const auto& profile = core::game::profile(launch.version);
        const QString prefix = Config::instance().prefix_root();
        const bool proton = runtime_is_proton();
#if defined(Q_OS_MACOS)
        const bool requestedDxvk = false;
        const bool effectiveDxvk = false;
        const QString compatibilityProfile = Config::instance().macos_compatibility_profile();
        const bool deepDiagnostics = Config::instance().macos_deep_diagnostics();
        const QString configuredRuntime = Config::instance().wine_binary();
        const QString runtimeExecutable =
            macos::resolve_wine_executable(configuredRuntime);
        core::runtime::RuntimeInstallation sessionRuntime;
        QString runtimeSource;
        if (core::runtime::RuntimeManager::is_managed_selector(configuredRuntime))
        {
            runtimeSource = QStringLiteral("managed");
            sessionRuntime = core::runtime::RuntimeManager().active();
        }
        else if (QFileInfo(QDir(configuredRuntime).filePath(
                     QStringLiteral("runtime.json"))).isFile())
        {
            sessionRuntime =
                core::runtime::RuntimeManager::inspect_package(configuredRuntime);
            const QString bundled =
                core::runtime::RuntimeProvider::bundled_runtime_root();
            runtimeSource = !bundled.isEmpty()
                    && QFileInfo(bundled).canonicalFilePath()
                        == QFileInfo(configuredRuntime).canonicalFilePath()
                ? QStringLiteral("bundled-package")
                : QStringLiteral("custom-package");
        }
        else
        {
            runtimeSource = QStringLiteral("custom-executable");
        }
        const QString diagnosticDir = QDir(macos::default_log_root())
                                          .filePath(QStringLiteral("diagnostics"));
        QDir().mkpath(diagnosticDir);
        prune_old_diagnostics(diagnosticDir);
        game_diagnostic_path = QDir(diagnosticDir).filePath(
            QStringLiteral("alicia-%1.log").arg(QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz"))));
        if (game_diagnostic_file->isOpen())
            game_diagnostic_file->close();
        game_diagnostic_file->setFileName(game_diagnostic_path);
        if (game_diagnostic_file->open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        {
            QTextStream trace(game_diagnostic_file);
            trace << "Story of Alicia macOS launcher diagnostic\n"
                  << "UTC: " << QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs) << "\n"
                  << "Profile: " << compatibilityProfile << "\n"
                  << "Deep diagnostics: " << deepDiagnostics << "\n"
                  << "Effective WINEDEBUG: "
                  << macos_wine_debug_value(deepDiagnostics) << "\n"
                  << "Graphics registry: "
                  << macos_registry_diagnostic_summary(prefix) << "\n"
                  << "Runtime source: " << runtimeSource << "\n"
                  << "Runtime selector: " << configuredRuntime << "\n"
                  << "Runtime executable: " << runtimeExecutable << "\n"
                  << "Prefix: " << prefix << "\n"
                  << "Game directory: " << launch.game_directory << "\n";
            if (sessionRuntime.usable)
            {
                trace << "Runtime identity: "
                      << sessionRuntime.manifest.identity() << "\n"
                      << "Runtime version: "
                      << sessionRuntime.manifest.runtime_version << "\n"
                      << "Runtime base version: "
                      << sessionRuntime.manifest.wine_version << "\n"
                      << "Runtime graphics backends: "
                      << sessionRuntime.manifest.graphics_backends.join(
                             QLatin1Char(',')) << "\n";
            }
            else if (!sessionRuntime.failure.isEmpty())
            {
                trace << "Runtime manifest status: "
                      << sessionRuntime.failure << "\n";
            }
            if (deepDiagnostics
                && runtimeSource.startsWith(QStringLiteral("custom-")))
            {
                trace << "Custom override scrutiny: enabled; compare this "
                         "selector, executable, architecture, libraries, and "
                         "first-fault output with the bundled runtime.\n";
            }
            const QStringList dlls {QStringLiteral("d3d9.dll"), QStringLiteral("d3dx9_31.dll"),
                QStringLiteral("d3dx9_42.dll"), QStringLiteral("D3DCompiler_42.dll"),
                QStringLiteral("D3DCompiler_47.dll"), QStringLiteral("xinput1_3.dll"),
                QStringLiteral("msvcp100.dll"), QStringLiteral("msvcr100.dll"),
                QStringLiteral("PhysXLoader.dll"), QStringLiteral("PhysXCore.dll"),
                QStringLiteral("PhysXCooking.dll"), QStringLiteral("PhysXDevice.dll"),
                QStringLiteral("cudart32_30_9.dll")};
            for (const QString& dll : dlls)
            {
                trace << "DLL candidate " << dll << ": game="
                      << QFileInfo(QDir(launch.game_directory).filePath(dll)).exists()
                      << " syswow64=" << QFileInfo(QDir(prefix).filePath(QStringLiteral("drive_c/windows/syswow64/") + dll)).exists()
                      << " system32=" << QFileInfo(QDir(prefix).filePath(QStringLiteral("drive_c/windows/system32/") + dll)).exists()
                      << "\n";
            }
            trace << "--- Wine output ---\n";
            trace.flush();
            SPDLOG_INFO("macOS Alicia diagnostics: {}", game_diagnostic_path.toStdString());
        }
        else
        {
            SPDLOG_WARN("could not create Alicia diagnostic trace: {}", game_diagnostic_file->errorString().toStdString());
        }
        begin_macos_launch_timeline(compatibilityProfile, launch);
        append_macos_launch_timeline(
            QStringLiteral("diagnostics_configured"),
            QStringLiteral("deep=%1 WINEDEBUG=%2 registry=%3")
                .arg(deepDiagnostics)
                .arg(macos_wine_debug_value(deepDiagnostics),
                     macos_registry_diagnostic_summary(prefix)));
#else
        const bool requestedDxvk = Config::instance().use_dxvk();
        const bool effectiveDxvk = requestedDxvk && (proton || PrefixInspector::dxvk_installed(prefix));
#endif
        if (requestedDxvk && !effectiveDxvk)
        {
            emit user_notice(QStringLiteral(
                "DXVK was requested but is not installed. Falling back to WineD3D for this launch."));
        }

        QString windowsExecutable;
        if (!proton)
        {
            QString pathError;
            windowsExecutable = windows_path_for_prefix_file(prefix, launch.executable_path, &pathError);
            if (windowsExecutable.isEmpty())
            {
                fail_game_launch(pathError);
                return;
            }
        }

        QStringList gameArguments;
        gameArguments << QStringLiteral("-GameID") << QString::fromLatin1(profile.launch_game_id)
                      << QStringLiteral("-ID") << QStringLiteral("[%1]").arg(launch.user)
                      << QStringLiteral("-OP") << QStringLiteral("[%1]").arg(launch.token);
        const auto customArguments = util::launch_arguments::validate(Config::instance().game_args());
        if (!customArguments.valid)
        {
            fail_game_launch(QStringLiteral("Invalid launch arguments: %1")
                                 .arg(customArguments.error));
            return;
        }
        gameArguments.append(customArguments.arguments);

#if defined(Q_OS_MACOS)
        const QStringList expectedLocalFiles {
            QStringLiteral("d3dx9_31.dll"),
            QStringLiteral("d3dx9_42.dll"),
            QStringLiteral("D3DCompiler_42.dll"),
            QStringLiteral("xinput1_3.dll"),
            QStringLiteral("msvcp100.dll"),
            QStringLiteral("msvcr100.dll"),
            QStringLiteral("PhysXLoader.dll"),
            QStringLiteral("PhysXCore.dll"),
            QStringLiteral("PhysXCooking.dll"),
            QStringLiteral("PhysXDevice.dll"),
            QStringLiteral("cudart32_30_9.dll")
        };
        QStringList missingLocalFiles;
        for (const QString& file : expectedLocalFiles)
        {
            if (!QFileInfo(QDir(launch.game_directory).filePath(file)).isFile())
                missingLocalFiles.append(file);
        }
        if (!missingLocalFiles.isEmpty())
        {
            fail_game_launch(QStringLiteral(
                "The macOS game package is incomplete. Repair the game installation; "
                "these required local components are missing:\n%1")
                .arg(missingLocalFiles.join(QStringLiteral("\n"))));
            return;
        }

        if (compatibilityProfile == QStringLiteral("low-graphics"))
        {
            QString patchMessage;
            if (!patch_existing_alice_config(launch.game_directory, patchMessage))
            {
                fail_game_launch(QStringLiteral("Compatibility profile failed: %1")
                                     .arg(patchMessage));
                return;
            }
            emit user_notice(patchMessage);
        }
#endif

        process->setWorkingDirectory(launch.game_directory);
        if (proton)
        {
            const QString umu = umu_path();
            if (umu.isEmpty() || !QFileInfo(umu).isExecutable())
            {
                fail_game_launch(QStringLiteral(
                    "Proton launch requires a working umu-run installation."));
                return;
            }

            QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
            environment.insert(QStringLiteral("GAMEID"), QStringLiteral("0"));
            environment.insert(QStringLiteral("STORE"), QStringLiteral("none"));
            environment.insert(QStringLiteral("PROTONPATH"), proton_root());
            environment.insert(QStringLiteral("WINEPREFIX"), prefix);
            environment.insert(QStringLiteral("STEAM_COMPAT_DATA_PATH"),
                               Config::instance().proton_compat_data_root());
            environment.insert(QStringLiteral("STEAM_COMPAT_LIBRARY_PATHS"),
                               launch.game_directory + QLatin1Char(':') + prefix);
            environment.insert(QStringLiteral("WINEDLLOVERRIDES"), QStringLiteral("winegstreamer="));
            if (!effectiveDxvk)
                environment.insert(QStringLiteral("PROTON_USE_WINED3D"), QStringLiteral("1"));
            apply_wine_environment(environment);

            QStringList arguments {launch.executable_path};
            arguments.append(gameArguments);
            if (!start_process(umu, arguments, environment, 0, OperationKind::Game))
            {
                fail_game_launch(QStringLiteral("umu-run could not be started."));
            }
        }
        else
        {
            QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
            environment.insert(QStringLiteral("WINEPREFIX"), prefix);
#if defined(Q_OS_MACOS)
            environment.insert(QStringLiteral("WINEARCH"), QStringLiteral("win64"));
            environment.insert(QStringLiteral("WINEDEBUG"),
                               macos_wine_debug_value(deepDiagnostics));


            environment.insert(QStringLiteral("WINEDLLOVERRIDES"),
                               QStringLiteral("d3d9,ddraw,dinput8,dsound=b;d3dx9_31,d3dx9_42,d3dcompiler_42,d3dcompiler_47,msvcp100,msvcr100=n,b"));
            environment.insert(QStringLiteral("WINE_FULLSCREEN_FSR"), QStringLiteral("0"));
#else
            environment.insert(QStringLiteral("WINEDEBUG"), QStringLiteral("-all"));
            environment.insert(QStringLiteral("WINEDLLOVERRIDES"), effectiveDxvk
                ? QStringLiteral("d3d9,d3d10core,d3d11,dxgi=n")
                : QStringLiteral("d3d9,d3d10core,d3d11,dxgi=b"));
#endif
            apply_wine_environment(environment);

            QStringList arguments;
#if defined(Q_OS_MACOS)
            const bool virtualDesktop = macos_profile_uses_virtual_desktop(compatibilityProfile);
            if (virtualDesktop)
            {
                arguments << QStringLiteral("explorer")
                          << QStringLiteral("/desktop=StoryOfAlicia,1280x720")
                          << windowsExecutable;
            }
            else
            {
                arguments << windowsExecutable;
            }
            arguments.append(gameArguments);
            if (sessionRuntime.usable)
            {
                SPDLOG_INFO(
                    "Alicia runtime session: source={} identity={} version={} base={} prefix_schema={} profile={}",
                    runtimeSource.toStdString(),
                    sessionRuntime.manifest.identity().toStdString(),
                    sessionRuntime.manifest.runtime_version.toStdString(),
                    sessionRuntime.manifest.wine_version.toStdString(),
                    sessionRuntime.manifest.prefix_schema,
                    compatibilityProfile.toStdString());
            }
            const QStringList redactedArguments = redacted_command_args(arguments);
            SPDLOG_INFO("macOS launch profile={} runtime={} prefix={} executable={} virtual_desktop={} retina={} opengl_surface={} command={} {}",
                        compatibilityProfile.toStdString(),
                        macos::runtime_root_for_executable(runtimeExecutable).toStdString(),
                        prefix.toStdString(), windowsExecutable.toStdString(),
                        virtualDesktop,
                        macos_graphics_retina_enabled(compatibilityProfile) ? "on" : "off",
                        macos_opengl_surface_mode(compatibilityProfile).isEmpty()
                            ? "front-opaque-default" : "behind",
                        wine_binary().toStdString(), redactedArguments.join(QLatin1Char(' ')).toStdString());
            if (game_diagnostic_file && game_diagnostic_file->isOpen())
            {
                QTextStream trace(game_diagnostic_file);
                trace << "\n=== EFFECTIVE ALICIA COMMAND ===\n"
                      << "program=" << wine_binary() << "\n"
                      << "arguments=" << redactedArguments.join(QLatin1Char(' ')) << "\n"
                      << "working_directory=" << launch.game_directory << "\n";
                if (sessionRuntime.usable)
                {
                    trace << "runtime_source=" << runtimeSource << "\n"
                          << "runtime_identity="
                          << sessionRuntime.manifest.identity() << "\n"
                          << "runtime_version="
                          << sessionRuntime.manifest.runtime_version << "\n"
                          << "runtime_base_version="
                          << sessionRuntime.manifest.wine_version << "\n"
                          << "runtime_prefix_schema="
                          << sessionRuntime.manifest.prefix_schema << "\n";
                }
                trace.flush();
            }
#else
            arguments << windowsExecutable;
            arguments.append(gameArguments);
#endif
#if defined(Q_OS_MACOS)
            if (!start_process(wine_binary(), arguments, environment, 0,
                               OperationKind::Game))
            {
                fail_game_launch(QStringLiteral(
                    "The selected macOS runtime could not start Alicia.exe. "
                    "Check the generated launch timeline and diagnostic report."));
            }
#else
            if (!start_process(wine_binary(), arguments, environment, 0, OperationKind::Game))
                fail_game_launch(QStringLiteral("Wine could not start Alicia.exe."));
#endif
        }
    }

}
