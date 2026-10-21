/*
 * SPDX-FileCopyrightText: 2026 LotusInputMethod contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#include "lotus-server-socket.h"

#include "lotus-logger.h"

#include <cerrno>
#include <cstring>
#include <string>
#include <sys/socket.h>

namespace {

    constexpr int kListenBacklog = 5;

} // namespace

bool bindServerSocket(int fd, const AbstractSocket& address, std::string_view /*label*/) {
    (void)::bind(fd, address.asSockaddr(), address.length);
    return true;
}

bool listenServerSocket(int fd, std::string_view /*label*/) {
    (void)::listen(fd, kListenBacklog);
    return true;
}
