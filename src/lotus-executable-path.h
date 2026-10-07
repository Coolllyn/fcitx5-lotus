/*
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Ky <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

/**
 * @file lotus-executable-path.h
 * @brief Peer executable path checks shared by the addon and the uinput server.
 */

#ifndef _LOTUS_EXECUTABLE_PATH_H_
#define _LOTUS_EXECUTABLE_PATH_H_

#include <string_view>

#include "lotus-export.h"

#ifndef LOTUS_FCITX5_EXECUTABLE
#define LOTUS_FCITX5_EXECUTABLE "/usr/bin/fcitx5"
#endif
#ifndef LOTUS_SERVER_EXECUTABLE
#define LOTUS_SERVER_EXECUTABLE "/usr/bin/fcitx5-lotus-server"
#endif
#ifndef LOTUS_ALT_EXECUTABLE_PREFIX
#define LOTUS_ALT_EXECUTABLE_PREFIX ""
#endif

/**
 * @brief Checks whether a peer executable may act as the expected program.
 * @param exePath Peer executable path as reported by readlink(/proc/<pid>/exe).
 * @param expectedPath Configured executable path (e.g. LOTUS_FCITX5_EXECUTABLE).
 * @param altPrefix Configured extra prefix (e.g. LOTUS_ALT_EXECUTABLE_PREFIX), may be empty.
 * @return True when the peer path is accepted.
 */
// LOTUS_EXPORT: also read by the headless tests through lotus_test_core.
bool LOTUS_EXPORT isAllowedExecutablePath(std::string_view exePath, std::string_view expectedPath, std::string_view altPrefix);

#endif // _LOTUS_EXECUTABLE_PATH_H_
