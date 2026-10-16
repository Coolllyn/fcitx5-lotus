/*
 * SPDX-FileCopyrightText: 2026 Nguyễn Hoàng Kỳ  <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#include "lotus-abstract-socket.h"

#include <algorithm>
#include <cstddef>
#include <cstring>

AbstractSocket makeAbstractSocket(const std::string& path) {
    AbstractSocket socket;
    socket.address.sun_family  = AF_UNIX;
    socket.address.sun_path[0] = '\0';
    const std::size_t length   = std::min(path.size(), sizeof(socket.address.sun_path) - 2U);
    std::memcpy(&socket.address.sun_path[1], path.data(), length);
    socket.length = static_cast<socklen_t>(offsetof(struct sockaddr_un, sun_path) + length + 1U);
    return socket;
}
