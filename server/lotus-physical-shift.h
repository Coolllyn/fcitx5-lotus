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
 * @param shiftHeld True when a physical keyboard currently holds a Shift key down.
 * @return KbOp::Backspace when a Shift+Left selection would fight the held modifier, requested otherwise.
 */
KbOp resolveKeyboardOp(KbOp requested, bool shiftHeld);

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
     * @brief Reads the live kernel key state of every tracked keyboard.
     * @return True when any tracked keyboard holds KEY_LEFTSHIFT or KEY_RIGHTSHIFT down.
     */
    bool shiftHeld();

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
    static Keyboard openKeyboard(const std::string& devnode);

    /**
     * @brief Reads the live kernel key state of one tracked keyboard.
     * @param keyboard Keyboard to query.
     * @return True when that keyboard holds a Shift key down.
     */
    bool                                           keyboardShiftHeld(Keyboard& keyboard);

    std::vector<Keyboard>                          keyboards_;
    std::array<unsigned char, (KEY_MAX + 7U) / 8U> keyBits_{}; ///< One EVIOCGKEY answer
};

#endif // _LOTUS_PHYSICAL_SHIFT_H_
