/*
 * SPDX-FileCopyrightText: 2026 Nguyễn Quang Minh <minhnbnt@gmail.com>
 * SPDX-FileCopyrightText: 2026 Nguyen Hoàng Ky <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#include "lotus-shift-watchdog.h"

void ShiftHoldWatchdog::activity(std::chrono::steady_clock::time_point now) {
    armed_    = true;
    deadline_ = now + kPeriod;
}

void ShiftHoldWatchdog::disarm() {
    armed_ = false;
}

bool ShiftHoldWatchdog::armed() const {
    return armed_;
}

bool ShiftHoldWatchdog::expired(std::chrono::steady_clock::time_point now) const {
    return armed_ && now >= deadline_;
}

std::chrono::steady_clock::duration ShiftHoldWatchdog::remaining(std::chrono::steady_clock::time_point now) const {
    if (!armed_ || now >= deadline_) {
        return std::chrono::steady_clock::duration::zero();
    }
    return deadline_ - now;
}
