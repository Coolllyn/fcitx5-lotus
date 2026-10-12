/*
 * SPDX-FileCopyrightText: 2018 Luong Thanh Lam <ltlam93@gmail.com>
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Ky <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
/**
 * @file case_map.cpp
 * @brief Implementation of the Unicode case helpers.
 */
#include "case_map.h"

#include "data/case_data.h"
#include "utf8.h"

#include <algorithm>
#include <utility>

namespace lotus::case_map {

    char32_t lookupCase(const std::vector<std::pair<char32_t, char32_t>>& table, char32_t key) {
        const auto it = std::lower_bound(table.begin(), table.end(), key, [](const std::pair<char32_t, char32_t>& entry, char32_t value) { return entry.first < value; });
        return (it != table.end() && it->first == key) ? it->second : key;
    }

    char32_t toLower(char32_t codePoint) {
        return lookupCase(data::upperLowerPairs(), codePoint);
    }

    char32_t toUpper(char32_t codePoint) {
        return lookupCase(data::lowerUpperPairs(), codePoint);
    }

    bool isUpper(char32_t codePoint) {
        return toLower(codePoint) != codePoint;
    }

    bool isLower(char32_t codePoint) {
        return toUpper(codePoint) != codePoint;
    }

    bool isLetter(char32_t codePoint) {
        const std::u32string_view letters = data::letters();
        return std::binary_search(letters.begin(), letters.end(), codePoint);
    }

    std::string toLowerUtf8(std::string_view utf8) {
        std::u32string text = lotus::utf8::decode(utf8);
        for (char32_t& codePoint : text) {
            codePoint = toLower(codePoint);
        }
        return lotus::utf8::encode(text);
    }

    std::string toUpperUtf8(std::string_view utf8) {
        std::u32string text = lotus::utf8::decode(utf8);
        for (char32_t& codePoint : text) {
            codePoint = toUpper(codePoint);
        }
        return lotus::utf8::encode(text);
    }

} // namespace lotus::case_map
