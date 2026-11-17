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

#include <QWidget>

#include "ui/pages/BasePage.h"

class HorizonInstance;
class QCheckBox;
class QComboBox;
class QLabel;

/*!
 * Milestone M2: Horizon-specific instance settings.
 *
 * Edits the `horizon.json` model directly: channel, pinned client and
 * preparer versions, and fullscreen. The launch contract values are shown
 * read-only so the user can see what the pinned binaries must speak.
 */
class HorizonSettingsPage : public QWidget, public BasePage {
    Q_OBJECT

   public:
    explicit HorizonSettingsPage(HorizonInstance* instance, QWidget* parent = nullptr);
    ~HorizonSettingsPage() override = default;

    QString displayName() const override { return tr("Horizon"); }
    QIcon icon() const override { return QIcon::fromTheme("instance-settings"); }
    QString id() const override { return "horizon-settings"; }
    QString helpPage() const override { return "Instance-settings"; }

    bool apply() override;
    void loadSettings();

   private:
    HorizonInstance* m_instance;
    QComboBox* m_channel = nullptr;
    QComboBox* m_clientVersion = nullptr;
    QComboBox* m_preparerVersion = nullptr;
    QCheckBox* m_fullscreen = nullptr;
    QLabel* m_resources = nullptr;
    QLabel* m_minecraft = nullptr;
    QLabel* m_launchApi = nullptr;
    QLabel* m_outputVersion = nullptr;
    QLabel* m_nativeHooks = nullptr;
};
