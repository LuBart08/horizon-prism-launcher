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

#include "HorizonRelease.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>

namespace {
/// Parses `json` and reports why when it is not what the CLI promised.
QJsonDocument parseDocument(const QByteArray& json, const QString& expected, QString* error)
{
    QJsonParseError parseError{};
    const auto document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        if (error != nullptr)
            *error = QStringLiteral("horizon-launcher %1: %2").arg(expected, parseError.errorString());
        return {};
    }
    return document;
}

void fail(const QString& expected, const QString& why, QString* error)
{
    if (error != nullptr)
        *error = QStringLiteral("horizon-launcher %1: %2").arg(expected, why);
}

QString stringOrEmpty(const QJsonValue& value)
{
    return value.isString() ? value.toString() : QString();
}
}  // namespace

QList<HorizonRelease> HorizonRelease::listFromJson(const QByteArray& json, QString* error)
{
    if (error != nullptr)
        error->clear();

    const auto document = parseDocument(json, QStringLiteral("versions --json"), error);
    if (document.isNull())
        return {};
    if (!document.isArray()) {
        fail(QStringLiteral("versions --json"), QStringLiteral("expected a list of releases"), error);
        return {};
    }

    QList<HorizonRelease> releases;
    const auto array = document.array();
    releases.reserve(array.size());
    for (const auto& value : array) {
        if (!value.isObject())
            continue;
        const auto object = value.toObject();
        const auto tag = object.value("tag").toString();
        if (tag.isEmpty())
            continue;

        HorizonRelease release;
        release.tag = tag;
        release.name = object.value("name").toString();
        release.date = object.value("date").toString();
        release.prerelease = object.value("prerelease").toBool();
        release.availableHere = object.value("available_here").toBool();
        release.installed = object.value("installed").toBool();
        releases << release;
    }
    return releases;
}

HorizonInstalledVersions HorizonInstalledVersions::fromJson(const QByteArray& json, QString* error)
{
    if (error != nullptr)
        error->clear();

    const auto document = parseDocument(json, QStringLiteral("versions --installed --json"), error);
    if (document.isNull())
        return {};
    if (!document.isObject()) {
        fail(QStringLiteral("versions --installed --json"), QStringLiteral("expected an object"), error);
        return {};
    }

    HorizonInstalledVersions versions;
    const auto object = document.object();
    for (const auto& value : object.value("installed").toArray()) {
        const auto tag = value.toString();
        if (!tag.isEmpty())
            versions.installed << tag;
    }
    // `selected` is null when nothing is selected.
    versions.selected = stringOrEmpty(object.value("selected"));
    return versions;
}

HorizonUpdates HorizonUpdates::fromJson(const QByteArray& json, QString* error)
{
    if (error != nullptr)
        error->clear();

    const auto document = parseDocument(json, QStringLiteral("update --check --json"), error);
    if (document.isNull())
        return {};
    if (!document.isObject()) {
        fail(QStringLiteral("update --check --json"), QStringLiteral("expected an object"), error);
        return {};
    }

    HorizonUpdates updates;
    const auto object = document.object();
    // `horizon` is a tag or null; `launcher` is a version or null.
    updates.horizonTag = stringOrEmpty(object.value("horizon"));
    updates.launcherVersion = stringOrEmpty(object.value("launcher"));
    updates.launcherCurrent = stringOrEmpty(object.value("launcher_current"));
    return updates;
}

HorizonInstallResult HorizonInstallResult::fromJson(const QByteArray& json, QString* error)
{
    if (error != nullptr)
        error->clear();

    const auto document = parseDocument(json, QStringLiteral("install --json"), error);
    if (document.isNull())
        return {};
    if (!document.isObject()) {
        fail(QStringLiteral("install --json"), QStringLiteral("expected an object"), error);
        return {};
    }

    HorizonInstallResult result;
    const auto object = document.object();
    result.installed = object.value("installed").toString();
    result.prepared = object.value("prepared").toBool();
    if (result.installed.isEmpty())
        fail(QStringLiteral("install --json"), QStringLiteral("no installed version in the answer"), error);
    return result;
}
