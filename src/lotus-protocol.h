/*
 * SPDX-FileCopyrightText: 2025 Võ Ngô Hoàng Thành 
 * SPDX-FileCopyrightText: 2026 Nguyễn Hoàng Kỳ  
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#ifndef _LOTUS_PROTOCOL_H_
#define _LOTUS_PROTOCOL_H_

#include <cstddef>
#include <cstdint>

/**
 * @brief Message on the keyboard socket from the fcitx5 addon to uinput server.
 */
struct KbMsg {
    int8_t   op;         ///< KB_OP_* operation
    size_t   count;      ///< number of backspaces / characters to select
    uint32_t pre_delay;  ///< ms to wait BEFORE sending any key (to let recent commits render)
    uint32_t post_delay; ///< ms to wait BEFORE sending the final trigger key (to let DOM expand selection)
};

enum KbOp : int8_t {
    KB_OP_BACKSPACE = 0, ///< emit count BackSpace key events
    KB_OP_SELECT    = 1, ///< select count characters via Shift+Left
};

#endif // _LOTUS_PROTOCOL_H_