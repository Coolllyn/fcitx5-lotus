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
#include <cstdint>
#include <linux/input.h>
#include <optional>
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
 * @brief Physical Shift key state, distinguishing a known release from an unknown reading.
 *
 * Unknown covers ioctl(EVIOCGKEY) failures and the no-keyboard case, so callers can
 * fail safe instead of treating an unreadable state as released.
 */
enum class ShiftState : std::uint8_t {
    Released,
    Held,
    Unknown,
};

/**
 * @brief Merges per-keyboard Shift states into one fail-safe state.
 * @param states One ShiftState per tracked keyboard.
 * @param hasUntracked True when at least one keyboard exists but cannot be tracked.
 * @return Held when any state is Held; Unknown when no state is given, an untracked
 *         keyboard exists, or any state is Unknown; Released otherwise.
 */
ShiftState mergeShiftStates(std::span<const ShiftState> states, bool hasUntracked);

/**
 * @brief Resolves a keyboard request using physical Shift state.
 * @param requested Operation asked by the addon.
 * @param state Live physical Shift key state.
 * @return Backspace for Select unless Shift is confirmed released,
 *         avoiding Shift+Left when the physical modifier is held or unknown.
 *         Other operations are unchanged.
 */
KbOp resolveKeyboardOp(KbOp requested, ShiftState state);

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
     * @brief Aggregates physical Shift state across keyboards.
     * @return Held if any Shift is pressed; Unknown if no Shift is pressed
     *         but some state is unavailable; Released otherwise.
     */
    ShiftState shiftHeld();

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
     * @return std::nullopt when the device is confirmed to have neither KEY_LEFTSHIFT nor
     *         KEY_RIGHTSHIFT, an engaged Keyboard with an invalid descriptor when the device
     *         cannot be opened or read via libevdev, an engaged tracked Keyboard with a
     *         valid descriptor otherwise.
     */
    static std::optional<Keyboard> openKeyboard(const std::string& devnode);

    /**
     * @brief Reads the live kernel key state of one tracked keyboard.
     * @param keyboard Keyboard to query.
     * @return Held when that keyboard holds a Shift key down, Unknown when its state
     *         cannot be read via ioctl(EVIOCGKEY), Released otherwise.
     */
    ShiftState                                     keyboardShiftHeld(Keyboard& keyboard);

    std::vector<Keyboard>                          keyboards_;
    std::vector<std::string>                       unreadableDevnodes_; ///< Keyboards whose Shift state cannot be tracked
    std::array<unsigned char, (KEY_MAX + 7U) / 8U> keyBits_{};          ///< One EVIOCGKEY answer
};

#endif // _LOTUS_PHYSICAL_SHIFT_H_
