// SPDX-License-Identifier: GPL-3.0-only
/*
 *  horizon-prism - Prism Launcher fork for Project Horizon
 *  Copyright (C) 2026 horizon-prism contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "HorizonInstance.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QStandardPaths>

#include "FileSystem.h"
#include "launch/LaunchTask.h"
#include "minecraft/launch/HorizonLaunchStep.h"
#include "minecraft/launch/HorizonNotPrepared.h"
#include "minecraft/launch/HorizonPrepareStep.h"

namespace {
constexpr auto HORIZON_MINECRAFT_VERSION = "26.3";
constexpr auto HORIZON_DEFAULT_CHANNEL = "stable";
constexpr auto HORIZON_DEFAULT_LAUNCH_API = "0.1.0";
constexpr int HORIZON_DEFAULT_OUTPUT_VERSION = 4;
constexpr int HORIZON_DEFAULT_NATIVE_HOOKS = 415;
}  // namespace

HorizonInstance::HorizonInstance(SettingsObject* globalSettings,
                                 std::unique_ptr<SettingsObject> settings,
                                 const QString& rootDir)
    : MinecraftInstance(globalSettings, std::move(settings), rootDir)
{
    // horizon.json is independent from instance.cfg, so it can be read before
    // the instance-specific settings are loaded.
    loadHorizonJson();
}

HorizonInstance::~HorizonInstance() = default;

QString HorizonInstance::horizonJsonPath() const
{
    return FS::PathCombine(instanceRoot(), "horizon.json");
}

void HorizonInstance::loadHorizonJson()
{
    auto result = FS::read(horizonJsonPath());
    if (!result) {
        // A brand new instance has no horizon.json yet: defaults are used and
        // the file is written on the next save.
        m_horizon = QJsonObject();
        return;
    }

    QJsonParseError error{};
    const auto document = QJsonDocument::fromJson(result.value(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        qWarning() << "horizon.json is not a JSON object:" << error.errorString();
        m_horizon = QJsonObject();
        return;
    }
    m_horizon = document.object();
}

void HorizonInstance::saveHorizonJson()
{
    if (!m_horizon.contains("formatVersion"))
        m_horizon["formatVersion"] = 1;
    if (!m_horizon.contains("channel"))
        m_horizon["channel"] = QString(HORIZON_DEFAULT_CHANNEL);
    if (!m_horizon.contains("minecraft"))
        m_horizon["minecraft"] = QString(HORIZON_MINECRAFT_VERSION);

    const auto data = QJsonDocument(m_horizon).toJson(QJsonDocument::Indented);
    if (auto result = FS::write(horizonJsonPath(), data); !result) {
        qWarning() << "Cannot write" << horizonJsonPath() << ":" << result.error();
    }
}

void HorizonInstance::loadSpecificSettings()
{
    if (isSpecificSettingsLoaded())
        return;

    MinecraftInstance::loadSpecificSettings();

    // MinecraftInstance::loadSpecificSettings() rewrites InstanceType to
    // "OneSix". Keep our own type so the instance survives a restart.
    if (m_settings->get("InstanceType").toString() != "Horizon")
        m_settings->set("InstanceType", "Horizon");

    loadHorizonJson();
}

void HorizonInstance::saveNow()
{
    MinecraftInstance::saveNow();
    saveHorizonJson();
}

QSet<QString> HorizonInstance::traits() const
{
    // No PackProfile: there is no Java version to inspect.
    return { "Horizon" };
}

QList<Task::Ptr> HorizonInstance::createUpdateTask()
{
    // Nothing to download or update on the Java side. M4 prepares resources
    // through the Horizon sidecar instead.
    return {};
}

LaunchTask* HorizonInstance::createLaunchTask(AuthSessionPtr account, MinecraftTarget::Ptr targetToJoin)
{
    auto process = LaunchTask::create(this);
    auto* task = process.get();

    // Prepare or repair the resources first, then make sure they are usable,
    // then run the real client.
    task->appendStep(makeShared<HorizonPrepareStep>(task));
    task->appendStep(makeShared<HorizonNotPrepared>(task));
    task->appendStep(makeShared<HorizonLaunchStep>(task, account, targetToJoin));
    return process.release();
}

QString HorizonInstance::channel() const
{
    return m_horizon.value("channel").toString(HORIZON_DEFAULT_CHANNEL);
}

QString HorizonInstance::clientVersion() const
{
    return m_horizon.value("clientVersion").toString();
}

QString HorizonInstance::preparerVersion() const
{
    return m_horizon.value("preparerVersion").toString();
}

QString HorizonInstance::horizonMinecraftVersion() const
{
    return m_horizon.value("minecraft").toString(HORIZON_MINECRAFT_VERSION);
}

QString HorizonInstance::launchApiPin() const
{
    return m_horizon.value("pins").toObject().value("launch_api").toString(HORIZON_DEFAULT_LAUNCH_API);
}

int HorizonInstance::outputVersionPin() const
{
    return m_horizon.value("pins").toObject().value("output_version").toInt(HORIZON_DEFAULT_OUTPUT_VERSION);
}

int HorizonInstance::nativeHooksPin() const
{
    return m_horizon.value("pins").toObject().value("native_hooks_api").toInt(HORIZON_DEFAULT_NATIVE_HOOKS);
}

bool HorizonInstance::fullscreen() const
{
    return m_horizon.value("fullscreen").toBool(false);
}

void HorizonInstance::setChannel(const QString& channel)
{
    m_horizon["channel"] = channel;
    saveHorizonJson();
    emit propertiesChanged();
}

void HorizonInstance::setClientVersion(const QString& version)
{
    m_horizon["clientVersion"] = version;
    saveHorizonJson();
    emit propertiesChanged();
}

void HorizonInstance::setPreparerVersion(const QString& version)
{
    m_horizon["preparerVersion"] = version;
    saveHorizonJson();
    emit propertiesChanged();
}

void HorizonInstance::setFullscreen(bool fullscreen)
{
    m_horizon["fullscreen"] = fullscreen;
    saveHorizonJson();
    emit propertiesChanged();
}

QStringList HorizonInstance::installedVersions() const
{
    QDir versions(FS::PathCombine(horizonLauncherDir(), "versions"));
    return versions.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
}

QString HorizonInstance::resourcesDir() const
{
    return FS::PathCombine(instanceRoot(), "horizon", "resources");
}

QString HorizonInstance::horizonHomeDir() const
{
    return FS::PathCombine(instanceRoot(), "horizon", "home");
}

QString HorizonInstance::horizonRootDir() const
{
    return FS::PathCombine(instanceRoot(), "horizon", "root");
}

bool HorizonInstance::isPrepared() const
{
    // Cheap check for M1. M4 asks `horizon-tools check`, which also compares
    // the preparer's output_version.
    return QFileInfo(FS::PathCombine(resourcesDir(), "assets")).isDir();
}

QString HorizonInstance::horizonLauncherDir() const
{
    // Where the official Horizon launcher stores its installed versions.
    const QByteArray appData = qgetenv("APPDATA");
#if defined(Q_OS_MACOS)
    return FS::PathCombine(QDir::homePath(), "Library", "Application Support", "horizon-launcher");
#elif defined(Q_OS_WIN)
    return FS::PathCombine(QString::fromLocal8Bit(appData), "horizon-launcher");
#else
    const QByteArray xdgData = qgetenv("XDG_DATA_HOME");
    const QString dataHome = xdgData.isEmpty() ? FS::PathCombine(QDir::homePath(), ".local", "share") : QString::fromLocal8Bit(xdgData);
    return FS::PathCombine(dataHome, "horizon-launcher");
#endif
}

QString HorizonInstance::horizonBinary(const QString& explicitKey, const QString& versionKey, const char* binaryName) const
{
    const QString name = QString::fromLatin1(binaryName);
#ifdef Q_OS_WIN
    const auto executableName = name + ".exe";
#else
    const auto executableName = name;
#endif

    // Explicit override in horizon.json, absolute or relative to the instance.
    const auto configured = m_horizon.value(explicitKey).toString();
    if (!configured.isEmpty()) {
        const QFileInfo info(configured);
        return info.isAbsolute() ? configured : FS::PathCombine(instanceRoot(), configured);
    }

    const auto versionsDir = FS::PathCombine(horizonLauncherDir(), "versions");
    const auto pinned = m_horizon.value(versionKey).toString();
    if (!pinned.isEmpty()) {
        const auto candidate = FS::PathCombine(versionsDir, pinned, executableName);
        if (QFileInfo(candidate).isFile())
            return candidate;
    }

    // Fall back to any installed version (newest name last in QDir's order).
    QDir versions(versionsDir);
    const auto entries = versions.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (auto it = entries.crbegin(); it != entries.crend(); ++it) {
        const auto candidate = FS::PathCombine(versionsDir, *it, executableName);
        if (QFileInfo(candidate).isFile())
            return candidate;
    }

    return QStandardPaths::findExecutable(executableName);
}

QString HorizonInstance::clientBinary() const
{
    return horizonBinary(QStringLiteral("clientPath"), QStringLiteral("clientVersion"), "horizon-client");
}

QString HorizonInstance::preparerBinary() const
{
    return horizonBinary(QStringLiteral("preparerPath"), QStringLiteral("preparerVersion"), "horizon-prepare");
}

QString HorizonInstance::instanceConfigFolder() const
{
    return FS::PathCombine(instanceRoot(), "horizon", "config");
}

QStringList HorizonInstance::getLogFileSearchPaths()
{
    return { FS::PathCombine(horizonHomeDir(), "logs"), horizonHomeDir(), gameRoot() };
}

QString HorizonInstance::getStatusbarDescription()
{
    QStringList parts{ "Horizon" };
    if (!clientVersion().isEmpty())
        parts << clientVersion();
    parts << QString("(%1)").arg(channel());
    if (!isPrepared())
        parts << tr("not prepared");
    return parts.join(' ');
}

QStringList HorizonInstance::verboseDescription(AuthSessionPtr session, MinecraftTarget::Ptr targetToJoin)
{
    Q_UNUSED(session);
    Q_UNUSED(targetToJoin);

    QStringList out;
    out << tr("Horizon instance: %1").arg(name());
    out << tr("Minecraft %1, channel %2").arg(horizonMinecraftVersion(), channel());
    out << tr("Client: %1").arg(clientVersion().isEmpty() ? tr("not installed") : clientVersion());
    out << tr("Preparer: %1").arg(preparerVersion().isEmpty() ? tr("not installed") : preparerVersion());
    out << tr("Launch API %1, output version %2, native hooks %3")
               .arg(launchApiPin())
               .arg(outputVersionPin())
               .arg(nativeHooksPin());
    out << tr("Resources: %1 (%2)").arg(resourcesDir(), isPrepared() ? tr("prepared") : tr("not prepared"));
    out << tr("Game folder: %1").arg(gameRoot());
    return out;
}
