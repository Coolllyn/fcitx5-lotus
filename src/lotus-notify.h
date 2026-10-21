/*
 * SPDX-FileCopyrightText: 2026 Nguyen Hoàng Ky <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

/**
 * @file lotus-notify.h
 * @brief Desktop notifications sent by the Lotus addon.
 */

#ifndef _LOTUS_NOTIFY_H_
#define _LOTUS_NOTIFY_H_

/**
 * @brief Tells the user that uinput modes cannot work without the Lotus uinput server.
 */
void notifyMissingUinputServer();

#endif // _LOTUS_NOTIFY_H_
