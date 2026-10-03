/*
 * SPDX-FileCopyrightText: 2025 Võ Ngô Hoàng Thành <thanhpy2009@gmail.com>
 * SPDX-FileCopyrightText: 2026 Nguyễn Hoàng Kỳ  <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */
#include "lotus-monitor.h"
#include "lotus-utils.h"

#include <cstdio>
#include <cstring>
#include <string>

#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <limits.h>

bool authenticateMouseSocketPeer(int sock, std::string& out_exe_path) {
    struct ucred cred{};
    socklen_t    cred_len = sizeof(cred);

    if (getsockopt(sock, SOL_SOCKET, SO_PEERCRED, &cred, &cred_len) != 0) {
        LOTUS_ERROR("Failed to get peer credentials: " + std::string(strerror(errno)));
        return false;
    }

    char proc_path[64];
    snprintf(proc_path, sizeof(proc_path), "/proc/%d/cmdline", cred.pid);

    int fd = open(proc_path, O_RDONLY);
    if (fd < 0) {
        LOTUS_ERROR("Failed to open cmdline: " + std::string(strerror(errno)));
        return false;
    }

    char    exe_path[PATH_MAX] = {0};
    ssize_t bytes_read         = read(fd, exe_path, sizeof(exe_path) - 1);
    close(fd);

    if (bytes_read <= 0) {
        LOTUS_ERROR("Failed to read cmdline: " + std::string(strerror(errno)));
        return false;
    }

    out_exe_path = exe_path;

    return strcmp(exe_path, "/usr/bin/fcitx5-lotus-server") == 0;
}
