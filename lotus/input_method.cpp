/*
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Ky <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
/**
 * @file input_method.cpp
 * @brief Implementation of the input method builder.
 */
#include "input_method.h"

#include "case_map.h"
#include "data/input_method_data.h"
#include "rules_parser.h"
#include "utf8.h"

#include <algorithm>
#include <array>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace lotus {

    InputMethod parseInputMethod(const std::unordered_map<std::string, std::string>& definition, std::string_view name) {
        InputMethod method;
        method.name = name;

        // Go iterates a map here, so the order of the definition keys is random; only the
        // key -> rules association matters, and the keys/toneKeys/appendingKeys lists are used
        // for membership tests. Sort the keys to keep the result deterministic.
        std::vector<std::pair<std::string, std::string>> entries(definition.begin(), definition.end());
        std::sort(entries.begin(), entries.end());

        for (const auto& entry : entries) {
            const std::u32string keys = utf8::decode(entry.first);
            if (keys.empty()) {
                continue;
            }
            const auto parsed = parseRules(keys.front(), entry.second);
            method.rules.insert(method.rules.end(), parsed.begin(), parsed.end());
            if (case_map::toLowerUtf8(entry.second).find("uo") != std::string::npos) {
                method.superKeys.push_back(keys.front());
            }
            method.keys.push_back(keys.front());
        }

        for (const auto& rule : method.rules) {
            if (rule.effectType == EffectType::Appending) {
                method.appendingKeys.push_back(rule.key);
            }
            if (rule.effectType == EffectType::ToneTransformation) {
                method.toneKeys.push_back(rule.key);
            }
        }
        return method;
    }

    InputMethod parseInputMethod(std::string_view name) {
        const auto& definitions = data::inputMethodDefinitions();
        const auto  definition  = definitions.find(std::string(name));
        if (definition == definitions.end()) {
            return InputMethod{};
        }
        return parseInputMethod(definition->second, name);
    }

    std::vector<std::string> inputMethodNames() {
        // Go's order table (bamboo/bamboo-c.go:279-287): the definitions live in a map, so the menu
        // order has to be spelled out instead of derived from the data.
        static constexpr std::array<std::string_view, 7> kOrder = {
            "Telex", "VNI", "Telex + VNI", "Telex + VNI + VIQR", "VIQR", "Microsoft layout", "VNI Bàn phím tiếng Pháp",
        };
        std::vector<std::string> names;
        names.reserve(kOrder.size());
        for (const std::string_view name : kOrder) {
            names.emplace_back(name);
        }
        return names;
    }

} // namespace lotus
