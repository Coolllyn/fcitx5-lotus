/*
 * SPDX-FileCopyrightText: 2026 Nguyễn Hoàng Kỳ  <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#include "lotus-physical-shift.h"
#include "lotus-logger.h"

#include <algorithm>
#include <fcntl.h>
#include <libinput.h>
#include <string_view>
#include <sys/ioctl.h>
#include <unistd.h>
#include <utility>

bool keyBitSet(std::span<const unsigned char> bits, unsigned int code) {
    return (bits[code / 8U] & (1U << (code % 8U))) != 0;
}

KbOp resolveKeyboardOp(KbOp requested, ShiftState state) {
    return (requested == KbOp::Select && state != ShiftState::Released) ? KbOp::Backspace : requested;
}

std::string PhysicalShiftState::devnodeOf(struct libinput_device* device) {
    const char* sysname = libinput_device_get_sysname(device);
    return sysname == nullptr ? std::string{} : std::string("/dev/input/") + sysname;
}

std::optional<PhysicalShiftState::Keyboard> PhysicalShiftState::openKeyboard(const std::string& devnode) {
    Keyboard  keyboard;
    const int fd     = open(devnode.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    keyboard.devnode = devnode;
    if (fd < 0) {
        return keyboard;
    }
    keyboard.fd   = FdGuard(fd);
    libevdev* raw = nullptr;
    if (libevdev_new_from_fd(keyboard.fd.get(), &raw) != 0 || raw == nullptr) {
        Keyboard invalid;
        invalid.devnode = devnode;
        return invalid;
    }
    LibevdevDevice dev(raw);
    if (libevdev_has_event_code(dev.get(), EV_KEY, KEY_LEFTSHIFT) == 0 && libevdev_has_event_code(dev.get(), EV_KEY, KEY_RIGHTSHIFT) == 0) {
        return std::nullopt;
    }
    keyboard.dev = std::move(dev);
    return keyboard;
}

void PhysicalShiftState::trackDevice(struct libinput_device* device) {
    if (libinput_device_has_capability(device, LIBINPUT_DEVICE_CAP_KEYBOARD) == 0) {
        return;
    }
    const char* const rawName = libinput_device_get_name(device);
    if (rawName != nullptr && std::string_view(rawName) == kLotusUinputName) {
        return;
    }
    const std::string devnode = devnodeOf(device);
    if (devnode.empty() || std::ranges::any_of(keyboards_, [&devnode](const Keyboard& keyboard) { return keyboard.devnode == devnode; })) {
        return;
    }
    std::optional<Keyboard> keyboardOpt = openKeyboard(devnode);
    if (!keyboardOpt) {
        return;
    }
    Keyboard keyboard = std::move(*keyboardOpt);
    if (!keyboard.fd.isValid()) {
        if (!std::ranges::any_of(unreadableDevnodes_, [&devnode](const std::string& node) { return node == devnode; })) {
            unreadableDevnodes_.push_back(devnode);
        }
        LotusLogger::instance().warn("Not tracking Shift keys of " + devnode);
        return;
    }
    std::erase_if(unreadableDevnodes_, [&devnode](const std::string& node) { return node == devnode; });
    keyboards_.push_back(std::move(keyboard));
    LotusLogger::instance().info("Tracking Shift keys of " + devnode);
}

void PhysicalShiftState::forgetDevice(struct libinput_device* device) {
    const std::string devnode = devnodeOf(device);
    if (devnode.empty()) {
        return;
    }
    std::erase_if(keyboards_, [&devnode](const Keyboard& keyboard) { return keyboard.devnode == devnode; });
    std::erase_if(unreadableDevnodes_, [&devnode](const std::string& node) { return node == devnode; });
}

ShiftState PhysicalShiftState::keyboardShiftHeld(Keyboard& keyboard) {
    keyBits_.fill(0);
    if (ioctl(keyboard.fd.get(), EVIOCGKEY(keyBits_.size()), keyBits_.data()) < 0) {
        LotusLogger::instance().warn("Failed to read Shift key state of " + keyboard.devnode);
        return ShiftState::Unknown;
    }
    if (keyBitSet(keyBits_, KEY_LEFTSHIFT) || keyBitSet(keyBits_, KEY_RIGHTSHIFT)) {
        return ShiftState::Held;
    }
    return ShiftState::Released;
}

ShiftState mergeShiftStates(std::span<const ShiftState> states, bool hasUntracked) {
    if (std::ranges::any_of(states, [](ShiftState state) { return state == ShiftState::Held; })) {
        return ShiftState::Held;
    }
    if (states.empty() || hasUntracked || std::ranges::any_of(states, [](ShiftState state) { return state == ShiftState::Unknown; })) {
        return ShiftState::Unknown;
    }
    return ShiftState::Released;
}

ShiftState PhysicalShiftState::shiftHeld() {
    std::vector<ShiftState> states;
    states.reserve(keyboards_.size());
    for (Keyboard& keyboard : keyboards_) {
        states.push_back(keyboardShiftHeld(keyboard));
    }
    return mergeShiftStates(states, !unreadableDevnodes_.empty());
}
