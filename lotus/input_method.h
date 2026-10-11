/*
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Ky <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
/**
 * @file input_method.h
 * @brief Builds an InputMethod from the generated definitions, ported 1:1 from
 *        bamboo-core/input_method_def.go (ParseInputMethod).
 */

#ifndef _LOTUS_INPUT_METHOD_H_
#define _LOTUS_INPUT_METHOD_H_

#include "rule.h"

#include <string_view>

namespace lotus {

    /**
     * @brief Builds the input method named by name, like Go's ParseInputMethod.
     * @param name Definition name ("Telex", "VNI", "VIQR", ...).
     * @return Parsed input method; a default (empty name, no rules) InputMethod when unknown.
     */
    InputMethod parseInputMethod(std::string_view name);

} // namespace lotus

#endif // _LOTUS_INPUT_METHOD_H_
