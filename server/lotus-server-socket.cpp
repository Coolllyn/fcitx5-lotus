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

bool bindServerSocket(int fd, const AbstractSocket& address, std::string_view label) {
    if (::bind(fd, address.asSockaddr(), address.length) != 0) {
        LotusLogger::instance().error("Failed to bind " + std::string(label) + ": " + std::string(strerror(errno)));
        return false;
    }
    return true;
}

bool listenServerSocket(int fd, std::string_view label) {
    if (::listen(fd, kListenBacklog) != 0) {
        LotusLogger::instance().error("Failed to listen on " + std::string(label) + ": " + std::string(strerror(errno)));
        return false;
    }
    return true;
}
