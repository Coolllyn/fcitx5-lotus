// SPDX-License-Identifier: GPL-3.0-or-later
//
// The server never holds a Shift of its own except for the in-flight selection it pressed itself,
// and a physically held Shift must turn a Select request into the Backspace fallback: a Shift+Left
// selection would fight the user's own modifier.
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

} // namespace

int main() {
    // A held physical Shift turns the selection into the Backspace fallback.
    check("Select with a held Shift falls back to Backspace", resolveKeyboardOp(KbOp::Select, true) == KbOp::Backspace);
    check("Select with no held Shift stays Select", resolveKeyboardOp(KbOp::Select, false) == KbOp::Select);
    check("Backspace is never changed", resolveKeyboardOp(KbOp::Backspace, true) == KbOp::Backspace);
    check("Backspace with no held Shift is unchanged", resolveKeyboardOp(KbOp::Backspace, false) == KbOp::Backspace);

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
