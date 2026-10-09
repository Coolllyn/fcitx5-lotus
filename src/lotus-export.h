/*
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Ky <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

/**
 * @file lotus-export.h
 * @brief Dependency-free exported-API marker (single LOTUS_EXPORT macro).
 */

#ifndef _LOTUS_EXPORT_H_
#define _LOTUS_EXPORT_H_

// Exported API marker: the project builds with hidden visibility, but the
// headless tests link shared symbols from lotus_test_core. Keep this header
// dependency-free, like fcitx5's fcitx-utils/macros.h.
#if defined(__GNUC__) || defined(__clang__)
#define LOTUS_EXPORT __attribute__((visibility("default")))
#else
#define LOTUS_EXPORT
#endif

#endif // _LOTUS_EXPORT_H_
