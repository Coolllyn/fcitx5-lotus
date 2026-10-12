/*
 * SPDX-FileCopyrightText: 2018 Luong Thanh Lam <ltlam93@gmail.com>
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Ky <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
/**
 * @file macro_table.cpp
 * @brief Implementation of the text macro expansion table.
 */
#include "macro_table.h"

#include "case_map.h"

namespace lotus {

    bool MacroTable::empty() const {
        return table_.empty();
    }

    std::optional<std::string_view> MacroTable::get(std::string_view keyUtf8) const {
        if (keyUtf8.empty() || empty()) {
            return std::nullopt;
        }
        const auto found = table_.find(case_map::toLowerUtf8(keyUtf8));
        if (found == table_.end()) {
            return std::nullopt;
        }
        return found->second;
    }

    void MacroTable::set(std::string_view keyUtf8, std::string_view valueUtf8) {
        table_.insert_or_assign(std::string(keyUtf8), std::string(valueUtf8));
    }

} // namespace lotus
