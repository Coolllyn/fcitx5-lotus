/*
 * SPDX-FileCopyrightText: 2026 LotusInputMethod contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

/**
 * @file lotus-server-socket.h
 * @brief Checked bind()/listen() wrappers for the uinput server sockets.
 *
 * The wrappers report failures through syslog and return false, so the
 * server never runs deaf on a socket it failed to set up.
 */

#ifndef _SERVER_LOTUS_SERVER_SOCKET_H_
#define _SERVER_LOTUS_SERVER_SOCKET_H_

#include "lotus-abstract-socket.h"

#include <string_view>

/**
 * @brief Binds a server socket, logging the errno on failure.
 * @param fd Socket descriptor to bind.
 * @param address Abstract address to bind to.
 * @param label Socket name used in the log message (e.g. "keyboard socket").
 * @return True when bind() succeeded, false otherwise.
 */
bool bindServerSocket(int fd, const AbstractSocket& address, std::string_view label);

/**
 * @brief Marks a bound server socket as listening, logging the errno on failure.
 * @param fd Bound socket descriptor.
 * @param label Socket name used in the log message (e.g. "keyboard socket").
 * @return True when listen() succeeded, false otherwise.
 */
bool listenServerSocket(int fd, std::string_view label);

#endif // _SERVER_LOTUS_SERVER_SOCKET_H_
