// SPDX-License-Identifier: GPL-3.0-or-later
//
// The server must never hold its own injected Shift forever: every press or injected key
// re-arms a 5 s debounce deadline, a release cancels the timer, and expiry tells the loop
// to force the Shift up as the last-resort guard. Addon delays are capped so the loop
// cannot be stalled past the watchdog period while Shift is held.
#include "lotus-shift-watchdog.h"

#include <chrono>
#include <cstdio>

namespace {

    using namespace std::chrono_literals;
    using Clock = std::chrono::steady_clock;

    int  failures = 0;

    void check(const char* name, bool condition) {
        if (!condition) {
            std::printf("FAIL %s\n", name);
            ++failures;
        }
    }

} // namespace

int main() {
    const auto t0 = Clock::time_point{} + 1000s; // arbitrary monotonic base time

    // Fresh timer: nothing on the watch, never expires.
    {
        const ShiftHoldWatchdog watchdog;
        check("fresh watchdog is disarmed", !watchdog.armed() && !watchdog.expired(t0) && watchdog.remaining(t0) == std::chrono::steady_clock::duration::zero());
    }

    // One press arms the deadline: due exactly 5 s later.
    {
        ShiftHoldWatchdog watchdog;
        watchdog.activity(t0);
        check("activity arms the timer", watchdog.armed());
        check("not expired before five seconds", !watchdog.expired(t0 + 4999ms));
        check("expired at five seconds", watchdog.expired(t0 + 5s));
        check("remaining counts down", watchdog.remaining(t0 + 2s) == 3s);
    }

    // Debounce: fresh activity while holding pushes the deadline back.
    {
        ShiftHoldWatchdog watchdog;
        watchdog.activity(t0);
        watchdog.activity(t0 + 4s);
        check("activity re-arms the deadline", !watchdog.expired(t0 + 5s) && watchdog.expired(t0 + 9s));
    }

    // Cancel: a normal release disarms the timer, it can never fire afterwards.
    {
        ShiftHoldWatchdog watchdog;
        watchdog.activity(t0);
        watchdog.disarm();
        check("disarm cancels the timer", !watchdog.armed() && !watchdog.expired(t0 + 10s) && watchdog.remaining(t0 + 10s) == std::chrono::steady_clock::duration::zero());
    }

    // Remaining time is zero once due, so the loop clamps its poll timeout there.
    {
        ShiftHoldWatchdog watchdog;
        watchdog.activity(t0);
        check("remaining is zero when expired", watchdog.remaining(t0 + 6s) == std::chrono::steady_clock::duration::zero());
    }

    // Addon delays are capped so the loop cannot be stalled past the watchdog.
    check("zero delay stays zero", capInjectedDelay(0) == 0);
    check("normal delay passes through", capInjectedDelay(300) == 300);
    check("huge delay is capped", capInjectedDelay(60000) == 1000);

    if (failures != 0) {
        std::printf("%d failure(s)\n", failures);
        return 1;
    }
    std::printf("lotus shift watchdog: all checks passed\n");
    return 0;
}
