// SPDX-License-Identifier: GPL-3.0-only
/*
 *  horizon-prism - Prism Launcher fork for Project Horizon
 *  Copyright (C) 2026 horizon-prism contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include "HorizonNotPrepared.h"

#include "launch/LaunchTask.h"
#include "minecraft/HorizonInstance.h"

HorizonNotPrepared::HorizonNotPrepared(LaunchTask* parent) : LaunchStep(parent) {}

void HorizonNotPrepared::executeTask()
{
    auto* horizon = qobject_cast<HorizonInstance*>(m_parent->instance());
    if (horizon != nullptr && !horizon->isPrepared()) {
        emit logLine(tr("Horizon resources are not prepared yet: %1").arg(horizon->resourcesDir()), MessageLevel::Error);
        emitFailed(tr("This Horizon instance has no prepared resources yet. Automatic preparation arrives in milestone M4."));
        return;
    }

    emit logLine(tr("Horizon launching is not wired into the launcher yet (milestone M3)."), MessageLevel::Error);
    emitFailed(tr("This Horizon instance cannot be launched yet: launching arrives in milestone M3."));
}
