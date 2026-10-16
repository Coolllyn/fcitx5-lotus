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

KbOp resolveKeyboardOp(KbOp requested, bool shift_held) {
    return (requested == KbOp::Select && shift_held) ? KbOp::Backspace : requested;
}

std::string PhysicalShiftState::devnodeOf(struct libinput_device* device) {
    const char* sysname = libinput_device_get_sysname(device);
    return sysname == nullptr ? std::string{} : std::string("/dev/input/") + sysname;
}

PhysicalShiftState::Keyboard PhysicalShiftState::openKeyboard(const std::string& devnode) {
    Keyboard  keyboard;
    const int fd     = open(devnode.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    keyboard.devnode = devnode;
    if (fd < 0) {
        return keyboard;
    }
    libevdev* raw = nullptr;
    if (libevdev_new_from_fd(fd, &raw) != 0 || raw == nullptr) {
        close(fd);
        return keyboard;
    }
    LibevdevDevice dev(raw);
    if (libevdev_has_event_code(dev.get(), EV_KEY, KEY_LEFTSHIFT) == 0 && libevdev_has_event_code(dev.get(), EV_KEY, KEY_RIGHTSHIFT) == 0) {
        return keyboard;
    }
    keyboard.fd  = FdGuard(fd);
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
    Keyboard keyboard = openKeyboard(devnode);
    if (!keyboard.fd.is_valid()) {
        LotusLogger::instance().warn("Not tracking Shift keys of " + devnode);
        return;
    }
    keyboards_.push_back(std::move(keyboard));
    LotusLogger::instance().info("Tracking Shift keys of " + devnode);
}

void PhysicalShiftState::forgetDevice(struct libinput_device* device) {
    const std::string devnode = devnodeOf(device);
    if (devnode.empty()) {
        return;
    }
    std::erase_if(keyboards_, [&devnode](const Keyboard& keyboard) { return keyboard.devnode == devnode; });
}

bool PhysicalShiftState::keyboardShiftHeld(Keyboard& keyboard) {
    key_bits_.fill(0);
    if (ioctl(keyboard.fd.get(), EVIOCGKEY(key_bits_.size()), key_bits_.data()) < 0) {
        return false;
    }
    return keyBitSet(key_bits_, KEY_LEFTSHIFT) || keyBitSet(key_bits_, KEY_RIGHTSHIFT);
}

bool PhysicalShiftState::shiftHeld() {
    return std::ranges::any_of(keyboards_, [this](Keyboard& keyboard) { return keyboardShiftHeld(keyboard); });
}
