/*
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

    bool MacroTable::get(std::string_view keyUtf8, std::string& out) const {
        out.clear();
        if (keyUtf8.empty() || empty()) {
            return false;
        }
        const auto found = table_.find(case_map::toLowerUtf8(keyUtf8));
        if (found == table_.end()) {
            return false;
        }
        out = found->second;
        return true;
    }

    void MacroTable::set(std::string_view keyUtf8, std::string_view valueUtf8) {
        table_.insert_or_assign(std::string(keyUtf8), std::string(valueUtf8));
    }

} // namespace lotus
