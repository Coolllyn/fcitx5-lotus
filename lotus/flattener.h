/*
 * SPDX-FileCopyrightText: 2018 Luong Thanh Lam <ltlam93@gmail.com>
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Ky <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
/**
 * @file flattener.h
 * @brief Renders a composition into code points, ported 1:1 from bamboo-core/flattener.go.
 */

#ifndef _LOTUS_FLATTENER_H_
#define _LOTUS_FLATTENER_H_

#include "composition.h"

#include <string>

namespace lotus {

    /**
     * @brief Renders a composition into code points, like Go's getCanvas / Flatten.
     * @param composition Composition.
     * @param mode Flattening mode (English, ToneLess, MarkLess, LowerCase, ...).
     * @return Rendered code points. Go's Flatten would then encode this canvas to UTF-8.
     */
    std::u32string flatten(const Composition& composition, ModeSet mode);

} // namespace lotus

#endif // _LOTUS_FLATTENER_H_
