/*
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Ky <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
/**
 * @file rule.h
 * @brief Rule and input method models, 1:1 with Go's Rule and InputMethod.
 */

#ifndef _LOTUS_RULE_H_
#define _LOTUS_RULE_H_

#include "types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace lotus {

    /** @brief One transformation rule, 1:1 with Go's Rule. */
    struct Rule {
        char32_t          key        = 0;                     // Input key that produced the rule
        uint8_t           effect     = 0;                     // Tone or mark value, depending on effectType
        EffectType        effectType = EffectType::Appending; // Kind of transformation
        char32_t          effectOn   = 0;                     // Character the effect applies to
        char32_t          result     = 0;                     // Character the rule produces
        std::vector<Rule> appendedRules;                      // Extra appending rules of the same key

        /** @brief Stores a tone in effect, like Go's Rule.SetTone. */
        constexpr void setTone(Tone tone) {
            effect = static_cast<uint8_t>(tone);
        }

        /** @brief Stores a mark in effect, like Go's Rule.SetMark. */
        constexpr void setMark(Mark mark) {
            effect = static_cast<uint8_t>(mark);
        }

        /** @brief Reads effect as a tone, like Go's Rule.GetTone. */
        [[nodiscard]] constexpr Tone getTone() const {
            return static_cast<Tone>(effect);
        }

        /** @brief Reads effect as a mark, like Go's Rule.GetMark. */
        [[nodiscard]] constexpr Mark getMark() const {
            return static_cast<Mark>(effect);
        }
    };

    /** @brief One input method with its parsed rules, 1:1 with Go's InputMethod. */
    struct InputMethod {
        std::string           name;          // Definition name, empty for an unknown name
        std::vector<Rule>     rules;         // Rules of every definition line, sorted by key
        std::vector<char32_t> superKeys;     // Keys whose line mentions "uo"
        std::vector<char32_t> toneKeys;      // Keys that produce tone rules
        std::vector<char32_t> appendingKeys; // Keys that produce appending rules
        std::vector<char32_t> keys;          // Every key of the definition
    };

} // namespace lotus

#endif // _LOTUS_RULE_H_
