// SPDX-License-Identifier: GPL-3.0-only
/*
 *  horizon-prism - Prism Launcher fork for Project Horizon
 *  Copyright (C) 2026 horizon-prism contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include "HorizonLaunchStep.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QProcess>
#include <QUuid>

#include "FileSystem.h"
#include "launch/LaunchTask.h"
#include "minecraft/HorizonInstance.h"
#include "minecraft/auth/MinecraftAccount.h"

namespace {
QString describeExit(int code)
{
    switch (code) {
        case 0:
            return QObject::tr("the player quit");
        case 1:
            return QObject::tr("the game stopped on an error");
        case 2:
            return QObject::tr("the command line was refused");
        case 101:
            return QObject::tr("the game panicked");
        default:
            return QObject::tr("horizon-client exited with code %1").arg(code);
    }
}
}  // namespace

HorizonLaunchStep::HorizonLaunchStep(LaunchTask* parent, AuthSessionPtr session, MinecraftTarget::Ptr targetToJoin)
    : LaunchStep(parent), m_session(std::move(session)), m_targetToJoin(std::move(targetToJoin))
{
    m_process.setProcessEnvironment(m_parent->instance()->createEnvironment());
    connect(&m_process, &LoggedProcess::log, this, &HorizonLaunchStep::onLogLines);
    connect(&m_process, &LoggedProcess::stateChanged, this, &HorizonLaunchStep::onState);
    connect(&m_process, &QProcess::started, this, &HorizonLaunchStep::onStarted);
}

QByteArray HorizonLaunchStep::sessionPayload() const
{
    QJsonObject session;
    session["name"] = m_session->player_name;

    QString uuid = m_session->uuid;
    uuid.remove('-');
    if (uuid.isEmpty())
        uuid = MinecraftAccount::uuidFromUsername(m_session->player_name).toString(QUuid::Id128);
    session["uuid"] = uuid.toLower();

    session["access_token"] = m_session->access_token;

    // Horizon's session contract is the Microsoft one; offline test sessions
    // keep the same shape with a dummy token (as validated by the M0 spike).
    QString userType = m_session->user_type;
    if (userType.isEmpty())
        userType = QStringLiteral("msa");
    session["user_type"] = userType;

    return QJsonDocument(session).toJson(QJsonDocument::Compact);
}

void HorizonLaunchStep::probeLaunchApi(HorizonInstance* horizon, const QString& client)
{
    QProcess probe;
    probe.setProcessEnvironment(m_parent->instance()->createEnvironment());
    probe.start(client, { "--launch-api" });
    if (!probe.waitForFinished(5000) || probe.exitStatus() != QProcess::NormalExit || probe.exitCode() != 0) {
        emit logLine(tr("Could not read horizon-client --launch-api; assuming launch API %1.").arg(horizon->launchApiPin()),
                     MessageLevel::Error);
        return;
    }

    QJsonParseError error{};
    const auto document = QJsonDocument::fromJson(probe.readAllStandardOutput(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        emit logLine(tr("horizon-client --launch-api returned invalid JSON: %1").arg(error.errorString()), MessageLevel::Error);
        return;
    }

    const auto api = document.object();
    const auto version = api.value("launch_api").toString();
    if (!version.isEmpty() && version != horizon->launchApiPin()) {
        emit logLine(tr("horizon-client speaks launch API %1, but this instance is pinned to %2.")
                         .arg(version, horizon->launchApiPin()),
                     MessageLevel::Error);
    }

    // The crash report marker is part of the launch contract: do not copy it.
    const auto prefix = api.value("crash_report_line").toString();
    if (!prefix.isEmpty())
        m_crashReportPrefix = prefix;
}

void HorizonLaunchStep::executeTask()
{
    auto* horizon = qobject_cast<HorizonInstance*>(m_parent->instance());
    if (horizon == nullptr) {
        emitFailed(tr("HorizonLaunchStep was given a non-Horizon instance."));
        return;
    }
    if (!m_session) {
        emitFailed(tr("No account session was provided for this Horizon launch."));
        return;
    }

    const auto client = horizon->clientBinary();
    if (client.isEmpty() || !QFileInfo(client).isFile()) {
        emit logLine(tr("horizon-client was not found. Set \"clientPath\" in horizon.json, "
                        "install a version through the Horizon launcher, or put horizon-client in PATH."),
                     MessageLevel::Fatal);
        emitFailed(tr("Cannot find horizon-client for this instance."));
        return;
    }

    probeLaunchApi(horizon, client);

    // Horizon writes its logs and crash reports under --home.
    if (!FS::ensureFolderPathExists(horizon->horizonHomeDir()) || !FS::ensureFolderPathExists(horizon->gameRoot())) {
        emitFailed(tr("Cannot create the Horizon home or game folder."));
        return;
    }

    QStringList args{ "--resources", horizon->resourcesDir(), "--game-dir", horizon->gameRoot(), "--home", horizon->horizonHomeDir() };

    const auto settings = horizon->settings();
    if (horizon->fullscreen()) {
        args << "--fullscreen";
    } else {
        const auto width = settings->get("MinecraftWinWidth").toInt();
        const auto height = settings->get("MinecraftWinHeight").toInt();
        if (width > 0 && height > 0)
            args << "--width" << QString::number(width) << "--height" << QString::number(height);
    }

    if (m_targetToJoin) {
        if (!m_targetToJoin->world.isEmpty()) {
            args << "--quickPlaySingleplayer" << m_targetToJoin->world;
        } else if (!m_targetToJoin->address.isEmpty()) {
            auto address = m_targetToJoin->address;
            if (m_targetToJoin->port != 25565)
                address += QString(":%1").arg(m_targetToJoin->port);
            args << "--quickPlayMultiplayer" << address;
        }
    }

    // The session goes over stdin, never argv: tokens must not show up in the
    // process list or in the launch log.
    args << "--session-stdin";

    emit logLine(tr("Running %1 %2").arg(client, args.join(' ')), MessageLevel::Launcher);
    m_process.setWorkingDirectory(horizon->gameRoot());
    m_process.start(client, args);
}

void HorizonLaunchStep::onStarted()
{
    m_parent->setPid(m_process.processId());
    m_process.write(sessionPayload());
    m_process.write("\n");
    m_process.closeWriteChannel();
}

void HorizonLaunchStep::onLogLines(const QStringList& lines, MessageLevel level)
{
    for (const auto& line : lines) {
        if (!m_crashReportPrefix.isEmpty() && line.startsWith(m_crashReportPrefix))
            m_crashReport = line.mid(m_crashReportPrefix.size()).trimmed();
    }
    emit logLines(lines, level);
}

void HorizonLaunchStep::onState(LoggedProcess::State state)
{
    switch (state) {
        case LoggedProcess::FailedToStart:
            emit logLine(tr("horizon-client failed to start."), MessageLevel::Fatal);
            emitFailed(tr("horizon-client failed to start."));
            return;
        case LoggedProcess::Crashed:
            emit logLine(tr("horizon-client crashed."), MessageLevel::Fatal);
            emitFailed(tr("horizon-client crashed."));
            return;
        case LoggedProcess::Aborted:
            emit logLine(tr("horizon-client was killed."), MessageLevel::Fatal);
            emitFailed(tr("horizon-client was killed."));
            return;
        case LoggedProcess::Finished:
            break;
        default:
            return;
    }

    const auto code = m_process.exitCode();
    if (code == 0) {
        emit logLine(tr("horizon-client exited: %1.").arg(describeExit(code)), MessageLevel::Launcher);
        emitSucceeded();
        return;
    }

    auto reason = tr("horizon-client exited: %1.").arg(describeExit(code));
    if (!m_crashReport.isEmpty())
        reason += tr("\n\nCrash report: %1").arg(m_crashReport);
    emit logLine(reason, MessageLevel::Fatal);
    emitFailed(reason);
}

bool HorizonLaunchStep::abort()
{
    const auto state = m_process.state();
    if (state == LoggedProcess::Running || state == LoggedProcess::Starting)
        m_process.kill();
    return true;
}
