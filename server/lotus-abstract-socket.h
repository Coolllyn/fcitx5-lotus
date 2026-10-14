/*
 * SPDX-FileCopyrightText: 2026 Nguyễn Hoàng Kỳ  <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

/**
 * @file lotus-abstract-socket.h
 * @brief Abstract-namespace AF_UNIX address helper for fcitx5-lotus-server sockets.
 */

#ifndef _LOTUS_ABSTRACT_SOCKET_H_
#define _LOTUS_ABSTRACT_SOCKET_H_

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>

/**
 * @brief An abstract-namespace AF_UNIX address together with the length bind() needs.
 */
struct AbstractSocket {
    struct sockaddr_un address{};  ///< Address; sun_path[0] = '\0' marks the abstract namespace
    socklen_t          length = 0; ///< Length to pass to bind()

    /**
     * @brief Views the address as the generic sockaddr bind() expects.
     * @return Borrowed pointer to the embedded address, owned by this object.
     */
    [[nodiscard]] const struct sockaddr* asSockaddr() const {
        return reinterpret_cast<const struct sockaddr*>(&address); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    }
};

/**
 * @brief Builds the abstract socket address of a path, truncating to the kernel limit.
 * @param path Abstract socket name, without the leading NUL.
 * @return Address ready for bind().
 */
inline AbstractSocket makeAbstractSocket(const std::string& path) {
    AbstractSocket socket;
    socket.address.sun_family  = AF_UNIX;
    socket.address.sun_path[0] = '\0';
    const std::size_t length   = std::min(path.size(), sizeof(socket.address.sun_path) - 2U);
    std::memcpy(&socket.address.sun_path[1], path.data(), length);
    socket.length = static_cast<socklen_t>(offsetof(struct sockaddr_un, sun_path) + length + 1U);
    return socket;
}

#endif // _LOTUS_ABSTRACT_SOCKET_H_
