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
#include <QWidget>

#include "minecraft/mod/HorizonMod.h"
#include "ui/pages/BasePage.h"

class HorizonInstance;
class QLabel;
class QTreeWidget;
class QTreeWidgetItem;

/*!
 * Milestone M5: the Horizon content page.
 *
 * Lists `.hmod` content mods and `.hnmod` native mods found in the instance's
 * game folder, turns them on and off by editing `horizon-mods.txt`, and
 * copies new files into the right folder.
 *
 * This fork launches a prebuilt `horizon-client`: native mods are shown, but
 * they are not built into the game, so the page says so instead of pretending
 * they run.
 */
class HorizonModsPage : public QWidget, public BasePage {
    Q_OBJECT

   public:
    explicit HorizonModsPage(HorizonInstance* instance, QWidget* parent = nullptr);
    ~HorizonModsPage() override = default;

    QString displayName() const override { return tr("Horizon Mods"); }
    QIcon icon() const override { return QIcon::fromTheme("mods"); }
    QString id() const override { return "horizon-mods"; }
    QString helpPage() const override { return "Mods"; }

    bool apply() override;
    void openedImpl() override;
    void retranslate() override;

   private:
    void refresh();
    void addMods();
    void removeSelected();
    void openFolder();
    void toggleItem(QTreeWidgetItem* item, int column);

    HorizonInstance* m_instance;
    HorizonModList m_list;
    QList<HorizonMod> m_mods;

    QLabel* m_notice = nullptr;
    QLabel* m_summary = nullptr;
    QTreeWidget* m_tree = nullptr;
    bool m_populating = false;
};
