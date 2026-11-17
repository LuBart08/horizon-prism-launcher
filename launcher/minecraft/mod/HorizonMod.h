// SPDX-License-Identifier: GPL-3.0-only
/*
 *  horizon-prism - Prism Launcher fork for Project Horizon
 *  Copyright (C) 2026 horizon-prism contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#pragma once

#include <QList>
#include <QMap>
#include <QSet>
#include <QString>

/*!
 * A Horizon mod found in the game folder (milestone M5).
 *
 * Horizon has two kinds of mods, both documented in `docs/mods.md`:
 * content mods (`.hmod`, data and assets) and native mods (`.hnmod`, Rust
 * sources the launcher builds into the game). A `.hmod` may carry both.
 *
 * This is deliberately not Prism's Java `Mod`: the manifest, the folders and
 * the enable/disable rules are Horizon's.
 */
struct HorizonMod {
    enum class Kind {
        Content,  ///< `horizon-mod.toml` only
        Native,   ///< `horizon-native.json` only (`.hnmod`, or a code-only `.hmod`)
        Both,     ///< one `.hmod` with both manifests
    };

    QString id;
    QString name;
    QString version;
    QString description;
    QString api;    ///< content API line: "0.1", "0.2" (content) or "0.3" (server)
    QString side;   ///< content side: "client", "server" or "both"
    Kind kind = Kind::Content;
    bool folder = false;  ///< a folder with a manifest, run with `--dev-mod`
    QString path;         ///< the file or the folder
    QString problem;      ///< why it cannot run; empty when it looks fine

    // Native manifest facts (`horizon-native.json`).
    QString game;
    QString horizonCommit;
    QString mixinsCommit;
    int hooks = 0;  ///< 0 when the manifest has no `hooks` field

    bool isContent() const { return kind != Kind::Native; }
    bool isNative() const { return kind != Kind::Content; }

    /// The name to show: the manifest's, else the file name.
    QString title() const;
};

/*!
 * `horizon-mods.txt`, beside `options.txt`: which installed mods are on.
 *
 * The game's own rules (`horizon-client-render/src/mod_list.rs`):
 * - a content mod is off until a `mod.<id>:true` line exists;
 * - a native mod is on until a `mod.<id>:false` line exists;
 * - `key.<id>.<name>:<key>` lines hold the player's key bindings and must be
 *   preserved verbatim.
 */
class HorizonModList {
   public:
    static HorizonModList load(const QString& gameRoot);
    /// Writes the file, unless the list is empty and no file exists yet.
    bool save(const QString& gameRoot) const;

    static HorizonModList parse(const QString& text);
    QString serialize() const;

    bool isOn(const HorizonMod& mod) const;
    void setOn(const HorizonMod& mod, bool on);

    QMap<QString, QString> keysOf(const QString& id) const { return m_keys.value(id); }

    QSet<QString> enabled() const { return m_enabled; }
    QSet<QString> disabled() const { return m_disabled; }

   private:
    QSet<QString> m_enabled;
    QSet<QString> m_disabled;
    QMap<QString, QMap<QString, QString>> m_keys;
};

namespace HorizonMods {
/// `<gameRoot>/horizon-mods.txt`
QString listPath(const QString& gameRoot);
/// `<gameRoot>/horizon-mods`, the content mods' folder.
QString contentDir(const QString& gameRoot);
/// `<gameRoot>/horizon-native-mods`, where the launcher installs native mods.
QString nativeDir(const QString& gameRoot);

/// Scans both folders. Files that cannot be read come back with `problem`
/// set instead of being dropped, so the page can show them.
QList<HorizonMod> scan(const QString& gameRoot);
}  // namespace HorizonMods
