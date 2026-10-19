/*
 * SPDX-FileCopyrightText: 2026 Nguyễn Hoàng Kỳ  <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

/**
 * @file lotus-physical-shift.h
 * @brief Tracks the Shift keys of the physical keyboards the server can see.
 */

#ifndef _LOTUS_PHYSICAL_SHIFT_H_
#define _LOTUS_PHYSICAL_SHIFT_H_

#include "common-fd-guard.h"
#include "common-protocol.h"
#include "lotus-uinput-device.h"

#include <array>
#include <linux/input.h>
#include <span>
#include <string>
#include <vector>

struct libinput_device;

/**
 * @brief Tests one key code inside an EVIOCGKEY bitmap.
 * @param bits Bitmap returned by EVIOCGKEY.
 * @param code Linux KEY_* code.
 * @return True when the key is currently down.
 */
bool keyBitSet(std::span<const unsigned char> bits, unsigned int code);

/**
 * @brief Resolves the operation the server executes for one keyboard request.
 * @param requested Operation asked by the addon.
 * @param shiftUnknown True when no physical keyboard is readable, so the user's Shift state is unknown.
 * @return KbOp::Backspace when a Shift+Left selection cannot be trusted to keep the user's modifier, requested otherwise.
 */
KbOp resolveKeyboardOp(KbOp requested, bool shiftUnknown);

/**
 * @brief The physical keyboards the server tracks to know whether Shift is held.
 */
class PhysicalShiftState {
  public:
    PhysicalShiftState()                                     = default;
    PhysicalShiftState(const PhysicalShiftState&)            = delete;
    PhysicalShiftState& operator=(const PhysicalShiftState&) = delete;
    PhysicalShiftState(PhysicalShiftState&&)                 = delete;
    PhysicalShiftState& operator=(PhysicalShiftState&&)      = delete;
    ~PhysicalShiftState()                                    = default;

    /**
     * @brief Opens the device as a tracked keyboard when it can report Shift keys.
     * @param device Device reported by LIBINPUT_EVENT_DEVICE_ADDED.
     */
    void trackDevice(struct libinput_device* device);

    /**
     * @brief Drops the tracked keyboard reported by LIBINPUT_EVENT_DEVICE_REMOVED.
     * @param device Device reported by LIBINPUT_EVENT_DEVICE_REMOVED.
     */
    void forgetDevice(struct libinput_device* device);

    /**
     * @brief One live read of the physical Shift keys.
     */
    struct ShiftSnapshot {
        bool left     = false; ///< KEY_LEFTSHIFT is down on some tracked keyboard
        bool right    = false; ///< KEY_RIGHTSHIFT is down on some tracked keyboard
        bool readable = false; ///< True when at least one tracked keyboard answered the query

        /**
         * @brief Reports whether any Shift key is down.
         * @return True when left or right is down.
         */
        [[nodiscard]] bool any() const {
            return left || right;
        }
    };

    /**
     * @brief Reads both Shift keys of every tracked keyboard.
     * @return Live state; all false when no keyboard can be read.
     */
    ShiftSnapshot snapshot();

    /**
     * @brief Reports whether any physical keyboard is tracked.
     * @return True when snapshot() reflects real keyboards.
     */
    [[nodiscard]] bool tracksKeyboards() const {
        return !keyboards_.empty();
    }

  private:
    /**
     * @brief One tracked physical keyboard.
     */
    struct Keyboard {
        std::string    devnode; ///< /dev/input/<sysname>, used to match add/remove events
        FdGuard        fd;      ///< Read-only descriptor, queried with EVIOCGKEY
        LibevdevDevice dev;     ///< Handle used for the EV_KEY capability check
    };

    /**
     * @brief Builds the /dev/input node of a libinput device.
     * @param device libinput device reported by an add/remove event.
     * @return Device node, empty when the sysname is unknown.
     */
    static std::string devnodeOf(struct libinput_device* device);

    /**
     * @brief Opens one keyboard for Shift state queries.
     * @param devnode Device node to open.
     * @return Tracked keyboard with an invalid descriptor when the device is not usable as a keyboard.
     */
    static Keyboard                                openKeyboard(const std::string& devnode);

    std::vector<Keyboard>                          keyboards_;
    std::array<unsigned char, (KEY_MAX + 7U) / 8U> keyBits_{}; ///< One EVIOCGKEY answer
};

/**
 * @brief Which Shift keys one Select sequence must press and release.
 */
struct SelectShiftPlan {
    bool pressLeft{false};    ///< press KEY_LEFTSHIFT before the selection (nothing is effectively down)
    bool releaseLeft{false};  ///< KEY_LEFTSHIFT is effectively down and must be released before the Delete
    bool releaseRight{false}; ///< KEY_RIGHTSHIFT is effectively down and must be released before the Delete
};

/**
 * @brief Builds the Shift plan of one Select sequence.
 * @param physical Live physical Shift state.
 * @param oursLeft True when the server already holds KEY_LEFTSHIFT from an earlier sequence.
 * @param oursRight True when the server already holds KEY_RIGHTSHIFT from an earlier sequence.
 * @return Plan for this sequence: press only when nothing is effectively down, release everything that is.
 */
SelectShiftPlan planSelectShift(const PhysicalShiftState::ShiftSnapshot& physical, bool oursLeft, bool oursRight);

#endif // _LOTUS_PHYSICAL_SHIFT_H_
