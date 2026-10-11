/*
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Ky <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
/**
 * @file utils.cpp
 * @brief Implementation of the character tables and primitives.
 */
#include "utils.h"

#include "case_map.h"
#include "data/vowel_data.h"

#include <algorithm>
#include <optional>
#include <utility>

namespace lotus::utils {

    std::u32string_view vowels() {
        return data::vowels();
    }

    bool isVowel(char32_t codePoint) {
        const std::u32string_view table = data::vowels();
        return std::find(table.begin(), table.end(), codePoint) != table.end();
    }

    int findVowelPosition(char32_t codePoint) {
        const std::u32string_view table = data::vowels();
        const auto* const         it    = std::find(table.begin(), table.end(), codePoint);
        return it == table.end() ? -1 : static_cast<int>(std::distance(table.begin(), it));
    }

    std::optional<std::u32string_view> findMarkFamily(char32_t codePoint) {
        for (const auto& family : data::markFamilies()) {
            if (family.first == codePoint) {
                return family.second;
            }
        }
        return std::nullopt;
    }

    std::vector<char32_t> getMarkFamily(char32_t codePoint) {
        std::vector<char32_t> result;
        const auto            family = findMarkFamily(codePoint);
        if (!family.has_value()) {
            return result;
        }
        for (char32_t codePointInFamily : *family) {
            if (codePointInFamily != U'_') {
                result.push_back(codePointInFamily);
            }
        }
        return result;
    }

    int findMarkPosition(char32_t codePoint) {
        const auto family = findMarkFamily(codePoint);
        if (!family.has_value()) {
            return -1;
        }
        const auto* const it = std::find(family->begin(), family->end(), codePoint);
        return it == family->end() ? -1 : static_cast<int>(std::distance(family->begin(), it));
    }

    bool findMarkFromChar(char32_t codePoint, Mark& mark) {
        const int position = findMarkPosition(codePoint);
        if (position < 0) {
            return false;
        }
        mark = static_cast<Mark>(position);
        return true;
    }

    char32_t addMarkToTonelessChar(char32_t codePoint, uint8_t mark) {
        const auto family = findMarkFamily(codePoint);
        if (!family.has_value() || static_cast<std::size_t>(mark) >= family->size() || (*family)[mark] == U'_') {
            return codePoint;
        }
        return (*family)[mark];
    }

    char32_t addToneToChar(char32_t codePoint, uint8_t tone) {
        const int position = findVowelPosition(codePoint);
        if (position < 0) {
            return codePoint;
        }
        const int                 target = position + static_cast<int>(tone) - (position % 6);
        const std::u32string_view table  = data::vowels();
        if (target < 0 || static_cast<std::size_t>(target) >= table.size()) {
            // Go indexes the vowel table without a bound check here; stay inside the table instead
            // of reading out of bounds.
            return codePoint;
        }
        return table[target];
    }

    char32_t addMarkToChar(char32_t codePoint, uint8_t mark) {
        const Tone tone = findToneFromChar(codePoint);
        char32_t   bare = addToneToChar(codePoint, 0);
        bare            = addMarkToTonelessChar(bare, mark);
        return addToneToChar(bare, static_cast<uint8_t>(tone));
    }

    Tone findToneFromChar(char32_t codePoint) {
        const int position = findVowelPosition(codePoint);
        if (position < 0) {
            return Tone::None;
        }
        return static_cast<Tone>(position % 6);
    }

    bool isPunctuationMark(char32_t codePoint) {
        const std::vector<char32_t>& marks = data::punctuationMarks();
        return std::find(marks.begin(), marks.end(), codePoint) != marks.end();
    }

    bool isWordBreakSymbol(char32_t codePoint) {
        return isPunctuationMark(codePoint) || (codePoint >= U'0' && codePoint <= U'9');
    }

    bool isAlpha(char32_t codePoint) {
        return (codePoint >= U'a' && codePoint <= U'z') || (codePoint >= U'A' && codePoint <= U'Z');
    }

    bool isSpace(char32_t codePoint) {
        return codePoint == U' ';
    }

    bool inKeyList(std::u32string_view keys, char32_t key) {
        return std::find(keys.begin(), keys.end(), key) != keys.end();
    }

    bool canProcessKey(char32_t lowerKey, std::u32string_view effectKeys) {
        if (isAlpha(lowerKey) || inKeyList(effectKeys, lowerKey)) {
            return true;
        }
        if (isWordBreakSymbol(lowerKey)) {
            return false;
        }
        return isVietnameseRune(lowerKey);
    }

    bool isVietnameseRune(char32_t lowerKey) {
        if (findToneFromChar(lowerKey) != Tone::None) {
            return true;
        }
        return lowerKey != addMarkToTonelessChar(lowerKey, 0);
    }

    bool hasAnyVietnameseRune(std::u32string_view word) {
        return std::any_of(word.begin(), word.end(), [](char32_t codePoint) { return isVietnameseRune(case_map::toLower(codePoint)); });
    }

    bool hasAnyVietnameseVower(std::u32string_view word) {
        return std::any_of(word.begin(), word.end(), [](char32_t codePoint) { return isVowel(case_map::toLower(codePoint)); });
    }

} // namespace lotus::utils
