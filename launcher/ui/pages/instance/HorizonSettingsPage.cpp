// SPDX-License-Identifier: GPL-3.0-only
/*
 *  horizon-prism - Prism Launcher fork for Project Horizon
 *  Copyright (C) 2026 horizon-prism contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include "HorizonSettingsPage.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "minecraft/HorizonInstance.h"
#include "ui/dialogs/HorizonVersionsDialog.h"

HorizonSettingsPage::HorizonSettingsPage(HorizonInstance* instance, QWidget* parent) : QWidget(parent), m_instance(instance)
{
    auto* layout = new QVBoxLayout(this);

    auto* general = new QGroupBox(tr("Horizon"), this);
    auto* form = new QFormLayout(general);

    m_channel = new QComboBox(general);
    m_channel->setEditable(true);
    m_channel->addItems({ "stable", "beta", "pinned" });
    form->addRow(tr("Channel:"), m_channel);

    m_clientVersion = new QComboBox(general);
    m_clientVersion->setEditable(true);
    m_clientVersion->setInsertPolicy(QComboBox::NoInsert);
    m_clientVersion->setToolTip(tr("Leave empty to use the newest installed Horizon version."));
    form->addRow(tr("Client version:"), m_clientVersion);

    m_preparerVersion = new QComboBox(general);
    m_preparerVersion->setEditable(true);
    m_preparerVersion->setInsertPolicy(QComboBox::NoInsert);
    m_preparerVersion->setToolTip(tr("Leave empty to use the newest installed Horizon version."));
    form->addRow(tr("Preparer version:"), m_preparerVersion);

    m_fullscreen = new QCheckBox(tr("Start in fullscreen"), general);
    form->addRow(QString(), m_fullscreen);

    m_manageVersions = new QPushButton(tr("Manage Horizon versions…"), general);
    m_manageVersions->setToolTip(tr("Install, update or remove Horizon versions, and use one for this instance."));
    form->addRow(QString(), m_manageVersions);
    connect(m_manageVersions, &QPushButton::clicked, this, &HorizonSettingsPage::openVersionsDialog);

    m_resources = new QLabel(general);
    m_resources->setWordWrap(true);
    m_resources->setTextInteractionFlags(Qt::TextSelectableByMouse);
    form->addRow(tr("Resources:"), m_resources);

    layout->addWidget(general);

    auto* contract = new QGroupBox(tr("Launch contract (read-only)"), this);
    auto* contractForm = new QFormLayout(contract);

    m_minecraft = new QLabel(contract);
    m_launchApi = new QLabel(contract);
    m_outputVersion = new QLabel(contract);
    m_nativeHooks = new QLabel(contract);

    contractForm->addRow(tr("Minecraft:"), m_minecraft);
    contractForm->addRow(tr("Launch API:"), m_launchApi);
    contractForm->addRow(tr("Output version:"), m_outputVersion);
    contractForm->addRow(tr("Native hooks:"), m_nativeHooks);

    layout->addWidget(contract);
    layout->addStretch(1);

    loadSettings();
}

void HorizonSettingsPage::loadSettings()
{
    const auto versions = m_instance->installedVersions();

    m_channel->setCurrentText(m_instance->channel());

    m_clientVersion->clear();
    m_clientVersion->addItems(versions);
    m_clientVersion->setCurrentText(m_instance->clientVersion());

    m_preparerVersion->clear();
    m_preparerVersion->addItems(versions);
    m_preparerVersion->setCurrentText(m_instance->preparerVersion());

    m_fullscreen->setChecked(m_instance->fullscreen());

    m_resources->setText(tr("%1 (%2)").arg(m_instance->resourcesDir(), m_instance->isPrepared() ? tr("prepared") : tr("not prepared")));
    m_minecraft->setText(m_instance->horizonMinecraftVersion());
    m_launchApi->setText(m_instance->launchApiPin());
    m_outputVersion->setText(QString::number(m_instance->outputVersionPin()));
    m_nativeHooks->setText(QString::number(m_instance->nativeHooksPin()));
}

bool HorizonSettingsPage::apply()
{
    const auto channel = m_channel->currentText().trimmed();
    m_instance->setChannel(channel.isEmpty() ? QStringLiteral("stable") : channel);
    m_instance->setClientVersion(m_clientVersion->currentText().trimmed());
    m_instance->setPreparerVersion(m_preparerVersion->currentText().trimmed());
    m_instance->setFullscreen(m_fullscreen->isChecked());
    return true;
}

void HorizonSettingsPage::openVersionsDialog()
{
    HorizonVersionsDialog dialog(m_instance, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const auto chosen = dialog.chosenVersion();
    if (chosen.isEmpty())
        return;

    // The page is what apply() saves, so fill the combos instead of writing
    // horizon.json behind its back.
    loadSettings();
    m_clientVersion->setCurrentText(chosen);
    m_preparerVersion->setCurrentText(chosen);
}
