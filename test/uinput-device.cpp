// SPDX-FileCopyrightText: 2026 Nguyen Hoang Ky <nhktmdzhg@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The server injects keys through a libevdev-uinput keyboard. Describing that keyboard in memory
// needs no privileges, so this test pins the identity and the exact key set the old raw UI_SET_*
// setup produced, without opening /dev/uinput (it passes inside a user namespace as well).
#include "lotus-uinput-device.h"

#include <cstdio>
#include <exception>
#include <string_view>

namespace {

    int  failures = 0;

    void check(const char* name, bool condition) {
        if (!condition) {
            std::printf("FAIL %s\n", name);
            ++failures;
        }
    }

} // namespace

int runUinputDevice() {
    const LibevdevDevice device = buildLotusUinputDevice();
    if (device == nullptr) {
        std::printf("FAIL libevdev_new\n");
        return 1;
    }

    const char* const name = libevdev_get_name(device.get());
    check("device name", name != nullptr && std::string_view(name) == kLotusUinputName);
    check("bustype is BUS_USB", libevdev_get_id_bustype(device.get()) == BUS_USB);
    check("vendor id", libevdev_get_id_vendor(device.get()) == kLotusUinputVendor);
    check("product id", libevdev_get_id_product(device.get()) == kLotusUinputProduct);
    check("EV_KEY is enabled", libevdev_has_event_type(device.get(), EV_KEY) != 0);
    for (const unsigned int code : kLotusUinputKeys) {
        check("declared key is enabled", libevdev_has_event_code(device.get(), EV_KEY, code) != 0);
    }
    check("no unrelated key", libevdev_has_event_code(device.get(), EV_KEY, KEY_A) == 0);
    check("no unrelated event type", libevdev_has_event_type(device.get(), EV_REL) == 0);

    if (failures != 0) {
        std::printf("%d failure(s)\n", failures);
        return 1;
    }
    std::printf("lotus uinput device: all checks passed\n");
    return 0;
}

int main() {
    try {
        return runUinputDevice();
    } catch (const std::exception& error) {
        std::printf("FAIL unexpected exception: %s\n", error.what());
        return 1;
    }
}
