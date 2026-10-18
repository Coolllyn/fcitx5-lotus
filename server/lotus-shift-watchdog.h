/*
 * SPDX-FileCopyrightText: 2026 Nguyễn Quang Minh <minhnbnt@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

/**
 * @file lotus-shift-watchdog.h
 * @brief Debounce timer that keeps the server from holding an injected Shift forever.
 */

#ifndef _LOTUS_SHIFT_WATCHDOG_H_
#define _LOTUS_SHIFT_WATCHDOG_H_

#include <chrono>
#include <cstdint>

/**
 * @brief Longest one keyboard request may keep the main loop asleep while Shift is held.
 *
 * The addon already paces keys with `interval`, capped at 1000 ms in lotus-server.cpp;
 * preDelay and postDelay get the same cap so the loop cannot be stalled past the watchdog
 * period while the server holds Shift.
 */
constexpr uint32_t kMaxInjectedDelayMs = 1000; ///< ms, matches the interval cap

/**
 * @brief Clamps one addon-provided delay so the main loop stays responsive.
 * @param ms Delay in milliseconds requested by the addon.
 * @return The delay capped at kMaxInjectedDelayMs.
 */
uint32_t capInjectedDelay(uint32_t ms);

/**
 * @brief Debounce timer for the Shift keys the server injected itself.
 *
 * Every press or injected key while holding re-arms the deadline (debounce); releasing the
 * server's own Shift cancels the timer. When the timer expires without fresh activity, the
 * server must release the Shift it holds — the last-resort guard against a Shift stuck
 * down forever.
 */
class ShiftHoldWatchdog {
  public:
    /**
     * @brief Re-arms the deadline with one fresh activity.
     * @param now Current monotonic time.
     */
    void activity(std::chrono::steady_clock::time_point now);

    /**
     * @brief Cancels the timer because the server no longer holds Shift.
     */
    void disarm();

    /**
     * @brief Reports whether the server is still on the watch.
     * @return True while the timer is armed.
     */
    [[nodiscard]] bool armed() const;

    /**
     * @brief Reports whether the watch period elapsed without fresh activity.
     * @param now Current monotonic time.
     * @return True when armed and `now` reached the deadline.
     */
    [[nodiscard]] bool expired(std::chrono::steady_clock::time_point now) const;

    /**
     * @brief Time left before the timer expires.
     * @param now Current monotonic time.
     * @return Remaining duration; zero when disarmed or already expired.
     */
    [[nodiscard]] std::chrono::steady_clock::duration remaining(std::chrono::steady_clock::time_point now) const;

  private:
    bool                                  armed_ = false; ///< True while the server holds its own Shift
    std::chrono::steady_clock::time_point deadline_;      ///< Last activity plus the watch period
};

#endif // _LOTUS_SHIFT_WATCHDOG_H_
