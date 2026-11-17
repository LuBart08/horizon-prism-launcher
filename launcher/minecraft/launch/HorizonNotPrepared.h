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

#include <launch/LaunchStep.h>

/*!
 * Milestone M1 placeholder: launching a Horizon instance must fail clearly
 * instead of running Minecraft's Java launch steps.
 *
 * M4 runs `HorizonPrepareStep` first, so this step now only guards against a
 * resources folder that is still missing after preparation.
 */
class HorizonNotPrepared : public LaunchStep {
    Q_OBJECT

   public:
    explicit HorizonNotPrepared(LaunchTask* parent);
    ~HorizonNotPrepared() override = default;

    void executeTask() override;
    bool canAbort() const override { return false; }
};
