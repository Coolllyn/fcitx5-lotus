/*
 * SPDX-FileCopyrightText: 2025 Võ Ngô Hoàng Thành <thanhpy2009@gmail.com>
 * SPDX-FileCopyrightText: 2026 Nguyễn Hoàng Kỳ  <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

/**
 * @file lotus-log.h
 * @brief Log category and message macros of the addon.
 *
 * Macro-only header: only translation units that log include it, so the log
 * header chain stays out of every other consumer of lotus-utils.h.
 */

#ifndef _FCITX5_LOTUS_LOG_H_
#define _FCITX5_LOTUS_LOG_H_

#include <fcitx-utils/log.h>

// The log category lives inside namespace fcitx: a global "lotus" function would collide with the
// lotus:: engine namespace, which the addon includes since it drives the C++ engine directly.
namespace fcitx {
    FCITX_DECLARE_LOG_CATEGORY(lotusLog);
}

#define LOTUS_DEBUG(msg) FCITX_LOGC(fcitx::lotusLog, Debug) << "[DEBUG] " << msg
#define LOTUS_INFO(msg)  FCITX_LOGC(fcitx::lotusLog, Info) << "[INFO] " << msg
#define LOTUS_WARN(msg)  FCITX_LOGC(fcitx::lotusLog, Warn) << "[WARN] " << msg
#define LOTUS_ERROR(msg) FCITX_LOGC(fcitx::lotusLog, Error) << "[ERROR] " << msg

#endif // _FCITX5_LOTUS_LOG_H_
