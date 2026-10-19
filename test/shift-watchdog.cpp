// SPDX-License-Identifier: GPL-3.0-or-later
//
// The server must never hold its own injected Shift forever: while it holds a Shift and nothing is
// queued, a short settle deadline is armed so the main loop comes back and re-reads the keyboards.
// Every activity re-arms the deadline, so it cannot expire while a sequence is still running, and a
// release disarms it.
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
    using Watchdog         = ShiftHoldWatchdog;
    constexpr auto kPeriod = Watchdog::kPeriod;

    const auto     t0 = Clock::time_point{} + 1000s; // arbitrary monotonic base time

    // Fresh timer: nothing on the watch, never expires.
    {
        const Watchdog watchdog;
        check("fresh watchdog is disarmed", !watchdog.armed() && !watchdog.expired(t0) && watchdog.remaining(t0) == std::chrono::steady_clock::duration::zero());
    }

    // One activity arms the deadline: due exactly one settle period later.
    {
        Watchdog watchdog;
        watchdog.activity(t0);
        check("activity arms the deadline", watchdog.armed());
        check("not expired before the settle period", !watchdog.expired(t0 + kPeriod - 1ms));
        check("expired at the settle period", watchdog.expired(t0 + kPeriod));
        check("remaining counts down", watchdog.remaining(t0 + 2ms) == kPeriod - 2ms);
    }

    // Debounce: fresh activity while holding pushes the deadline back.
    {
        Watchdog watchdog;
        watchdog.activity(t0);
        watchdog.activity(t0 + 30ms);
        check("activity re-arms the deadline", !watchdog.expired(t0 + kPeriod) && watchdog.expired(t0 + 80ms));
    }

    // Cancel: a normal release disarms the deadline, it can never fire afterwards.
    {
        Watchdog watchdog;
        watchdog.activity(t0);
        watchdog.disarm();
        check("disarm cancels the deadline", !watchdog.armed() && !watchdog.expired(t0 + 10s) && watchdog.remaining(t0 + 10s) == std::chrono::steady_clock::duration::zero());
    }

    // Remaining time is zero once due, so the loop clamps its poll timeout there.
    {
        Watchdog watchdog;
        watchdog.activity(t0);
        check("remaining is zero when due", watchdog.remaining(t0 + kPeriod + 1ms) == std::chrono::steady_clock::duration::zero());
    }

    if (failures != 0) {
        std::printf("%d failure(s)\n", failures);
        return 1;
    }
    std::printf("lotus shift watchdog: all checks passed\n");
    return 0;
}
