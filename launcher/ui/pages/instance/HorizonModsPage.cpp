// SPDX-License-Identifier: GPL-3.0-only
/*
 *  horizon-prism - Prism Launcher fork for Project Horizon
 *  Copyright (C) 2026 horizon-prism contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include "HorizonModsPage.h"

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

#include "FileSystem.h"
#include "minecraft/HorizonInstance.h"

namespace {
QString kindLabel(const HorizonMod& mod)
{
    switch (mod.kind) {
        case HorizonMod::Kind::Content:
            return QObject::tr("Content");
        case HorizonMod::Kind::Native:
            return QObject::tr("Native");
        case HorizonMod::Kind::Both:
            return QObject::tr("Content + native");
    }
    return {};
}

QString statusLabel(const HorizonMod& mod)
{
    if (!mod.problem.isEmpty())
        return mod.problem;
    if (mod.isNative())
        return QObject::tr("installed, not built into this game");
    return QObject::tr("API %1, %2 side").arg(mod.api, mod.side);
}
}  // namespace

HorizonModsPage::HorizonModsPage(HorizonInstance* instance, QWidget* parent) : QWidget(parent), m_instance(instance)
{
    auto* layout = new QVBoxLayout(this);

    m_notice = new QLabel(this);
    m_notice->setWordWrap(true);
    layout->addWidget(m_notice);

    m_summary = new QLabel(this);
    layout->addWidget(m_summary);

    m_tree = new QTreeWidget(this);
    m_tree->setRootIsDecorated(false);
    m_tree->setUniformRowHeights(true);
    m_tree->setAlternatingRowColors(true);
    m_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_tree->setColumnCount(6);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(5, QHeaderView::Stretch);
    layout->addWidget(m_tree, 1);

    auto* buttons = new QHBoxLayout();
    auto* add = new QPushButton(tr("Add…"), this);
    auto* remove = new QPushButton(tr("Remove"), this);
    auto* refreshButton = new QPushButton(tr("Refresh"), this);
    auto* open = new QPushButton(tr("Open folder"), this);
    buttons->addWidget(add);
    buttons->addWidget(remove);
    buttons->addWidget(refreshButton);
    buttons->addStretch(1);
    buttons->addWidget(open);
    layout->addLayout(buttons);

    connect(add, &QPushButton::clicked, this, &HorizonModsPage::addMods);
    connect(remove, &QPushButton::clicked, this, &HorizonModsPage::removeSelected);
    connect(refreshButton, &QPushButton::clicked, this, &HorizonModsPage::refresh);
    connect(open, &QPushButton::clicked, this, &HorizonModsPage::openFolder);
    connect(m_tree, &QTreeWidget::itemChanged, this, &HorizonModsPage::toggleItem);

    retranslate();
    refresh();
}

void HorizonModsPage::retranslate()
{
    m_notice->setText(tr("Native mods (.hnmod) are listed but not built into this game: this "
                         "launcher runs a prebuilt horizon-client. Content mods (.hmod) are read "
                         "by the game at start."));
    m_tree->setHeaderLabels({ tr("On"), tr("Name"), tr("Id"), tr("Version"), tr("Kind"), tr("Status") });
}

void HorizonModsPage::openedImpl()
{
    refresh();
}

void HorizonModsPage::refresh()
{
    m_populating = true;
    m_tree->clear();
    m_mods = HorizonMods::scan(m_instance->gameRoot());
    m_list = HorizonModList::load(m_instance->gameRoot());

    int enabledCount = 0;
    for (int i = 0; i < m_mods.size(); i++) {
        const auto& mod = m_mods.at(i);
        auto* item = new QTreeWidgetItem(m_tree);
        item->setText(1, mod.title());
        item->setText(2, mod.id);
        item->setText(3, mod.version);
        item->setText(4, kindLabel(mod));
        item->setText(5, statusLabel(mod));
        item->setToolTip(1, mod.path);

        const bool canToggle = !mod.id.isEmpty() && mod.problem.isEmpty();
        if (canToggle) {
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(0, m_list.isOn(mod) ? Qt::Checked : Qt::Unchecked);
        } else {
            // Broken or unreadable: still selectable for removal, but no box.
            item->setFlags(item->flags() & ~Qt::ItemIsUserCheckable);
        }
        if (m_list.isOn(mod))
            enabledCount++;
        item->setData(0, Qt::UserRole, i);
    }

    m_tree->resizeColumnToContents(0);
    m_tree->resizeColumnToContents(2);
    m_tree->resizeColumnToContents(3);
    m_tree->resizeColumnToContents(4);

    m_summary->setText(tr("%1 mod(s) found, %2 on.").arg(m_mods.size()).arg(enabledCount));
    m_populating = false;
}

void HorizonModsPage::toggleItem(QTreeWidgetItem* item, int column)
{
    if (m_populating || column != 0)
        return;

    const int index = item->data(0, Qt::UserRole).toInt();
    if (index < 0 || index >= m_mods.size())
        return;

    const auto& mod = m_mods.at(index);
    m_list.setOn(mod, item->checkState(0) == Qt::Checked);
    m_list.save(m_instance->gameRoot());

    // Keep the summary in sync without rescanning the folders.
    int enabledCount = 0;
    for (const auto& candidate : m_mods) {
        if (m_list.isOn(candidate))
            enabledCount++;
    }
    m_summary->setText(tr("%1 mod(s) found, %2 on.").arg(m_mods.size()).arg(enabledCount));
}

void HorizonModsPage::addMods()
{
    const auto files = QFileDialog::getOpenFileNames(this, tr("Add Horizon mods"), QDir::homePath(),
                                                     tr("Horizon mods (*.hmod *.hnmod);;All files (*)"));
    if (files.isEmpty())
        return;

    QStringList copied;
    QStringList failed;

    for (const auto& file : files) {
        const QFileInfo info(file);
        const bool native = info.suffix().compare(QLatin1String("hnmod"), Qt::CaseInsensitive) == 0;
        const auto folder = native ? HorizonMods::nativeDir(m_instance->gameRoot()) : HorizonMods::contentDir(m_instance->gameRoot());
        if (!FS::ensureFolderPathExists(folder)) {
            failed.append(info.fileName());
            continue;
        }

        QString target = FS::PathCombine(folder, info.fileName());
        for (int i = 1; QFileInfo::exists(target); i++)
            target = FS::PathCombine(folder, QStringLiteral("%1-%2.%3").arg(info.completeBaseName()).arg(i).arg(info.suffix()));

        if (QFile::copy(file, target))
            copied.append(QFileInfo(target).fileName());
        else
            failed.append(info.fileName());
    }

    refresh();

    if (!failed.isEmpty())
        QMessageBox::warning(this, tr("Add Horizon mods"), tr("Could not copy: %1").arg(failed.join(QStringLiteral(", "))));
}

void HorizonModsPage::removeSelected()
{
    const auto items = m_tree->selectedItems();
    if (items.isEmpty())
        return;

    QStringList names;
    for (const auto* item : items)
        names.append(item->text(1));

    if (QMessageBox::question(this, tr("Remove Horizon mods"), tr("Remove %1?").arg(names.join(QStringLiteral(", ")))) != QMessageBox::Yes)
        return;

    for (const auto* item : items) {
        const int index = item->data(0, Qt::UserRole).toInt();
        if (index < 0 || index >= m_mods.size())
            continue;
        const auto& mod = m_mods.at(index);
        if (mod.folder)
            QDir(mod.path).removeRecursively();
        else
            QFile::remove(mod.path);
    }

    refresh();
}

void HorizonModsPage::openFolder()
{
    const auto folder = HorizonMods::contentDir(m_instance->gameRoot());
    FS::ensureFolderPathExists(folder);
    QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
}

bool HorizonModsPage::apply()
{
    m_list.save(m_instance->gameRoot());
    return true;
}
