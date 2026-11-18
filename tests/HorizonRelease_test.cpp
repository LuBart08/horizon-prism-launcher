// SPDX-License-Identifier: GPL-3.0-only
/*
 *  horizon-prism - Prism Launcher fork for Project Horizon
 *  Copyright (C) 2026 horizon-prism contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include <QTest>

#include "minecraft/HorizonRelease.h"

namespace {
// Trimmed from `horizon-launcher versions --json` on 2026-11-18.
constexpr auto VERSIONS = R"([
  {
    "tag": "nightly-2026-11-18-0b14f8a",
    "name": "Nightly 2026-11-18 · 0b14f8a",
    "date": "2026-11-18",
    "prerelease": false,
    "available_here": false,
    "installed": false
  },
  {
    "tag": "nightly-2026-11-13-9f42064",
    "name": "Nightly 2026-11-13 · 9f42064",
    "date": "2026-11-13",
    "prerelease": true,
    "available_here": true,
    "installed": true
  }
])";

constexpr auto INSTALLED = R"({
  "installed": [
    "nightly-2026-11-13-9f42064",
    "nightly-2026-11-08-fdc87ae"
  ],
  "selected": "nightly-2026-11-13-9f42064"
})";

constexpr auto NO_SELECTION = R"({ "installed": [], "selected": null })";

constexpr auto UPDATE_CHECK = R"({
  "horizon": null,
  "launcher": "0.2.78",
  "launcher_current": "0.2.68"
})";
}  // namespace

class HorizonReleaseTest : public QObject {
    Q_OBJECT
   private slots:
    void parsesTheReleaseList()
    {
        QString error;
        const auto releases = HorizonRelease::listFromJson(VERSIONS, &error);

        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(releases.size(), 2);
        QCOMPARE(releases.at(0).tag, QStringLiteral("nightly-2026-11-18-0b14f8a"));
        QCOMPARE(releases.at(0).name, QStringLiteral("Nightly 2026-11-18 · 0b14f8a"));
        QCOMPARE(releases.at(0).date, QStringLiteral("2026-11-18"));
    }

    void keepsThePerReleaseFlags()
    {
        const auto releases = HorizonRelease::listFromJson(VERSIONS);

        QCOMPARE(releases.size(), 2);
        QVERIFY(!releases.at(0).availableHere);
        QVERIFY(!releases.at(0).installed);
        QVERIFY(releases.at(1).prerelease);
        QVERIFY(releases.at(1).availableHere);
        QVERIFY(releases.at(1).installed);
    }

    void skipsEntriesWithoutATag()
    {
        const auto releases = HorizonRelease::listFromJson(R"([{"name": "no tag"}, {"tag": "v0.2.97"}])");

        QCOMPARE(releases.size(), 1);
        QCOMPARE(releases.at(0).tag, QStringLiteral("v0.2.97"));
    }

    void acceptsAnEmptyReleaseList()
    {
        QString error;
        QVERIFY(HorizonRelease::listFromJson("[]", &error).isEmpty());
        QVERIFY2(error.isEmpty(), qPrintable(error));
    }

    void rejectsAMalformedAnswer()
    {
        QString error;
        QVERIFY(HorizonRelease::listFromJson("{not json", &error).isEmpty());
        QVERIFY(!error.isEmpty());
    }

    void rejectsAnObjectWhereAListIsExpected()
    {
        QString error;
        QVERIFY(HorizonRelease::listFromJson(R"({"installed": []})", &error).isEmpty());
        QVERIFY(error.contains(QStringLiteral("list")));
    }

    void parsesTheInstalledVersions()
    {
        QString error;
        const auto versions = HorizonInstalledVersions::fromJson(INSTALLED, &error);

        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(versions.installed.size(), 2);
        QCOMPARE(versions.installed.at(0), QStringLiteral("nightly-2026-11-13-9f42064"));
        QCOMPARE(versions.selected, QStringLiteral("nightly-2026-11-13-9f42064"));
    }

    void treatsANullSelectionAsNone()
    {
        const auto versions = HorizonInstalledVersions::fromJson(NO_SELECTION);

        QVERIFY(versions.installed.isEmpty());
        QVERIFY(versions.selected.isEmpty());
    }

    void parsesTheUpdateCheck()
    {
        QString error;
        const auto updates = HorizonUpdates::fromJson(UPDATE_CHECK, &error);

        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(!updates.hasHorizonUpdate());
        QVERIFY(updates.hasLauncherUpdate());
        QCOMPARE(updates.launcherVersion, QStringLiteral("0.2.78"));
        QCOMPARE(updates.launcherCurrent, QStringLiteral("0.2.68"));
    }

    void reportsAHorizonUpdate()
    {
        const auto updates = HorizonUpdates::fromJson(
            R"({"horizon": "v0.3.0", "launcher": null, "launcher_current": "0.2.68"})");

        QVERIFY(updates.hasHorizonUpdate());
        QCOMPARE(updates.horizonTag, QStringLiteral("v0.3.0"));
        QVERIFY(!updates.hasLauncherUpdate());
    }

    void parsesTheInstallResult()
    {
        QString error;
        const auto result = HorizonInstallResult::fromJson(
            R"({"installed": "nightly-2026-11-18-0b14f8a", "prepared": false})", &error);

        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(result.installed, QStringLiteral("nightly-2026-11-18-0b14f8a"));
        QVERIFY(!result.prepared);
    }

    void rejectsAnInstallResultWithoutAVersion()
    {
        QString error;
        const auto result = HorizonInstallResult::fromJson(R"({"prepared": true})", &error);

        QVERIFY(result.installed.isEmpty());
        QVERIFY(!error.isEmpty());
    }
};

QTEST_GUILESS_MAIN(HorizonReleaseTest)

#include "HorizonRelease_test.moc"
