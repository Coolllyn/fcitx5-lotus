/*
 * SPDX-FileCopyrightText: 2026 Nguyễn Hoàng Kỳ  <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

/**
 * @file lotus-uinput-device.h
 * @brief Description of the virtual keyboard fcitx5-lotus-server injects keys through.
 */

#ifndef _LOTUS_UINPUT_DEVICE_H_
#define _LOTUS_UINPUT_DEVICE_H_

#include <libevdev/libevdev-uinput.h>

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

/**
 * @brief Name of the virtual keyboard, identical to the raw setup it replaces.
 */
inline constexpr std::string_view kLotusUinputName = "Lotus-Uinput-Server";

/**
 * @brief Vendor id of the virtual keyboard.
 */
inline constexpr uint16_t kLotusUinputVendor = 0x1234;

/**
 * @brief Product id of the virtual keyboard.
 */
inline constexpr uint16_t kLotusUinputProduct = 0x5678;

/**
 * @brief Keys the virtual keyboard can emit, in the order they are enabled.
 */
inline constexpr std::array<unsigned int, 5> kLotusUinputKeys = {KEY_BACKSPACE, KEY_LEFT, KEY_LEFTSHIFT, KEY_RIGHTSHIFT, KEY_DELETE};

/**
 * @brief Frees a libevdev device.
 */
struct LibevdevDeleter {
    /**
     * @brief Releases the device handle.
     * @param device Device to free, may be null.
     */
    void operator()(libevdev* device) const noexcept {
        libevdev_free(device);
    }
};

/**
 * @brief Destroys a uinput device, closing /dev/uinput when it was opened by libevdev.
 */
struct LibevdevUinputDeleter {
    /**
     * @brief Releases the uinput device handle.
     * @param device Device to destroy, may be null.
     */
    void operator()(libevdev_uinput* device) const noexcept {
        libevdev_uinput_destroy(device); // NOLINT(cppcoreguidelines-owning-memory)
    }
};

/**
 * @brief Owning handle of a libevdev device.
 */
using LibevdevDevice = std::unique_ptr<libevdev, LibevdevDeleter>;

/**
 * @brief Owning handle of a uinput device.
 */
using LibevdevUinput = std::unique_ptr<libevdev_uinput, LibevdevUinputDeleter>;

/**
 * @brief Builds the in-memory description of the virtual keyboard, like the old UI_SET_* setup.
 *
 * @return Description, or a null handle when libevdev cannot allocate.
 */
inline LibevdevDevice buildLotusUinputDevice() {
    LibevdevDevice device{libevdev_new()};
    if (device == nullptr) {
        return device;
    }
    libevdev_set_name(device.get(), std::string(kLotusUinputName).c_str());
    libevdev_set_id_bustype(device.get(), BUS_USB);
    libevdev_set_id_vendor(device.get(), kLotusUinputVendor);
    libevdev_set_id_product(device.get(), kLotusUinputProduct);
    libevdev_enable_event_type(device.get(), EV_KEY);
    for (const unsigned int code : kLotusUinputKeys) {
        libevdev_enable_event_code(device.get(), EV_KEY, code, nullptr);
    }
    return device;
}

#endif // _LOTUS_UINPUT_DEVICE_H_
