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

#include <QByteArray>

#include <LoggedProcess.h>
#include <launch/LaunchStep.h>
#include <minecraft/auth/AuthSession.h>
#include <minecraft/launch/MinecraftTarget.h>

class HorizonInstance;

/*!
 * Milestone M3: run `horizon-client` for a Horizon instance.
 *
 * The account session is written to the client's stdin (`--session-stdin`),
 * never passed on the command line, so tokens do not end up in `ps` output or
 * in the launch log.
 */
class HorizonLaunchStep : public LaunchStep {
    Q_OBJECT

   public:
    HorizonLaunchStep(LaunchTask* parent, AuthSessionPtr session, MinecraftTarget::Ptr targetToJoin);
    ~HorizonLaunchStep() override = default;

    void executeTask() override;
    bool abort() override;
    bool canAbort() const override { return true; }

   private slots:
    void onStarted();
    void onLogLines(const QStringList& lines, MessageLevel level);
    void onState(LoggedProcess::State state);

   private:
    QByteArray sessionPayload() const;
    void probeLaunchApi(HorizonInstance* horizon, const QString& client);

    LoggedProcess m_process;
    AuthSessionPtr m_session;
    MinecraftTarget::Ptr m_targetToJoin;
    QString m_crashReport;
    QString m_crashReportPrefix = QStringLiteral("Horizon: crash report saved to ");
};
