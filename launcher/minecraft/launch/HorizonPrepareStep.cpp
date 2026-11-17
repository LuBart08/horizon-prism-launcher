// SPDX-License-Identifier: GPL-3.0-only
/*
 *  horizon-prism - Prism Launcher fork for Project Horizon
 *  Copyright (C) 2026 horizon-prism contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include "HorizonPrepareStep.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QProcess>
#include <QRegularExpression>

#include "Application.h"
#include "FileSystem.h"
#include "launch/LaunchTask.h"
#include "minecraft/HorizonInstance.h"

namespace {
// Gaps that `horizon-prepare --complete` fills (sounds, built-in packs, title
// panorama, Unifont glyphs, bundled languages). They are optional: the game
// runs without them (silent, with the classic panorama, etc.).
const QStringList OPTIONAL_GAPS = { "packs", "panorama", "fonts", "languages", "sounds" };

constexpr int DEFAULT_JAVA_MAJOR = 25;

int javaMajorOf(const QString& home)
{
    QFile release(FS::PathCombine(home, "release"));
    if (!release.open(QIODevice::ReadOnly))
        return -1;

    static const QRegularExpression versionPattern("JAVA_VERSION=\"(\\d+)");
    const auto match = versionPattern.match(QString::fromUtf8(release.readAll()));
    return match.hasMatch() ? match.captured(1).toInt() : -1;
}

QString javaHomeOf(const QString& path)
{
    const QFileInfo info(path);
    if (info.fileName() == "java" || info.fileName() == "java.exe")
        return QDir::cleanPath(info.absolutePath() + "/..");
    return path;
}
}  // namespace

HorizonPrepareStep::HorizonPrepareStep(LaunchTask* parent) : LaunchStep(parent)
{
    m_process.setProcessEnvironment(parent->instance()->createEnvironment());
    connect(&m_process, &LoggedProcess::log, this, &HorizonPrepareStep::onPrepareLog);
    connect(&m_process, &LoggedProcess::stateChanged, this, &HorizonPrepareStep::onPrepareState);
}

void HorizonPrepareStep::executeTask()
{
    auto* horizon = qobject_cast<HorizonInstance*>(m_parent->instance());
    if (horizon == nullptr) {
        emitFailed(tr("HorizonPrepareStep was given a non-Horizon instance."));
        return;
    }

    m_preparer = horizon->preparerBinary();
    if (m_preparer.isEmpty() || !QFileInfo(m_preparer).isFile()) {
        // A client-only install can still run prepared resources: do not make
        // the preparer mandatory when there is nothing to prepare.
        if (horizon->isPrepared()) {
            emit logLine(tr("horizon-prepare was not found; using the existing resources as-is."), MessageLevel::Warning);
            emitSucceeded();
            return;
        }
        emit logLine(tr("horizon-prepare was not found. Set \"preparerPath\" in horizon.json, "
                        "install a version through the Horizon launcher, or put horizon-prepare in PATH."),
                     MessageLevel::Fatal);
        emitFailed(tr("Cannot find horizon-prepare for this instance."));
        return;
    }

    // The preparer writes into the resources folder; its parent must exist.
    if (!FS::ensureFolderPathExists(QFileInfo(horizon->resourcesDir()).absolutePath())) {
        emitFailed(tr("Cannot create the Horizon folder in the instance."));
        return;
    }

    emit logLine(tr("Checking Horizon resources with %1").arg(m_preparer), MessageLevel::Launcher);
    const auto check = runCheck(horizon);
    if (check.upToDate) {
        if (!check.optionalGaps.isEmpty())
            emit logLine(tr("Horizon resources are usable; optional extras missing: %1.").arg(check.optionalGaps.join(", ")),
                         MessageLevel::Warning);
        else
            emit logLine(tr("Horizon resources are up to date."), MessageLevel::Launcher);
        emitSucceeded();
        return;
    }

    emit logLine(tr("Horizon resources need preparation: %1").arg(check.requiredGaps.join("; ")), MessageLevel::Launcher);
    startPrepare(horizon);
}

HorizonPrepareStep::CheckResult HorizonPrepareStep::runCheck(HorizonInstance* horizon)
{
    CheckResult result;
    if (!QFileInfo(horizon->resourcesDir()).isDir()) {
        result.requiredGaps << tr("the resources folder does not exist yet");
        return result;
    }

    QProcess check;
    check.setProcessEnvironment(m_parent->instance()->createEnvironment());
    check.setWorkingDirectory(QFileInfo(horizon->resourcesDir()).absolutePath());
    check.start(m_preparer, QStringList{ "--check", "--output", horizon->resourcesDir() });
    if (!check.waitForFinished(120000)) {
        check.kill();
        check.waitForFinished(5000);
        result.requiredGaps << tr("horizon-prepare --check timed out");
        return result;
    }

    const auto output = QString::fromUtf8(check.readAllStandardOutput()).trimmed();
    const auto errors = QString::fromUtf8(check.readAllStandardError()).trimmed();
    if (check.exitStatus() == QProcess::NormalExit && check.exitCode() == 0) {
        result.upToDate = true;
        return result;
    }

    // A gap line looks like "the sounds missing: horizon-prepare sounds".
    const auto text = output.isEmpty() ? errors : output;
    for (const auto& raw : text.split('\n')) {
        const auto line = raw.trimmed();
        if (line.isEmpty() || line.startsWith("error:"))
            continue;

        QString name;
        const auto missingColon = line.indexOf("missing:");
        if (missingColon >= 0) {
            const auto command = line.mid(missingColon + int(qstrlen("missing:"))).trimmed().split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            if (!command.isEmpty())
                name = command.last();
        } else if (line.contains("missing")) {
            name = line.section(' ', -1).remove(':');
        }

        if (OPTIONAL_GAPS.contains(name))
            result.optionalGaps << name;
        else
            result.requiredGaps << line;
    }
    result.optionalGaps.removeDuplicates();
    result.requiredGaps.removeDuplicates();

    if (result.requiredGaps.isEmpty() && result.optionalGaps.isEmpty())
        result.requiredGaps << tr("horizon-prepare --check failed with exit code %1").arg(check.exitCode());
    else if (result.requiredGaps.isEmpty())
        result.upToDate = true;

    return result;
}

void HorizonPrepareStep::startPrepare(HorizonInstance* horizon)
{
    // The preparer's own contract says which options and Java major it expects.
    int javaMajor = DEFAULT_JAVA_MAJOR;
    QStringList supported;
    {
        QProcess probe;
        probe.setProcessEnvironment(m_parent->instance()->createEnvironment());
        probe.start(m_preparer, QStringList{ "--launch-api" });
        if (probe.waitForFinished(5000) && probe.exitStatus() == QProcess::NormalExit && probe.exitCode() == 0) {
            const auto document = QJsonDocument::fromJson(probe.readAllStandardOutput());
            if (document.isObject()) {
                const auto api = document.object();
                if (api.value("java_major").isDouble())
                    javaMajor = api.value("java_major").toInt();
                for (const auto& value : api.value("options").toArray())
                    supported << value.toObject().value("name").toString();
            }
        }
    }
    const auto supports = [&supported](const char* option) { return supported.isEmpty() || supported.contains(option); };

    QStringList args{ "--prepare", "--output", horizon->resourcesDir() };

    // Let the preparer read Prism's own layout (version JSON, Maven libraries,
    // assets). It then uses the client jar as the server side, so no server jar
    // is needed at all.
    if (supports("--prism-dir")) {
        const auto prismDir = prismDataDirFor(horizon);
        if (!prismDir.isEmpty())
            args << "--prism-dir" << prismDir;
    }

    if (supports("--java-home")) {
        const auto javaHome = resolveJavaHome(javaMajor);
        if (!javaHome.isEmpty())
            args << "--java-home" << javaHome;
    }

    if (supports("--progress"))
        args << "--progress" << "json";

    emit logLine(tr("Preparing Horizon resources (the first run can take a few minutes)…"), MessageLevel::Launcher);
    emit logLine(tr("Running %1 %2").arg(m_preparer, args.join(' ')), MessageLevel::Launcher);
    setStatus(tr("Preparing Horizon resources…"));
    setProgress(0, 100);
    m_tail.clear();
    m_lastStatus.clear();
    m_process.setWorkingDirectory(QFileInfo(horizon->resourcesDir()).absolutePath());
    m_process.start(m_preparer, args);
}

QString HorizonPrepareStep::prismDataDirFor(HorizonInstance* horizon) const
{
    // A Prism root is better than an instance: it carries meta/ with the
    // rewritten 26.3 version JSON. `horizon-prepare` accepts either.
    const auto root = APPLICATION->dataRoot();
    if (QFileInfo(FS::PathCombine(root, "meta")).isDir())
        return root;
    return horizon->instanceRoot();
}

QString HorizonPrepareStep::resolveJavaHome(int requiredMajor) const
{
    // 1. Prism's configured Java, when it matches the pinned major.
    const auto configured = m_parent->instance()->settings()->get("JavaPath").toString();
    if (!configured.isEmpty()) {
        const auto home = javaHomeOf(configured);
        if (javaMajorOf(home) == requiredMajor)
            return home;
    }

    // 2. Any runtime Prism manages that matches (Prism ships Java 25 as
    //    java-runtime-epsilon for 26.x).
    QDir runtimes(FS::PathCombine(APPLICATION->dataRoot(), "java"));
    for (const auto& entry : runtimes.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        const auto home = runtimes.absoluteFilePath(entry);
        if (javaMajorOf(home) == requiredMajor)
            return home;
    }

#if defined(Q_OS_MACOS)
    // 3. A system JDK, the way the M0 spike found it.
    QProcess javaHome;
    javaHome.start("/usr/libexec/java_home", QStringList{ "-v", QString::number(requiredMajor) });
    if (javaHome.waitForFinished(10000) && javaHome.exitStatus() == QProcess::NormalExit && javaHome.exitCode() == 0) {
        const auto home = QString::fromUtf8(javaHome.readAllStandardOutput()).trimmed();
        if (!home.isEmpty() && QFileInfo(home).isDir())
            return home;
    }
#endif

    return {};
}

void HorizonPrepareStep::onPrepareLog(const QStringList& lines, MessageLevel level)
{
    for (const auto& line : lines) {
        if (level != MessageLevel::StdOut) {
            if (!line.trimmed().isEmpty()) {
                m_tail << line;
                if (m_tail.size() > 20)
                    m_tail.removeFirst();
            }
            emit logLine(line, level);
            continue;
        }

        // `--progress json` writes one event per line on stdout.
        const auto trimmed = line.trimmed();
        if (trimmed.startsWith('{')) {
            QJsonParseError error{};
            const auto document = QJsonDocument::fromJson(trimmed.toUtf8(), &error);
            if (error.error == QJsonParseError::NoError && document.isObject()) {
                const auto event = document.object();
                if (event.contains("error")) {
                    emit logLine(tr("horizon-prepare: %1").arg(event.value("error").toString()), MessageLevel::Error);
                    continue;
                }
                if (event.contains("fraction")) {
                    const auto fraction = qBound(0.0, event.value("fraction").toDouble(), 1.0);
                    setProgress(qRound(fraction * 100.0), 100);
                }
                const auto message = event.value("message").toString().trimmed();
                if (!message.isEmpty() && message != m_lastStatus) {
                    m_lastStatus = message;
                    setStatus(message);
                    emit logLine(message, MessageLevel::Launcher);
                }
                continue;
            }
        }
        emit logLine(line, level);
    }
}

void HorizonPrepareStep::onPrepareState(LoggedProcess::State state)
{
    switch (state) {
        case LoggedProcess::FailedToStart:
            emit logLine(tr("horizon-prepare failed to start."), MessageLevel::Fatal);
            emitFailed(tr("horizon-prepare failed to start."));
            return;
        case LoggedProcess::Crashed:
            emit logLine(tr("horizon-prepare crashed."), MessageLevel::Fatal);
            emitFailed(tr("horizon-prepare crashed."));
            return;
        case LoggedProcess::Aborted:
            emit logLine(tr("horizon-prepare was killed."), MessageLevel::Fatal);
            emitFailed(tr("horizon-prepare was killed."));
            return;
        case LoggedProcess::Finished:
            break;
        default:
            return;
    }

    auto* horizon = qobject_cast<HorizonInstance*>(m_parent->instance());
    if (horizon == nullptr) {
        emitFailed(tr("HorizonPrepareStep lost its instance."));
        return;
    }

    const auto exitCode = m_process.exitCode();
    if (exitCode != 0) {
        auto reason = tr("horizon-prepare failed with exit code %1.").arg(exitCode);
        if (!m_tail.isEmpty())
            reason += "\n\n" + m_tail.join('\n');
        emit logLine(reason, MessageLevel::Fatal);
        emitFailed(reason);
        return;
    }

    setStatus(tr("Checking the prepared resources…"));
    const auto check = runCheck(horizon);
    if (!check.upToDate) {
        const auto reason = tr("Preparation finished but the resources are still incomplete:\n%1").arg(check.requiredGaps.join('\n'));
        emit logLine(reason, MessageLevel::Fatal);
        emitFailed(reason);
        return;
    }

    if (!check.optionalGaps.isEmpty())
        emit logLine(tr("Horizon resources are ready; optional extras missing: %1.").arg(check.optionalGaps.join(", ")),
                     MessageLevel::Warning);
    else
        emit logLine(tr("Horizon resources are ready."), MessageLevel::Launcher);
    emitSucceeded();
}

bool HorizonPrepareStep::canAbort() const
{
    const auto state = m_process.state();
    return state == LoggedProcess::Starting || state == LoggedProcess::Running;
}

bool HorizonPrepareStep::abort()
{
    const auto state = m_process.state();
    if (state == LoggedProcess::Running || state == LoggedProcess::Starting)
        m_process.kill();
    return true;
}
