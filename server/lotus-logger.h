/*
 * SPDX-FileCopyrightText: 2026 Nguyễn Hoàng Kỳ  <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

/**
 * @file lotus-logger.h
 * @brief Thin syslog() wrapper for fcitx5-lotus-server.
 *
 * Messages go to the local syslog daemon (journald/rsyslog), which adds
 * timestamps and rotates the log.  Facility: LOG_DAEMON, ident:
 * "fcitx5-lotus-server".
 */

#ifndef _LOTUS_LOGGER_H_
#define _LOTUS_LOGGER_H_

#include <atomic>
#include <cstdint>
#include <string_view>

enum class LogLevel : std::uint8_t {
    Debug,
    Info,
    Warn,
    Error,
    None,
};

class LotusLogger {
  public:
    /**
     * @brief Instance constructor
     */
    static LotusLogger& instance();

    // Rule of five
    LotusLogger(const LotusLogger&)            = delete;
    LotusLogger& operator=(const LotusLogger&) = delete;
    LotusLogger(LotusLogger&&)                 = delete;
    LotusLogger& operator=(LotusLogger&&)      = delete;

    /**
     * @brief Set minimum log level
     */
    void setLevel(LogLevel level);

    /**
     * @brief Check if logging is enabled for given level
     */
    [[nodiscard]] bool isEnabled(LogLevel level) const;

    // Convenience methods
    void debug(std::string_view msg) const {
        if (isEnabled(LogLevel::Debug))
            log(LogLevel::Debug, msg);
    }
    void info(std::string_view msg) const {
        if (isEnabled(LogLevel::Info))
            log(LogLevel::Info, msg);
    }
    void warn(std::string_view msg) const {
        if (isEnabled(LogLevel::Warn))
            log(LogLevel::Warn, msg);
    }
    void error(std::string_view msg) const {
        if (isEnabled(LogLevel::Error))
            log(LogLevel::Error, msg);
    }

  private:
    /**
     * @brief Constructor
     * @param level Minimum log level to output
     */
    explicit LotusLogger(LogLevel level = LogLevel::Info);

    /**
     * @brief Destructor
     */
    ~LotusLogger();

    /**
     * @brief Log a message through syslog()
     * @param level Level the message is logged at
     * @param message Message text, passed as data and never as a format string
     */
    static void           log(LogLevel level, std::string_view message);

    std::atomic<LogLevel> level_;
};

#endif // _LOTUS_LOGGER_H_
