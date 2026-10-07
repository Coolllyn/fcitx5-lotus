/*
 * SPDX-FileCopyrightText: 2026 Nguyễn Hoàng Kỳ  <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

/**
 * @file ack-apps.h
 * @brief List of applications requiring acknowledgment workaround.
 *
 * These browsers need special handling for uinput mode to work correctly.
 */

#include <array>
#include <string_view>

/**
 * @brief List of application names requiring ACK workaround.
 *
 * Chromium-based browsers that need special handling for text replacement.
 */
inline constexpr std::array<std::string_view, 12> ack_apps = {
    "chrome", "chromium", "brave", "edge", "vivaldi", "opera", "coccoc", "cromite", "helium", "thorium", "slimjet", "yandex",
};
