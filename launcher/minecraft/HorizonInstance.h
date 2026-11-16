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

#pragma once

#include <QJsonObject>

#include "minecraft/MinecraftInstance.h"

/*!
 * An instance that runs Project Horizon (Minecraft Java 26.3 rewritten in Rust)
 * instead of the Java client.
 *
 * Milestone M1: the instance loads and saves its own `horizon.json` model.
 * Milestone M3: launching runs `horizon-client` with the account session on
 * stdin. Preparation (M4) and settings UI (M2) are still to come.
 *
 * `horizon.json` is the source of truth for the Horizon side; `instance.cfg`
 * keeps the Prism-wide settings (name, icon, play time) and `InstanceType=Horizon`.
 */
class HorizonInstance : public MinecraftInstance {
    Q_OBJECT

   public:
    HorizonInstance(SettingsObject* globalSettings, std::unique_ptr<SettingsObject> settings, const QString& rootDir);
    ~HorizonInstance() override;

    void saveNow() override;
    void loadSpecificSettings() override;
    QSet<QString> traits() const override;
    QList<Task::Ptr> createUpdateTask() override;
    LaunchTask* createLaunchTask(AuthSessionPtr account, MinecraftTarget::Ptr targetToJoin) override;
    QString getStatusbarDescription() override;
    QString instanceConfigFolder() const override;
    QStringList getLogFileSearchPaths() override;
    QStringList verboseDescription(AuthSessionPtr session, MinecraftTarget::Ptr targetToJoin) override;
    bool canExport() const override { return false; }

    ////// Horizon model (horizon.json) //////
    QString horizonJsonPath() const;

    QString channel() const;
    QString clientVersion() const;
    QString preparerVersion() const;
    QString horizonMinecraftVersion() const;
    QString launchApiPin() const;
    int outputVersionPin() const;
    int nativeHooksPin() const;

    void setChannel(const QString& channel);
    void setClientVersion(const QString& version);
    void setPreparerVersion(const QString& version);

    ////// Horizon folders //////
    /// Prepared resources, passed to `horizon-client --resources`.
    QString resourcesDir() const;
    /// Logs and crash reports, passed to `horizon-client --home`.
    QString horizonHomeDir() const;
    /// Scratch folder of the preparer (`--root`), holds research-lock.json in a release.
    QString horizonRootDir() const;

    /// True when the resources folder looks prepared (M4 will use `--check`).
    bool isPrepared() const;

    /// `horizon-client` to run: `clientPath` from horizon.json, else the
    /// version pinned by `clientVersion`, else any installed version, else PATH.
    QString clientBinary() const;

   private:
    void loadHorizonJson();
    void saveHorizonJson();
    QString horizonLauncherDir() const;

    QJsonObject m_horizon;
};
