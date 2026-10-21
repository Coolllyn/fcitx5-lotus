/*
 * SPDX-FileCopyrightText: 2026 Nguyễn Hoàng Kỳ  <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#include "lotus-logger.h"

#include <syslog.h>

LotusLogger& LotusLogger::instance() {
    static LotusLogger instance_;
    return instance_;
}

LotusLogger::LotusLogger(LogLevel level) {
    level_.store(level, std::memory_order_release);
    openlog("fcitx5-lotus-server", LOG_PID, LOG_DAEMON);
}

LotusLogger::~LotusLogger() {
    closelog();
}

void LotusLogger::setLevel(LogLevel level) {
    level_.store(level, std::memory_order_release);
}

bool LotusLogger::isEnabled(LogLevel level) const {
    return level >= level_.load(std::memory_order_acquire);
}

void LotusLogger::log(LogLevel level, std::string_view message) {
    int priority = LOG_INFO;
    switch (level) {
        case LogLevel::Debug: priority = LOG_DEBUG; break;
        case LogLevel::Info: priority = LOG_INFO; break;
        case LogLevel::Warn: priority = LOG_WARNING; break;
        case LogLevel::Error: priority = LOG_ERR; break;
        default: break;
    }
    // Message is data, never a format string
    syslog(priority, "%.*s", static_cast<int>(message.size()), message.data());
}
