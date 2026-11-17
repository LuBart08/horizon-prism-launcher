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

#include <QStringList>

#include <LoggedProcess.h>
#include <launch/LaunchStep.h>

class HorizonInstance;

/*!
 * Milestone M4: prepare (or repair) the Horizon resources before launching.
 *
 * The step asks `horizon-prepare --check` whether the resources are usable.
 * Only optional extras (sounds, built-in packs, panorama, fonts, languages)
 * are tolerated as gaps: the game runs without them. Anything else triggers
 * `horizon-prepare --prepare --progress json`, whose JSON events drive the
 * task progress, followed by a final `--check`.
 *
 * The preparer reads Prism's own layout through `--prism-dir`, so no server
 * jar is needed: Prism's client jar and assets are reused.
 */
class HorizonPrepareStep : public LaunchStep {
    Q_OBJECT

   public:
    explicit HorizonPrepareStep(LaunchTask* parent);
    ~HorizonPrepareStep() override = default;

    void executeTask() override;
    bool abort() override;
    bool canAbort() const override;

   private slots:
    void onPrepareLog(const QStringList& lines, MessageLevel level);
    void onPrepareState(LoggedProcess::State state);

   private:
    struct CheckResult {
        bool upToDate = false;
        QStringList optionalGaps;
        QStringList requiredGaps;
    };

    CheckResult runCheck(HorizonInstance* horizon);
    void startPrepare(HorizonInstance* horizon);
    void finishAfterPrepare(HorizonInstance* horizon, int exitCode);

    QString resolveJavaHome(int requiredMajor) const;
    QString prismDataDirFor(HorizonInstance* horizon) const;

    LoggedProcess m_process;
    QString m_preparer;
    QStringList m_tail;
    QString m_lastStatus;
};
