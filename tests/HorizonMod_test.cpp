// SPDX-License-Identifier: GPL-3.0-only
/*
 *  horizon-prism - Prism Launcher fork for Project Horizon
 *  Copyright (C) 2026 horizon-prism contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include "FileSystem.h"
#include "archive/ArchiveWriter.h"
#include "minecraft/mod/HorizonMod.h"

namespace {
constexpr auto CONTENT_TOML = R"(id = "coords_hud"
name = "Coords HUD"
version = "1.2.0"
api = "0.2"
side = "client"
capabilities = ["registries"]
)";

constexpr auto NATIVE_JSON = R"({
  "schema": 1,
  "id": "jade",
  "version": "26.3.8",
  "game": "26.3",
  "horizon": "6560641ba51a452f6bc2407baa4eb1b3be517ca8",
  "mixins": "5a64af938ec86b00d53d16a408aabc0038fb3e57",
  "hooks": 411
})";

bool writeArchive(const QString& path, const QList<QPair<QString, QByteArray>>& files)
{
    MMCZip::ArchiveWriter writer(path);
    if (!writer.open())
        return false;
    for (const auto& [name, data] : files) {
        if (!writer.addFile(name, data))
            return false;
    }
    return bool(writer.close());
}
}  // namespace

class HorizonModTest : public QObject {
    Q_OBJECT
   private slots:
    void parsesTheGameList()
    {
        const auto list = HorizonModList::parse(
            "mod.coords_hud:true\n"
            "mod.other:false\n"
            "key.zoom.zoom.in:key.mouse.4\n"
            "key.zoom.out:key.keyboard.x\n"
            "unrelated:true\n");

        QVERIFY(list.enabled().contains("coords_hud"));
        QVERIFY(list.disabled().contains("other"));
        QCOMPARE(list.keysOf("zoom").value("zoom.in"), QStringLiteral("key.mouse.4"));
        QCOMPARE(list.keysOf("zoom").value("out"), QStringLiteral("key.keyboard.x"));
        QVERIFY(list.keysOf("missing").isEmpty());

        // Serialization is deterministic: mods first, sorted, then keys.
        const auto text = list.serialize();
        QCOMPARE(text, QStringLiteral("mod.coords_hud:true\nmod.other:false\n"
                                      "key.zoom.out:key.keyboard.x\nkey.zoom.zoom.in:key.mouse.4\n"));
        QCOMPARE(HorizonModList::parse(text).serialize(), text);
    }

    void contentIsOffUntilTurnedOn()
    {
        HorizonMod content;
        content.id = "coords_hud";
        content.kind = HorizonMod::Kind::Content;

        HorizonModList list;
        QVERIFY(!list.isOn(content));
        list.setOn(content, true);
        QVERIFY(list.isOn(content));
        QCOMPARE(list.serialize(), QStringLiteral("mod.coords_hud:true\n"));
        list.setOn(content, false);
        QVERIFY(!list.isOn(content));
        QCOMPARE(list.serialize(), QString());
    }

    void nativeIsOnUntilTurnedOff()
    {
        HorizonMod native;
        native.id = "jade";
        native.kind = HorizonMod::Kind::Native;

        HorizonModList list;
        QVERIFY(list.isOn(native));
        list.setOn(native, false);
        QVERIFY(!list.isOn(native));
        QCOMPARE(list.serialize(), QStringLiteral("mod.jade:false\n"));
        list.setOn(native, true);
        QVERIFY(list.isOn(native));
        QCOMPARE(list.serialize(), QStringLiteral("mod.jade:true\n"));
    }

    void scansFilesAndFolders()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto gameRoot = dir.path();
        const auto contentDir = HorizonMods::contentDir(gameRoot);
        const auto nativeDir = HorizonMods::nativeDir(gameRoot);
        QVERIFY(FS::ensureFolderPathExists(contentDir));
        QVERIFY(FS::ensureFolderPathExists(nativeDir));

        QVERIFY(writeArchive(FS::PathCombine(contentDir, "coords_hud.hmod"),
                             { { "horizon-mod.toml", QByteArray(CONTENT_TOML) }, { "data/coords_hud/x.json", "{}" } }));
        QVERIFY(writeArchive(FS::PathCombine(nativeDir, "jade.hnmod"), { { "horizon-native.json", QByteArray(NATIVE_JSON) } }));
        QVERIFY(writeArchive(FS::PathCombine(contentDir, "broken.hmod"), { { "assets/x/y.png", "x" } }));

        // A folder mod, as a developer runs it with --dev-mod.
        const auto folder = FS::PathCombine(contentDir, "dev_mod");
        QVERIFY(FS::ensureFolderPathExists(folder));
        QFile toml(FS::PathCombine(folder, "horizon-mod.toml"));
        QVERIFY(toml.open(QIODevice::WriteOnly));
        toml.write(R"(id = "dev_mod"
name = "Dev mod"
version = "0.0.1"
api = "0.1"
)");
        toml.close();

        const auto mods = HorizonMods::scan(gameRoot);
        QCOMPARE(mods.size(), 4);

        auto byPath = [&mods](const QString& name) -> const HorizonMod* {
            for (const auto& mod : mods) {
                if (QFileInfo(mod.path).fileName() == name)
                    return &mod;
            }
            return nullptr;
        };

        const auto* content = byPath("coords_hud.hmod");
        QVERIFY(content);
        QCOMPARE(content->id, QStringLiteral("coords_hud"));
        QCOMPARE(content->name, QStringLiteral("Coords HUD"));
        QCOMPARE(content->version, QStringLiteral("1.2.0"));
        QCOMPARE(content->kind, HorizonMod::Kind::Content);
        QVERIFY(content->problem.isEmpty());
        QVERIFY(!content->folder);

        const auto* native = byPath("jade.hnmod");
        QVERIFY(native);
        QCOMPARE(native->id, QStringLiteral("jade"));
        QCOMPARE(native->kind, HorizonMod::Kind::Native);
        QCOMPARE(native->hooks, 411);
        QCOMPARE(native->game, QStringLiteral("26.3"));
        QVERIFY(native->problem.isEmpty());

        const auto* broken = byPath("broken.hmod");
        QVERIFY(broken);
        QVERIFY(!broken->problem.isEmpty());
        QVERIFY(broken->id.isEmpty());

        const auto* dev = byPath("dev_mod");
        QVERIFY(dev);
        QCOMPARE(dev->id, QStringLiteral("dev_mod"));
        QVERIFY(dev->folder);
        QVERIFY(dev->problem.isEmpty());
    }

    void combinedFileNeedsTheSameId()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto contentDir = HorizonMods::contentDir(dir.path());
        QVERIFY(FS::ensureFolderPathExists(contentDir));

        const QByteArray otherNative = R"({
          "schema": 1,
          "id": "other",
          "version": "1",
          "game": "26.3",
          "horizon": "6560641ba51a452f6bc2407baa4eb1b3be517ca8",
          "mixins": "5a64af938ec86b00d53d16a408aabc0038fb3e57"
        })";

        QVERIFY(writeArchive(FS::PathCombine(contentDir, "mixed.hmod"),
                             { { "horizon-mod.toml", QByteArray(CONTENT_TOML) }, { "horizon-native.json", otherNative } }));

        const auto mods = HorizonMods::scan(dir.path());
        QCOMPARE(mods.size(), 1);
        QCOMPARE(mods.first().kind, HorizonMod::Kind::Both);
        QVERIFY(mods.first().problem.contains(QLatin1String("different ids")));
    }

    void roundTripsTheListOnDisk()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        HorizonModList list;
        HorizonMod content;
        content.id = "coords_hud";
        content.kind = HorizonMod::Kind::Content;
        HorizonMod native;
        native.id = "jade";
        native.kind = HorizonMod::Kind::Native;

        list.setOn(content, true);
        list.setOn(native, false);
        QVERIFY(list.save(dir.path()));
        QVERIFY(QFileInfo::exists(HorizonMods::listPath(dir.path())));

        const auto reloaded = HorizonModList::load(dir.path());
        QVERIFY(reloaded.isOn(content));
        QVERIFY(!reloaded.isOn(native));
        QCOMPARE(reloaded.serialize(), list.serialize());
    }

    void doesNotWriteAnEmptyFile()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(HorizonModList().save(dir.path()));
        QVERIFY(!QFileInfo::exists(HorizonMods::listPath(dir.path())));
    }
};

QTEST_GUILESS_MAIN(HorizonModTest)

#include "HorizonMod_test.moc"
