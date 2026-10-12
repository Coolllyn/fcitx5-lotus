/*
 * SPDX-FileCopyrightText: 2018 Luong Thanh Lam <ltlam93@gmail.com>
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Ky <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
/**
 * @file dictionary.cpp
 * @brief Implementation of the custom dictionary lookups.
 */
#include "dictionary.h"

#include "case_map.h"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace lotus {

    void Dictionary::loadFromWords(std::vector<std::string> words) {
        words.erase(std::remove_if(words.begin(), words.end(), [](const std::string& word) { return word.empty(); }), words.end());
        // Go sorts and compares strings byte-wise (sort.Strings); std::string ordering uses the same
        // byte-wise comparison, so the sort and the later binary searches stay interchangeable.
        std::sort(words.begin(), words.end());
        words.erase(std::unique(words.begin(), words.end()), words.end());
        words_ = std::move(words);
    }

    void Dictionary::loadFromText(std::string_view wholeFile) {
        std::vector<std::string> words;
        std::size_t              lineStart = 0;
        while (lineStart <= wholeFile.size()) {
            const std::size_t lineEnd = wholeFile.find('\n', lineStart);
            std::string_view  line    = wholeFile.substr(lineStart, lineEnd == std::string_view::npos ? std::string_view::npos : lineEnd - lineStart);
            if (!line.empty() && line.back() == '\r') {
                line.remove_suffix(1);
            }
            if (!line.empty()) {
                words.push_back(case_map::toLowerUtf8(line));
            }
            if (lineEnd == std::string_view::npos) {
                break;
            }
            lineStart = lineEnd + 1;
        }
        loadFromWords(std::move(words));
    }

    bool Dictionary::sortsBefore(const std::string& stored, std::string_view candidate) {
        return std::string_view(stored) < candidate;
    }

    bool Dictionary::hasWord(std::string_view word) const {
        if (word.empty()) {
            return false;
        }
        const auto found = std::lower_bound(words_.begin(), words_.end(), word, sortsBefore);
        return found != words_.end() && std::string_view(*found) == word;
    }

    bool Dictionary::hasWordOrPrefix(std::string_view word) const {
        if (word.empty()) {
            return false;
        }
        const auto found = std::lower_bound(words_.begin(), words_.end(), word, sortsBefore);
        // substr() clamps to the stored word, so a longer candidate is not a prefix (Go's
        // strings.HasPrefix).
        return found != words_.end() && std::string_view(*found).substr(0, word.size()) == word;
    }

    const std::vector<std::string>& Dictionary::words() const {
        return words_;
    }

} // namespace lotus
