/*
 * SPDX-FileCopyrightText: 2026 Nguyễn Quang Minh <minhnbnt@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#include "lotus-shift-watchdog.h"

#include <algorithm>

namespace {

    constexpr auto kShiftWatchPeriod = std::chrono::seconds(5); ///< quiet time after which held Shift must go up

} // namespace

uint32_t capInjectedDelay(uint32_t ms) {
    return std::min(ms, kMaxInjectedDelayMs);
}

void ShiftHoldWatchdog::activity(std::chrono::steady_clock::time_point now) {
    armed_    = true;
    deadline_ = now + kShiftWatchPeriod;
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
