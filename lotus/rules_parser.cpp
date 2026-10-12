/*
 * SPDX-FileCopyrightText: 2018 Luong Thanh Lam <ltlam93@gmail.com>
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Ky <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
/**
 * @file rules_parser.cpp
 * @brief Implementation of the definition DSL parser.
 */
#include "rules_parser.h"

#include "case_map.h"
#include "utf8.h"
#include "utils.h"

#include <array>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace lotus {

    // Tone order used when expanding a mark rule, 1:1 with Go's ParseToneLessRule.
    constexpr std::array<Tone, 6> kToneOrder = {Tone::None, Tone::Dot, Tone::Acute, Tone::Grave, Tone::Hook, Tone::Tilde};

    // Definition lines naming a tone, 1:1 with Go's tones map.
    constexpr std::array<std::pair<std::string_view, Tone>, 6> kToneNames = {
        std::pair<std::string_view, Tone>{"XoaDauThanh", Tone::None},
        {"DauSac", Tone::Acute},
        {"DauHuyen", Tone::Grave},
        {"DauNga", Tone::Tilde},
        {"DauNang", Tone::Dot},
        {"DauHoi", Tone::Hook},
    };

    std::optional<DslMatch> matchDsl(std::string_view line) {
        const std::u32string text = utf8::decode(line);
        for (std::size_t start = 0; start < text.size(); ++start) {
            std::size_t       index      = start;
            const std::size_t asciiStart = index;
            while (index < text.size() && utils::isAlpha(text[index])) {
                ++index;
            }
            const std::size_t asciiEnd = index;
            if (asciiEnd == asciiStart || index >= text.size() || text[index] != U'_') {
                continue;
            }
            ++index;
            const std::size_t lettersStart = index;
            while (index < text.size() && case_map::isLetter(text[index])) {
                ++index;
            }
            const std::size_t lettersEnd = index;
            if (lettersEnd == lettersStart) {
                continue;
            }
            const std::size_t appendedStart = index;
            while (index < text.size() && (text[index] == U'_' || case_map::isLetter(text[index]))) {
                ++index;
            }
            DslMatch match;
            match.effectiveOns = utf8::encode(text.substr(asciiStart, asciiEnd - asciiStart));
            match.results      = utf8::encode(text.substr(lettersStart, lettersEnd - lettersStart));
            match.appended     = utf8::encode(text.substr(appendedStart, index - appendedStart));
            return match;
        }
        return std::nullopt;
    }

    std::optional<std::string> matchAppendingLetters(std::string_view value) {
        const std::u32string text = utf8::decode(value);
        for (std::size_t start = 0; start < text.size(); ++start) {
            // `_?` is greedy: the one underscore alternative is tried first, like RE2.
            for (int optionalUnderscore = 1; optionalUnderscore >= 0; --optionalUnderscore) {
                std::size_t index = start;
                if (optionalUnderscore == 1) {
                    if (index >= text.size() || text[index] != U'_') {
                        continue;
                    }
                    ++index;
                }
                if (index >= text.size() || text[index] != U'_') {
                    continue;
                }
                ++index;
                const std::size_t lettersStart = index;
                while (index < text.size() && case_map::isLetter(text[index])) {
                    ++index;
                }
                if (index == lettersStart) {
                    continue;
                }
                return utf8::encode(text.substr(lettersStart, index - lettersStart));
            }
        }
        return std::nullopt;
    }

    std::optional<Tone> toneFromName(std::string_view name) {
        for (const auto& entry : kToneNames) {
            if (entry.first == name) {
                return entry.second;
            }
        }
        return std::nullopt;
    }

    std::vector<Rule> parseRules(char32_t key, std::string_view line) {
        if (const auto tone = toneFromName(line); tone.has_value()) {
            Rule rule;
            rule.key        = key;
            rule.effectType = EffectType::ToneTransformation;
            rule.effect     = static_cast<uint8_t>(tone.value());
            return {rule};
        }
        return parseTonelessRules(key, line);
    }

    std::vector<Rule> parseTonelessRules(char32_t key, std::string_view line) {
        std::vector<Rule> rules;
        if (matchDsl(line).has_value()) {
            // Go matches the original line but captures the groups from the lowercased one.
            const auto parts = matchDsl(case_map::toLowerUtf8(line));
            if (!parts.has_value()) {
                return rules;
            }
            const std::u32string effectiveOns = utf8::decode(parts->effectiveOns);
            const std::u32string results      = utf8::decode(parts->results);
            for (std::size_t i = 0; i < effectiveOns.size() && i < results.size(); ++i) {
                Mark effect = Mark::None;
                if (!utils::findMarkFromChar(results[i], effect)) {
                    continue;
                }
                const auto markRules = parseToneLessRule(key, effectiveOns[i], results[i], effect);
                rules.insert(rules.end(), markRules.begin(), markRules.end());
            }
            if (const auto appending = getAppendingRule(key, parts->appended); appending.has_value()) {
                rules.push_back(appending.value());
            }
        } else if (const auto appending = getAppendingRule(key, line); appending.has_value()) {
            rules.push_back(appending.value());
        }
        return rules;
    }

    std::vector<Rule> parseToneLessRule(char32_t key, char32_t effectiveOn, char32_t result, Mark effect) {
        std::vector<Rule> rules;
        for (char32_t codePoint : utils::getMarkFamily(effectiveOn)) {
            if (codePoint == result) {
                Rule rule;
                rule.key        = key;
                rule.effectType = EffectType::MarkTransformation;
                rule.effect     = 0;
                rule.effectOn   = result;
                rule.result     = effectiveOn;
                rules.push_back(rule);
            } else if (utils::isVowel(codePoint)) {
                for (Tone tone : kToneOrder) {
                    Rule rule;
                    rule.key        = key;
                    rule.effectType = EffectType::MarkTransformation;
                    rule.effectOn   = utils::addToneToChar(codePoint, static_cast<uint8_t>(tone));
                    rule.effect     = static_cast<uint8_t>(effect);
                    rule.result     = utils::addToneToChar(result, static_cast<uint8_t>(tone));
                    rules.push_back(rule);
                }
            } else {
                Rule rule;
                rule.key        = key;
                rule.effectType = EffectType::MarkTransformation;
                rule.effectOn   = codePoint;
                rule.effect     = static_cast<uint8_t>(effect);
                rule.result     = result;
                rules.push_back(rule);
            }
        }
        return rules;
    }

    std::optional<Rule> getAppendingRule(char32_t key, std::string_view value) {
        const auto letters = matchAppendingLetters(value);
        if (!letters.has_value()) {
            return std::nullopt;
        }
        const std::u32string chars = utf8::decode(letters.value());
        if (chars.empty()) {
            return std::nullopt;
        }
        Rule rule;
        rule.key        = key;
        rule.effectType = EffectType::Appending;
        rule.effectOn   = chars.front();
        rule.result     = chars.front();
        for (std::size_t i = 1; i < chars.size(); ++i) {
            Rule appended;
            appended.key        = key;
            appended.effectType = EffectType::Appending;
            appended.effectOn   = chars[i];
            appended.result     = chars[i];
            rule.appendedRules.push_back(appended);
        }
        return rule;
    }

} // namespace lotus
