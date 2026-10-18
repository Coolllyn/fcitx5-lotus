/*
 * SPDX-FileCopyrightText: 2025 Võ Ngô Hoàng Thành <thanhpy2009@gmail.com>
 * SPDX-FileCopyrightText: 2026 Nguyễn Hoàng Kỳ  <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#include "lotus-abstract-socket.h"
#include "lotus-server.h"
#include "lotus-logger.h"
#include "common-executable-path.h"
#include "common-fd-guard.h"
#include "common-protocol.h"
#include "common-socket-name.h"
#include "lotus-physical-shift.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <span>
#include <thread>
#include <utility>
#include <vector>
#include <csignal>
#include <cstddef>

#include <climits> // IWYU pragma: keep
#include <sched.h>
#include <unistd.h>

std::atomic<bool> g_running{true};

bool              UinputDevice::initialize() {
    dev_ = buildLotusUinputDevice();
    if (dev_ == nullptr) {
        LotusLogger::instance().error("Failed to build the uinput device description");
        return false;
    }

    libevdev_uinput* raw = nullptr;
    const int        err = libevdev_uinput_create_from_device(dev_.get(), LIBEVDEV_UINPUT_OPEN_MANAGED, &raw);
    if (err != 0) {
        LotusLogger::instance().error("Failed to create the uinput device: " + std::string(strerror(-err)));
        return false;
    }
    uidev_.reset(raw);
    if (const char* node = libevdev_uinput_get_devnode(uidev_.get()); node != nullptr) {
        for (int waited = 0; waited < 1000 && access(node, R_OK) != 0; waited += 10) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        LotusLogger::instance().info("Uinput device ready: " + std::string(node));
    } else {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        LotusLogger::instance().info("Uinput device ready (device node unknown)");
    }
    return true;
}

void UinputDevice::reportWriteFailure(int error) {
    static bool reported = false;
    if (!reported) {
        reported = true;
        LotusLogger::instance().error("Failed to write an uinput event: " + std::string(strerror(-error)));
    }
}

void UinputDevice::sendTap(uint16_t code) {
    if (uidev_ == nullptr) {
        return;
    }
    if (const int err = libevdev_uinput_write_event(uidev_.get(), EV_KEY, code, 1); err != 0) {
        reportWriteFailure(err);
        return;
    }
    if (const int err = libevdev_uinput_write_event(uidev_.get(), EV_SYN, SYN_REPORT, 0); err != 0) {
        reportWriteFailure(err);
        return;
    }
    if (const int err = libevdev_uinput_write_event(uidev_.get(), EV_KEY, code, 0); err != 0) {
        reportWriteFailure(err);
        return;
    }
    if (const int err = libevdev_uinput_write_event(uidev_.get(), EV_SYN, SYN_REPORT, 0); err != 0) {
        reportWriteFailure(err);
    }
}

void UinputDevice::sendMod(uint16_t code, int value) {
    if (uidev_ == nullptr) {
        return;
    }
    if (const int err = libevdev_uinput_write_event(uidev_.get(), EV_KEY, code, value); err != 0) {
        reportWriteFailure(err);
        return;
    }
    if (const int err = libevdev_uinput_write_event(uidev_.get(), EV_SYN, SYN_REPORT, 0); err != 0) {
        reportWriteFailure(err);
    }
}

void UinputDevice::sendBackspace() {
    sendTap(KEY_BACKSPACE);
}

void UinputDevice::sendDelete() {
    sendTap(KEY_DELETE);
}

void UinputDevice::sendShiftDown(uint16_t code) {
    sendMod(code, 1);
}

void UinputDevice::sendShiftUp(uint16_t code) {
    sendMod(code, 0);
}

void UinputDevice::sendLeft() {
    sendTap(KEY_LEFT);
}

LibinputContext::LibinputContext(const struct libinput_interface* interface) : udev_(udev_new()) {
    if (udev_ == nullptr) {
        return;
    }
    li_.reset(libinput_udev_create_context(interface, nullptr, udev_.get()));
    if (li_ != nullptr && libinput_udev_assign_seat(li_.get(), "seat0") != 0) {
        li_.reset();
    }
}

void signalHandler(int sig) {
    if (sig == SIGTERM || sig == SIGINT) {
        g_running.store(false);
    }
}

std::string getCurrentUsername() {
    struct passwd  pwd{};
    struct passwd* result  = nullptr;
    long           bufSize = sysconf(_SC_GETPW_R_SIZE_MAX);
    if (bufSize == -1) {
        bufSize = 16384;
    }
    std::vector<char> buffer(static_cast<std::size_t>(bufSize));
    if (getpwuid_r(getuid(), &pwd, buffer.data(), buffer.size(), &result) != 0 || result == nullptr) {
        return "unknown";
    }
    return {result->pw_name};
}

std::optional<uid_t> getUidForUser(std::string_view username) {
    const std::string      name(username);
    struct passwd          pwBuf{};
    struct passwd*         pw = nullptr;
    std::array<char, 1024> buffer{};
    if (getpwnam_r(name.c_str(), &pwBuf, buffer.data(), buffer.size(), &pw) != 0 || pw == nullptr) {
        return std::nullopt;
    }
    return pw->pw_uid;
}

void boostProcessPriority() {
    if (setpriority(PRIO_PROCESS, 0, -10) != 0) { //NOLINT
        LotusLogger::instance().error("Failed to boost process priority");
    }
}

void pinToPcore() {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    for (int i = 0; i <= 3; ++i)
        CPU_SET(i, &cpuset);
    if (sched_setaffinity(0, sizeof(cpuset), &cpuset) != 0) {
        LotusLogger::instance().error("Failed to pin process to core");
    }
}

int open_restricted(const char* path, int flags, void* /*user_data*/) {
    int fd = open(path, flags);
    return fd < 0 ? -errno : fd;
}

void close_restricted(int fd, void* /*user_data*/) {
    close(fd);
}

const struct libinput_interface interface = {
    .open_restricted  = open_restricted,
    .close_restricted = close_restricted,
};

std::optional<pid_t> authorizePeer(int fd, uid_t expectedUid, std::string_view socketLabel) {
    struct ucred cred{};
    socklen_t    len = sizeof(struct ucred);
    if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cred, &len) != 0) {
        LotusLogger::instance().warn("Failed to get peer credentials for " + std::string(socketLabel));
        return std::nullopt;
    }
    if (cred.uid != expectedUid) {
        LotusLogger::instance().warn("Unauthorized UID connection attempt to " + std::string(socketLabel) + " from UID: " + std::to_string(cred.uid));
        return std::nullopt;
    }

    const std::string          exeLink = "/proc/" + std::to_string(cred.pid) + "/exe";
    std::array<char, PATH_MAX> exePath{};
    const ssize_t              length = ::readlink(exeLink.c_str(), exePath.data(), exePath.size() - 1);
    if (length <= 0) {
        LotusLogger::instance().warn("Failed to resolve the peer executable for " + std::string(socketLabel));
        return std::nullopt;
    }

    const std::string_view exe(exePath.data(), static_cast<std::size_t>(length));
    if (!isAllowedExecutablePath(exe, LOTUS_FCITX5_EXECUTABLE, LOTUS_ALT_EXECUTABLE_PREFIX)) {
        LotusLogger::instance().warn("Unauthorized executable connection attempt to " + std::string(socketLabel) + " from: " + std::string(exe));
        return std::nullopt;
    }
    return cred.pid;
}

int main(int argc, char* argv[]) {
    const std::span<char* const> args(argv, static_cast<std::size_t>(argc));
    const std::string            targetUser = args.size() == 3 && std::string_view(args[1]) == "-u" ? std::string(args[2]) : getCurrentUsername();
    LotusLogger::instance().info("Target user: " + targetUser);

    const std::optional<uid_t> expectedUid = getUidForUser(targetUser);
    if (!expectedUid.has_value()) {
        LotusLogger::instance().error("Failed to find UID for target user: " + targetUser);
        return 1;
    }

    boostProcessPriority();
    pinToPcore();

    const std::string    socketNamespace = lotusSocketNamespace(targetUser);
    const AbstractSocket kbSocket        = makeAbstractSocket("lotussocket-" + socketNamespace + "-kbSocket");
    const AbstractSocket mouseSocket     = makeAbstractSocket("lotussocket-" + socketNamespace + "-mouseSocket");

    // Setup Uinput
    UinputDevice       uinput;
    PhysicalShiftState physical_shift;
    if (!uinput.initialize()) {
        LotusLogger::instance().error("Failed to initialize uinput device");
        return 1;
    }

    FdGuard serverFd(socket(AF_UNIX, SOCK_SEQPACKET | SOCK_NONBLOCK, 0));
    FdGuard mouseServerFd(socket(AF_UNIX, SOCK_SEQPACKET | SOCK_NONBLOCK, 0));

    if (bind(serverFd.get(), kbSocket.asSockaddr(), kbSocket.length) != 0) {
        LotusLogger::instance().error("Failed to bind socket");
        return 1;
    }

    if (bind(mouseServerFd.get(), mouseSocket.asSockaddr(), mouseSocket.length) != 0) {
        LotusLogger::instance().error("Failed to bind socket");
        return 1;
    }

    listen(serverFd.get(), 5);
    listen(mouseServerFd.get(), 5);

    LibinputContext li_ctx(&interface);
    if (!li_ctx.isValid()) {
        LotusLogger::instance().error("Failed to create libinput/udev context");
        return 1;
    }

    constexpr std::size_t        kKeyboardServerIndex = 0U; ///< Slot of the keyboard socket server fd
    constexpr std::size_t        kMouseServerIndex    = 2U; ///< Slot of the mouse socket server fd
    constexpr std::size_t        kKbClientIndex       = 3U; ///< Slot of the connected keyboard socket client

    std::array<struct pollfd, 4> fds{
        {
            {.fd = serverFd.get(), .events = POLLIN, .revents = 0},
            {.fd = li_ctx.getFd(), .events = POLLIN, .revents = 0},
            {.fd = mouseServerFd.get(), .events = POLLIN, .revents = 0},
            {.fd = -1, .events = POLLIN, .revents = 0},
        },
    };

    FdGuard         addonFd;
    FdGuard         kbClientFd;
    size_t          pendingBackspaces = 0;
    size_t          pendingSelects    = 0;     ///< remaining Shift+Left events to pace out
    bool            oursLeft          = false; ///< the server holds KEY_LEFTSHIFT on behalf of the user
    bool            oursRight         = false; ///< the server holds KEY_RIGHTSHIFT on behalf of the user

    SelectShiftPlan selectPlan; ///< Shift plan of the in-flight Select sequence

    const auto      releaseOursIfNobodyHolds = [&] {
        const auto physical = physical_shift.snapshot();
        if (oursLeft && !physical.left) {
            uinput.sendShiftUp(KEY_LEFTSHIFT);
            oursLeft = false;
        }
        if (oursRight && !physical.right) {
            uinput.sendShiftUp(KEY_RIGHTSHIFT);
            oursRight = false;
        }
    };

    uint32_t         currentPostDelay = 0; ///< stores postDelay from KbMsg
    uint32_t         currentInterval  = 5; ///< ms between injected keys within one request

    struct sigaction sa{};
    sa.sa_handler = signalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGINT, &sa, nullptr);

    while (g_running.load(std::memory_order_acquire)) {
        int pollTimeout = -1;
        if (pendingBackspaces > 0 || pendingSelects > 0) {
            pollTimeout = static_cast<int>(currentInterval);
        }
        int ret = poll(fds.data(), fds.size(), pollTimeout);

        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }

        if (ret == 0) {
            if (pendingBackspaces > 0) {
                if (pendingBackspaces == 1 && currentPostDelay > 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(currentPostDelay));
                }
                uinput.sendBackspace();
                --pendingBackspaces;
            } else if (pendingSelects > 0) {
                if (pendingSelects > 1) {
                    uinput.sendLeft();
                    --pendingSelects;
                } else if (selectPlan.releaseLeft || selectPlan.releaseRight || oursLeft || oursRight) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    if (selectPlan.releaseLeft || oursLeft) {
                        uinput.sendShiftUp(KEY_LEFTSHIFT);
                        oursLeft = false;
                    }
                    if (selectPlan.releaseRight || oursRight) {
                        uinput.sendShiftUp(KEY_RIGHTSHIFT);
                        oursRight = false;
                    }
                    selectPlan = {};
                } else {
                    uinput.sendDelete();
                    if (currentPostDelay > 0) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(currentPostDelay));
                    }
                    uinput.sendLeft();
                    --pendingSelects;
                    const auto physical = physical_shift.snapshot();
                    if (physical.left) {
                        uinput.sendShiftDown(KEY_LEFTSHIFT);
                        oursLeft = true;
                    }
                    if (physical.right) {
                        uinput.sendShiftDown(KEY_RIGHTSHIFT);
                        oursRight = true;
                    }
                }
            }
        }

        libinput_dispatch(li_ctx.getLi());

        // handle socket (backspace)
        if ((static_cast<unsigned>(fds[kKeyboardServerIndex].revents) & static_cast<unsigned>(POLLIN)) != 0) {
            FdGuard candidate(accept4(serverFd.get(), nullptr, nullptr, SOCK_NONBLOCK));
            if (candidate.isValid()) {
                const std::optional<pid_t> peer = authorizePeer(candidate.get(), *expectedUid, "keyboard socket");

                if (peer.has_value()) {
                    LotusLogger::instance().info("Fcitx5 connected to keyboard socket (PID: " + std::to_string(*peer) + ")");
                    kbClientFd             = std::move(candidate);
                    fds[kKbClientIndex].fd = kbClientFd.get();
                }
            }
        }

        // handle connect from addon
        if (fds[kKbClientIndex].fd >= 0 &&
            (static_cast<unsigned>(fds[kKbClientIndex].revents) & (static_cast<unsigned>(POLLIN) | static_cast<unsigned>(POLLHUP) | static_cast<unsigned>(POLLERR))) != 0) {
            KbMsg   msg{};
            ssize_t n = recv(fds[kKbClientIndex].fd, &msg, sizeof(msg), 0);
            if (n <= 0) {
                LotusLogger::instance().warn("Keyboard client disconnected or connection error");
                releaseOursIfNobodyHolds(); // never leave Shift stuck down
                selectPlan     = {};
                pendingSelects = 0;
                kbClientFd.reset(-1);
                fds[kKbClientIndex].fd = -1;
            } else if (std::cmp_not_equal(n, sizeof(KbMsg)) && std::cmp_not_equal(n, sizeof(int32_t))) {
                LotusLogger::instance().warn("Malformed keyboard message (" + std::to_string(n) + " bytes)");
            } else {
                currentInterval = msg.interval > 0 ? std::min<uint32_t>(msg.interval, 1000) : 5;
                releaseOursIfNobodyHolds();
                const auto physical = physical_shift.snapshot();
                const KbOp op       = resolveKeyboardOp(msg.op, !physical_shift.tracksKeyboards());
                if (op != msg.op) {
                    LotusLogger::instance().info("Shift state unknown: replacing with Backspace instead of Shift+Left");
                }
                if (msg.count > 0 && op == KbOp::Select) {
                    if (msg.preDelay > 0) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(msg.preDelay));
                    }
                    currentPostDelay = msg.postDelay;
                    selectPlan       = planSelectShift(physical, oursLeft, oursRight);
                    if (selectPlan.pressLeft) {
                        uinput.sendShiftDown(KEY_LEFTSHIFT);
                        oursLeft = true;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(currentInterval));
                    uinput.sendLeft();
                    pendingSelects += msg.count - 1;

                } else if (msg.count > 0 && op == KbOp::Backspace) {
                    if (msg.preDelay > 0) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(msg.preDelay));
                    }
                    currentPostDelay = msg.postDelay;
                    selectPlan       = {};
                    pendingSelects   = 0;
                    pendingBackspaces += msg.count - 1;
                    uinput.sendBackspace();
                } else if (msg.count > 0) {
                    LotusLogger::instance().warn("Unknown keyboard message op: " + std::to_string(static_cast<int>(msg.op)));
                }
            }
        }

        // connect to mouse socket
        if ((static_cast<unsigned>(fds[kMouseServerIndex].revents) & static_cast<unsigned>(POLLIN)) != 0) {
            FdGuard candidate(accept4(mouseServerFd.get(), nullptr, nullptr, SOCK_NONBLOCK));
            if (candidate.isValid()) {
                const std::optional<pid_t> peer = authorizePeer(candidate.get(), *expectedUid, "mouse socket");

                if (peer.has_value()) {
                    LotusLogger::instance().info("New mouse flag client connected");
                    addonFd = std::move(candidate);
                }
            }
        }

        // handle mouse (libinput)
        // libinput_dispatch() may have already consumed the fd in an iteration where
        // the libinput revents were 0 (poll timeout, or another fd woke us up).  Always
        // drain the queue; libinput_get_event() returns nullptr when it is empty.
        while (LibinputEvent event{libinput_get_event(li_ctx.getLi())}) {
            switch (libinput_event_get_type(event.get())) {
                case LIBINPUT_EVENT_POINTER_BUTTON: {
                    struct libinput_event_pointer* pointer = libinput_event_get_pointer_event(event.get());
                    if (libinput_event_pointer_get_button_state(pointer) == LIBINPUT_BUTTON_STATE_PRESSED && addonFd.isValid() &&
                        send(addonFd.get(), "C", 1, MSG_NOSIGNAL | MSG_DONTWAIT) <= 0) {
                        LotusLogger::instance().warn("Failed to send to mouse flag client, closing connection");
                        addonFd.reset(-1);
                    }
                    break;
                }
                case LIBINPUT_EVENT_DEVICE_ADDED: {
                    struct libinput_device* device  = libinput_event_get_device(event.get());
                    const char* const       rawName = libinput_device_get_name(device);
                    const std::string_view  name    = rawName != nullptr ? std::string_view(rawName) : std::string_view{};
                    LotusLogger::instance().info("Device added: " + std::string(name));
                    physical_shift.trackDevice(device);
                    if (libinput_device_config_tap_get_finger_count(device) > 0) {
                        libinput_device_config_tap_set_enabled(device, LIBINPUT_CONFIG_TAP_ENABLED);
                        libinput_device_config_tap_set_button_map(device, LIBINPUT_CONFIG_TAP_MAP_LRM);
                    }
                    break;
                }
                case LIBINPUT_EVENT_DEVICE_REMOVED: {
                    physical_shift.forgetDevice(libinput_event_get_device(event.get()));
                    break;
                }
                default: break;
            }
        }
    }
    LotusLogger::instance().info("Terminating server...");
    return 0;
}
