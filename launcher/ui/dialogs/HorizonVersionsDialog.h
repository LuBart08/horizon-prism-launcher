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

#include <QDialog>
#include <QList>

#include <functional>

#include "minecraft/HorizonRelease.h"

class QComboBox;
class QLabel;
class QProgressBar;
class QPushButton;
class QTableWidget;
class HorizonInstance;

/*!
 * Milestone M7: install, update and remove Horizon versions.
 *
 * The official launcher already owns that job, so this dialog is a thin front
 * end to its CLI (`horizon-launcher versions|install|remove|update --check`).
 * It never downloads anything by itself and never removes a version another
 * Horizon instance or the official launcher plays.
 */
class HorizonVersionsDialog : public QDialog {
    Q_OBJECT

   public:
    explicit HorizonVersionsDialog(HorizonInstance* instance, QWidget* parent = nullptr);

    /// The version the user picked with "Use for this instance", if any.
    QString chosenVersion() const { return m_chosen; }

   private slots:
    void refresh();
    void checkForUpdates();
    void installSelected();
    void removeSelected();
    void useForInstance();

   private:
    void populate();
    void updateButtons();
    void setBusy(bool busy, const QString& what);
    void runCli(const QStringList& args, const QString& what, const std::function<void(int, const QByteArray&, const QByteArray&)>& done);
    void showFailure(const QString& what, const QByteArray& stderrText);
    QString selectedTag() const;
    bool isInUse(const QString& tag) const;

    HorizonInstance* m_instance = nullptr;
    QTableWidget* m_table = nullptr;
    QLabel* m_status = nullptr;
    QProgressBar* m_progress = nullptr;
    QPushButton* m_refresh = nullptr;
    QPushButton* m_checkUpdates = nullptr;
    QPushButton* m_install = nullptr;
    QPushButton* m_remove = nullptr;
    QPushButton* m_use = nullptr;

    QList<HorizonRelease> m_releases;
    HorizonInstalledVersions m_installed;
    HorizonUpdates m_updates;
    QString m_chosen;
    bool m_busy = false;
};
