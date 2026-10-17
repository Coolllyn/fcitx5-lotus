/*
 * SPDX-FileCopyrightText: 2025 Võ Ngô Hoàng Thành <thanhpy2009@gmail.com>
 * SPDX-FileCopyrightText: 2026 Nguyễn Hoàng Kỳ  <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */
#include "common-executable-path.h"
#include "common-fd-guard.h"
#include "lotus-log.h"
#include "lotus-monitor.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <string>

#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <climits> // IWYU pragma: keep

bool authenticateMouseSocketPeer(int sock, std::string& out_exe_path) {
    struct ucred cred{};
    socklen_t    credLen = sizeof(cred);

    if (getsockopt(sock, SOL_SOCKET, SO_PEERCRED, &cred, &credLen) != 0) {
        LOTUS_ERROR("Failed to get peer credentials: " + std::string(strerror(errno)));
        return false;
    }

    std::array<char, 64> procPath{};
    snprintf(procPath.data(), procPath.size(), "/proc/%d/cmdline", cred.pid);

    FdGuard fd(open(procPath.data(), O_RDONLY));
    if (!fd.isValid()) {
        LOTUS_ERROR("Failed to open cmdline: " + std::string(strerror(errno)));
        return false;
    }

    std::array<char, PATH_MAX> exePath{};
    ssize_t                    bytesRead = read(fd.get(), exePath.data(), exePath.size() - 1);

    if (bytesRead <= 0) {
        LOTUS_ERROR("Failed to read cmdline: " + std::string(strerror(errno)));
        return false;
    }

    out_exe_path = exePath.data();

    return isAllowedExecutablePath(exePath.data(), LOTUS_SERVER_EXECUTABLE, LOTUS_ALT_EXECUTABLE_PREFIX);
}
