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

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

/*!
 * The version model of the official Horizon launcher (milestone M7).
 *
 * Everything here parses the JSON the `horizon-launcher` CLI prints with
 * `--json`; no network and no file access, so it is unit-testable:
 *
 *   horizon-launcher versions --json
 *   horizon-launcher versions --installed --json
 *   horizon-launcher update --check --json
 *   horizon-launcher install <tag> --json
 */

/// One entry of `horizon-launcher versions --json`.
struct HorizonRelease {
    QString tag;
    QString name;
    QString date;
    bool prerelease = false;
    /// A build exists for this platform, so it can be installed here.
    bool availableHere = false;
    bool installed = false;

    /// `nullptr` on success; otherwise a message for the user.
    static QList<HorizonRelease> listFromJson(const QByteArray& json, QString* error = nullptr);
};

/// `horizon-launcher versions --installed --json`.
struct HorizonInstalledVersions {
    QStringList installed;
    /// The version the official launcher plays by default; empty when none is set.
    QString selected;

    static HorizonInstalledVersions fromJson(const QByteArray& json, QString* error = nullptr);
};

/// `horizon-launcher update --check --json`.
struct HorizonUpdates {
    /// Tag of the newer Horizon release; empty when Horizon is up to date.
    QString horizonTag;
    /// Version of the newer launcher; empty when the launcher is up to date.
    QString launcherVersion;
    /// Version of the launcher that answered.
    QString launcherCurrent;

    bool hasHorizonUpdate() const { return !horizonTag.isEmpty(); }
    bool hasLauncherUpdate() const { return !launcherVersion.isEmpty(); }

    static HorizonUpdates fromJson(const QByteArray& json, QString* error = nullptr);
};

/// `horizon-launcher install <tag> --json`.
struct HorizonInstallResult {
    QString installed;
    /// The installed version already had prepared resources, so no prepare is needed.
    bool prepared = false;

    static HorizonInstallResult fromJson(const QByteArray& json, QString* error = nullptr);
};
