// SPDX-License-Identifier: GPL-3.0-only
/*
 *  horizon-prism - Prism Launcher fork for Project Horizon
 *  Copyright (C) 2026 horizon-prism contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include "HorizonMod.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStringList>

#include <toml++/toml.h>

#include "FileSystem.h"
#include "archive/ArchiveReader.h"

namespace {
constexpr auto CONTENT_MANIFEST = "horizon-mod.toml";
constexpr auto NATIVE_MANIFEST = "horizon-native.json";

bool validId(const QString& id)
{
    static const QRegularExpression re(QStringLiteral("^[a-z0-9_]{1,64}$"));
    return re.match(id).hasMatch();
}

bool validRevision(const QString& revision)
{
    static const QRegularExpression re(QStringLiteral("^[0-9a-f]{40}$"));
    return re.match(revision).hasMatch();
}

/// Reads a manifest from a zip; returns an empty array when it is absent.
QByteArray readFromArchive(const QString& archive, const QString& entry)
{
    MMCZip::ArchiveReader reader(archive);
    auto result = reader.readFile(entry);
    if (!result) {
        // Absent and unreadable both come back as an error; callers treat an
        // absent manifest as "not this kind" and report the file separately.
        return {};
    }
    return *result;
}

QString readFile(const QString& path, bool* ok)
{
    auto result = FS::read(path);
    *ok = bool(result);
    return result ? QString::fromUtf8(*result) : QString();
}

/// Fills the content fields of `mod` from `horizon-mod.toml`.
/// Returns an error message, or an empty string on success.
QString applyContentManifest(HorizonMod& mod, const QByteArray& data)
{
    toml::table table;
#if TOML_EXCEPTIONS
    try {
        table = toml::parse(std::string_view(data.constData(), size_t(data.size())));
    } catch (const std::exception& err) {
        return QStringLiteral("horizon-mod.toml: %1").arg(QString::fromUtf8(err.what()));
    }
#else
    toml::parse_result result = toml::parse(std::string_view(data.constData(), size_t(data.size())));
    if (!result) {
        return QStringLiteral("horizon-mod.toml: %1").arg(QString::fromStdString(result.error().description()));
    }
    table = std::move(result.table());
#endif

    auto str = [&table](const char* key) -> QString {
        if (auto value = table[key].value<std::string>())
            return QString::fromStdString(*value);
        return {};
    };

    const auto id = str("id");
    if (!validId(id))
        return QStringLiteral("horizon-mod.toml: invalid id \"%1\" (expected [a-z0-9_]{1,64})").arg(id);

    const auto version = str("version");
    if (version.isEmpty())
        return QStringLiteral("horizon-mod.toml: missing version");

    const auto api = str("api");
    if (api.isEmpty())
        return QStringLiteral("horizon-mod.toml: missing api");

    mod.id = id;
    mod.name = str("name");
    mod.version = version;
    mod.description = str("description");
    mod.api = api;

    const auto side = str("side");
    mod.side = side.isEmpty() ? QStringLiteral("client") : side;
    if (mod.side != QLatin1String("client") && mod.side != QLatin1String("server") && mod.side != QLatin1String("both"))
        return QStringLiteral("horizon-mod.toml: unknown side \"%1\"").arg(mod.side);

    // A content mod declares the `registries` capability; a code mod doesn't.
    bool registries = false;
    if (auto caps = table["capabilities"].as_array()) {
        for (const auto& cap : *caps) {
            if (auto value = cap.value<std::string>(); value && *value == "registries")
                registries = true;
        }
    }
    if (api == QLatin1String("0.2") && !registries)
        return QStringLiteral("horizon-mod.toml: api 0.2 without the registries capability");

    return {};
}

/// Fills the native fields of `mod` from `horizon-native.json`.
QString applyNativeManifest(HorizonMod& mod, const QByteArray& data)
{
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(data, &parseError);
    if (document.isNull())
        return QStringLiteral("horizon-native.json: %1").arg(parseError.errorString());
    if (!document.isObject())
        return QStringLiteral("horizon-native.json: not an object");

    const auto object = document.object();

    const auto id = object.value("id").toString();
    if (!validId(id))
        return QStringLiteral("horizon-native.json: invalid id \"%1\"").arg(id);

    const auto version = object.value("version").toString();
    if (version.isEmpty())
        return QStringLiteral("horizon-native.json: missing version");

    const auto game = object.value("game").toString();
    if (game.isEmpty())
        return QStringLiteral("horizon-native.json: missing game");

    const auto horizon = object.value("horizon").toString();
    if (!validRevision(horizon))
        return QStringLiteral("horizon-native.json: horizon is not a 40-character commit");

    const auto mixins = object.value("mixins").toString();
    if (!validRevision(mixins))
        return QStringLiteral("horizon-native.json: mixins is not a 40-character commit");

    if (object.value("schema").toInt() != 1)
        return QStringLiteral("horizon-native.json: schema must be 1");

    const auto hooksValue = object.value("hooks");
    if (!hooksValue.isUndefined() && (!hooksValue.isDouble() || hooksValue.toInt() <= 0))
        return QStringLiteral("horizon-native.json: hooks must be a positive number");

    mod.id = id;
    mod.version = version;
    mod.game = game;
    mod.horizonCommit = horizon;
    mod.mixinsCommit = mixins;
    mod.hooks = hooksValue.isUndefined() ? 0 : hooksValue.toInt();
    if (mod.name.isEmpty())
        mod.name = object.value("name").toString();

    return {};
}

/// Reads both manifests of an archive into `mod`, setting its kind.
void readArchive(HorizonMod& mod)
{
    const auto content = readFromArchive(mod.path, CONTENT_MANIFEST);
    const auto native = readFromArchive(mod.path, NATIVE_MANIFEST);

    if (content.isEmpty() && native.isEmpty()) {
        mod.problem = QStringLiteral("no horizon-mod.toml or horizon-native.json");
        return;
    }

    mod.kind = !native.isEmpty() ? (!content.isEmpty() ? HorizonMod::Kind::Both : HorizonMod::Kind::Native) : HorizonMod::Kind::Content;

    if (!content.isEmpty()) {
        auto problem = applyContentManifest(mod, content);
        if (!problem.isEmpty())
            mod.problem = problem;
    }
    if (!native.isEmpty()) {
        const auto contentId = mod.id;
        auto problem = applyNativeManifest(mod, native);
        if (!problem.isEmpty())
            mod.problem = problem;
        else if (!contentId.isEmpty() && contentId != mod.id)
            mod.problem = QStringLiteral("horizon-mod.toml and horizon-native.json name different ids");
    }
}

/// Reads both manifests of a folder into `mod`, setting its kind.
void readFolder(HorizonMod& mod)
{
    const auto contentPath = FS::PathCombine(mod.path, CONTENT_MANIFEST);
    const auto nativePath = FS::PathCombine(mod.path, NATIVE_MANIFEST);

    const bool hasContent = QFileInfo::exists(contentPath);
    const bool hasNative = QFileInfo::exists(nativePath);
    if (!hasContent && !hasNative) {
        mod.problem = QStringLiteral("no manifest");
        return;
    }

    mod.kind = hasNative && hasContent ? HorizonMod::Kind::Both : (hasNative ? HorizonMod::Kind::Native : HorizonMod::Kind::Content);

    QString contentId;
    if (hasContent) {
        bool ok = false;
        const auto text = readFile(contentPath, &ok);
        if (!ok)
            mod.problem = QStringLiteral("cannot read horizon-mod.toml");
        else if (auto problem = applyContentManifest(mod, text.toUtf8()); !problem.isEmpty())
            mod.problem = problem;
        contentId = mod.id;
    }
    if (hasNative) {
        bool ok = false;
        const auto text = readFile(nativePath, &ok);
        if (!ok)
            mod.problem = QStringLiteral("cannot read horizon-native.json");
        else if (auto problem = applyNativeManifest(mod, text.toUtf8()); !problem.isEmpty())
            mod.problem = problem;
        else if (!contentId.isEmpty() && contentId != mod.id)
            mod.problem = QStringLiteral("horizon-mod.toml and horizon-native.json name different ids");
    }
}
}  // namespace

QString HorizonMod::title() const
{
    if (!name.isEmpty())
        return name;
    return QFileInfo(path).fileName();
}

QString HorizonMods::listPath(const QString& gameRoot)
{
    return FS::PathCombine(gameRoot, "horizon-mods.txt");
}

QString HorizonMods::contentDir(const QString& gameRoot)
{
    return FS::PathCombine(gameRoot, "horizon-mods");
}

QString HorizonMods::nativeDir(const QString& gameRoot)
{
    return FS::PathCombine(gameRoot, "horizon-native-mods");
}

QList<HorizonMod> HorizonMods::scan(const QString& gameRoot)
{
    QList<HorizonMod> mods;

    auto scanDir = [&mods](const QString& dirPath, bool nativeFolder) {
        QDir dir(dirPath);
        if (!dir.exists())
            return;

        const auto entries = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const auto& info : entries) {
            if (info.isDir()) {
                HorizonMod mod;
                mod.folder = true;
                mod.path = info.filePath();
                readFolder(mod);
                if (mod.kind == HorizonMod::Kind::Content && nativeFolder)
                    continue;  // not a native mod
                mods.append(mod);
                continue;
            }

            const auto suffix = info.suffix().toLower();
            if (suffix == QLatin1String("jar")) {
                HorizonMod mod;
                mod.path = info.filePath();
                mod.name = info.completeBaseName();
                mod.problem = QStringLiteral("Java mods do not run on Horizon");
                mods.append(mod);
                continue;
            }

            if (suffix != QLatin1String("hmod") && suffix != QLatin1String("hnmod"))
                continue;

            HorizonMod mod;
            mod.path = info.filePath();
            readArchive(mod);
            if (nativeFolder && mod.kind == HorizonMod::Kind::Content)
                continue;  // a content mod left in the native folder
            mods.append(mod);
        }
    };

    scanDir(contentDir(gameRoot), false);
    scanDir(nativeDir(gameRoot), true);
    return mods;
}

HorizonModList HorizonModList::load(const QString& gameRoot)
{
    auto result = FS::read(HorizonMods::listPath(gameRoot));
    if (!result)
        return {};
    return parse(QString::fromUtf8(*result));
}

HorizonModList HorizonModList::parse(const QString& text)
{
    HorizonModList list;

    const auto lines = text.split('\n');
    for (const auto& raw : lines) {
        const auto line = raw.trimmed();
        if (line.isEmpty())
            continue;

        const auto colon = line.indexOf(':');
        if (colon < 0)
            continue;

        const auto key = line.left(colon).trimmed();
        const auto value = line.mid(colon + 1).trimmed();

        if (key.startsWith(QLatin1String("mod."))) {
            const auto id = key.mid(4);
            if (value == QLatin1String("true"))
                list.m_enabled.insert(id);
            else if (value == QLatin1String("false"))
                list.m_disabled.insert(id);
        } else if (key.startsWith(QLatin1String("key."))) {
            const auto rest = key.mid(4);
            const auto dot = rest.indexOf('.');
            if (dot > 0) {
                const auto id = rest.left(dot);
                const auto name = rest.mid(dot + 1);
                list.m_keys[id].insert(name, value);
            }
        }
        // Anything else stays out of the model and is dropped on save, like
        // the game's own parser (`mod_list.rs`) which only keeps these keys.
    }

    return list;
}

QString HorizonModList::serialize() const
{
    QStringList out;

    auto enabled = m_enabled.values();
    enabled.sort();
    for (const auto& id : enabled)
        out.append(QStringLiteral("mod.%1:true").arg(id));

    auto disabled = m_disabled.values();
    disabled.sort();
    for (const auto& id : disabled)
        out.append(QStringLiteral("mod.%1:false").arg(id));

    for (auto it = m_keys.constBegin(); it != m_keys.constEnd(); ++it) {
        for (auto key = it.value().constBegin(); key != it.value().constEnd(); ++key)
            out.append(QStringLiteral("key.%1.%2:%3").arg(it.key(), key.key(), key.value()));
    }

    if (out.isEmpty())
        return {};
    return out.join('\n') + '\n';
}

bool HorizonModList::save(const QString& gameRoot) const
{
    const auto path = HorizonMods::listPath(gameRoot);
    const auto text = serialize();
    if (text.isEmpty() && !QFileInfo::exists(path))
        return true;
    return bool(FS::write(path, text.toUtf8()));
}

bool HorizonModList::isOn(const HorizonMod& mod) const
{
    if (mod.id.isEmpty())
        return false;
    // Native mods are built in, so on until turned off; content mods wait
    // for an explicit `true`.
    if (mod.isNative())
        return !m_disabled.contains(mod.id);
    return m_enabled.contains(mod.id);
}

void HorizonModList::setOn(const HorizonMod& mod, bool on)
{
    if (mod.id.isEmpty())
        return;

    if (mod.isNative()) {
        if (on) {
            m_disabled.remove(mod.id);
            m_enabled.insert(mod.id);
        } else {
            m_enabled.remove(mod.id);
            m_disabled.insert(mod.id);
        }
    } else if (on) {
        m_enabled.insert(mod.id);
    } else {
        m_enabled.remove(mod.id);
    }
}
