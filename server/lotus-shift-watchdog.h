/*
 * SPDX-FileCopyrightText: 2026 Nguyễn Quang Minh <minhnbnt@gmail.com>
 * SPDX-FileCopyrightText: 2026 Nguyen Hoàng Ky <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

/**
 * @file lotus-shift-watchdog.h
 * @brief Last-resort deadline that keeps the server from holding an injected Shift forever.
 */

#ifndef _LOTUS_SHIFT_WATCHDOG_H_
#define _LOTUS_SHIFT_WATCHDOG_H_

#include <chrono>

/**
 * @brief Deadline for the Shift the server presses on behalf of the user.
 *
 * After a Select sequence the server presses a Shift again while the user is still holding theirs,
 * and it cannot observe the release of that physical key. A deadline brings the main loop back:
 * once the settle period passed, the loop re-reads the keyboards and lets the server's own Shift go
 * when nobody holds Shift any more.
 */
class ShiftHoldWatchdog {
  public:
    static constexpr std::chrono::milliseconds kPeriod{50};

    /**
     * @brief Arms or re-arms the deadline with one fresh activity.
     * @param now Current monotonic time.
     */
    void activity(std::chrono::steady_clock::time_point now);

    /**
     * @brief Cancels the deadline because the server no longer holds Shift.
     */
    void disarm();

    /**
     * @brief Reports whether the deadline is on.
     * @return True while armed.
     */
    [[nodiscard]] bool armed() const;

    /**
     * @brief Reports whether the settle period elapsed without fresh activity.
     * @param now Current monotonic time.
     * @return True when armed and @p now reached the deadline.
     */
    [[nodiscard]] bool expired(std::chrono::steady_clock::time_point now) const;

    /**
     * @brief Time left before the deadline.
     * @param now Current monotonic time.
     * @return Remaining duration; zero when disarmed or already due.
     */
    [[nodiscard]] std::chrono::steady_clock::duration remaining(std::chrono::steady_clock::time_point now) const;

  private:
    bool                                  armed_ = false; ///< True while the deadline is on
    std::chrono::steady_clock::time_point deadline_;      ///< Last activity plus the settle period
};

#endif // _LOTUS_SHIFT_WATCHDOG_H_
