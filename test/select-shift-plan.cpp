// SPDX-License-Identifier: GPL-3.0-or-later
//
// The server must not steal the user's Shift and must not turn the Delete of a Select sequence into
// Shift+Delete (a cut): it presses Shift only when nothing is effectively down, releases everything
// that is before the Delete, and puts a still-held Shift back afterwards.
#include "lotus-physical-shift.h"

#include <array>
#include <cstdio>

namespace {

    int  failures = 0;

    void check(const char* name, bool condition) {
        if (!condition) {
            std::printf("FAIL %s\n", name);
            ++failures;
        }
    }

    void checkPlan(const char* name, const SelectShiftPlan& plan, bool pressLeft, bool releaseLeft, bool releaseRight) {
        check(name, plan.pressLeft == pressLeft && plan.releaseLeft == releaseLeft && plan.releaseRight == releaseRight);
    }

} // namespace

int main() {
    const PhysicalShiftState::ShiftSnapshot none{};
    const PhysicalShiftState::ShiftSnapshot left{.left = true};
    const PhysicalShiftState::ShiftSnapshot right{.right = true};
    const PhysicalShiftState::ShiftSnapshot both{.left = true, .right = true};

    // Nothing held: press Left for the selection; the matching release happens through oursLeft at
    // run time, so the plan itself has nothing to release yet.
    checkPlan("nothing held", planSelectShift(none, false, false), true, false, false);
    // The user holds Left: no press, but that side must be released before the Delete.
    checkPlan("user holds left", planSelectShift(left, false, false), false, true, false);
    // The user holds Right: the old code left it down, so the Delete became a cut.
    checkPlan("user holds right", planSelectShift(right, false, false), false, false, true);
    checkPlan("user holds both", planSelectShift(both, false, false), false, true, true);
    // The server still holds a Shift from an earlier sequence: release it, never press an extra one.
    checkPlan("stale own left", planSelectShift(none, true, false), false, true, false);
    checkPlan("stale own right", planSelectShift(none, false, true), false, false, true);

    // The Backspace stream is the fallback for an unknown Shift state only.
    check("Select with unknown shift falls back", resolveKeyboardOp(KbOp::Select, true) == KbOp::Backspace);
    check("Select with known shift stays Select", resolveKeyboardOp(KbOp::Select, false) == KbOp::Select);
    check("Backspace is never changed", resolveKeyboardOp(KbOp::Backspace, true) == KbOp::Backspace);

    // Decoding one EVIOCGKEY answer.
    std::array<unsigned char, (KEY_MAX + 7U) / 8U> bits{};
    check("empty bitmap", !keyBitSet(bits, KEY_LEFTSHIFT) && !keyBitSet(bits, KEY_RIGHTSHIFT));
    bits[KEY_LEFTSHIFT / 8U] |= static_cast<unsigned char>(1U << (KEY_LEFTSHIFT % 8U));
    check("left bit only", keyBitSet(bits, KEY_LEFTSHIFT) && !keyBitSet(bits, KEY_RIGHTSHIFT));
    bits[KEY_LEFTSHIFT / 8U] = 0;
    bits[KEY_RIGHTSHIFT / 8U] |= static_cast<unsigned char>(1U << (KEY_RIGHTSHIFT % 8U));
    check("right bit only", keyBitSet(bits, KEY_RIGHTSHIFT) && !keyBitSet(bits, KEY_LEFTSHIFT));

    if (failures != 0) {
        std::printf("%d failure(s)\n", failures);
        return 1;
    }
    std::printf("lotus select shift plan: all checks passed\n");
    return 0;
}
