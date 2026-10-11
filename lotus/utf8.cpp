/*
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Ky <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
/**
 * @file utf8.cpp
 * @brief Implementation of the UTF-8 <-> UTF-32 helpers.
 */
#include "lotus/utf8.h"

namespace lotus::utf8 {

    namespace {

        constexpr unsigned char kContinuationMask = 0xC0;
        constexpr unsigned char kContinuationBits = 0x80;

        bool                    isContinuation(char c) {
            return (static_cast<unsigned char>(c) & kContinuationMask) == kContinuationBits;
        }

        /// Decodes one sequence at in[index] and advances index by the number of bytes consumed.
        char32_t decodeOne(std::string_view in, std::size_t& index) {
            const auto b0        = static_cast<unsigned char>(in[index]);
            const auto available = in.size() - index;
            const auto trail     = [&](std::size_t offset) { return static_cast<char32_t>(static_cast<unsigned char>(in[index + offset]) & 0x3F); };

            if (b0 < 0x80) {
                ++index;
                return static_cast<char32_t>(b0);
            }
            if (b0 >= 0xC2 && b0 <= 0xDF && available >= 2 && isContinuation(in[index + 1])) {
                const char32_t codePoint = ((b0 & 0x1FU) << 6) | trail(1);
                index += 2;
                return codePoint;
            }
            if (b0 >= 0xE0 && b0 <= 0xEF && available >= 3 && isContinuation(in[index + 1]) && isContinuation(in[index + 2])) {
                const char32_t codePoint = ((b0 & 0x0FU) << 12) | (trail(1) << 6) | trail(2);
                if (codePoint >= 0x800 && (codePoint < 0xD800 || codePoint > 0xDFFF)) {
                    index += 3;
                    return codePoint;
                }
            }
            if (b0 >= 0xF0 && b0 <= 0xF4 && available >= 4 && isContinuation(in[index + 1]) && isContinuation(in[index + 2]) && isContinuation(in[index + 3])) {
                const char32_t codePoint = ((b0 & 0x07U) << 18) | (trail(1) << 12) | (trail(2) << 6) | trail(3);
                if (codePoint >= 0x10000 && codePoint <= 0x10FFFF) {
                    index += 4;
                    return codePoint;
                }
            }
            // Invalid sequence: consume a single byte, like Go's range over a string.
            ++index;
            return kReplacement;
        }

    } // namespace

    std::size_t length(std::string_view in) {
        std::size_t count = 0;
        for (char c : in) {
            if (!isContinuation(c)) {
                ++count;
            }
        }
        return count;
    }

    char32_t last(std::string_view in) {
        if (in.empty()) {
            return 0;
        }
        std::size_t start  = in.size();
        std::size_t walked = 0;
        while (start > 0 && walked < 3 && isContinuation(in[start - 1])) {
            --start;
            ++walked;
        }
        if (start == 0) {
            return kReplacement;
        }
        --start;
        std::size_t    index     = start;
        const char32_t codePoint = decodeOne(in, index);
        return index == in.size() ? codePoint : kReplacement;
    }

    std::u32string decode(std::string_view in) {
        std::u32string out;
        out.reserve(in.size());
        std::size_t index = 0;
        while (index < in.size()) {
            out.push_back(decodeOne(in, index));
        }
        return out;
    }

    void appendEncoded(std::string& out, char32_t codePoint) {
        if (codePoint > 0x10FFFF || (codePoint >= 0xD800 && codePoint <= 0xDFFF)) {
            // Go: string(rune) replaces out of range and surrogate halves with U+FFFD.
            codePoint = kReplacement;
        }
        if (codePoint < 0x80) {
            out.push_back(static_cast<char>(codePoint));
            return;
        }
        if (codePoint < 0x800) {
            out.push_back(static_cast<char>(0xC0U | (codePoint >> 6)));
        } else if (codePoint < 0x10000) {
            out.push_back(static_cast<char>(0xE0U | (codePoint >> 12)));
            out.push_back(static_cast<char>(0x80U | ((codePoint >> 6) & 0x3FU)));
        } else {
            out.push_back(static_cast<char>(0xF0U | (codePoint >> 18)));
            out.push_back(static_cast<char>(0x80U | ((codePoint >> 12) & 0x3FU)));
            out.push_back(static_cast<char>(0x80U | ((codePoint >> 6) & 0x3FU)));
        }
        out.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
    }

    std::string encode(std::u32string_view in) {
        std::string out;
        out.reserve(in.size());
        for (char32_t codePoint : in) {
            appendEncoded(out, codePoint);
        }
        return out;
    }

} // namespace lotus::utf8
