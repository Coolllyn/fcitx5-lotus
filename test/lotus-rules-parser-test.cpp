// SPDX-FileCopyrightText: 2026 Nguyen Hoang Ky <nhktmdzhg@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ports bamboo-core/rules_parser_test.go 1:1 (same lines, same expectations, including the rule
// counts and the index based assertions) plus direct cases for the hand written DSL matchers.
// The Go test TestParseRulesWithIm is empty and therefore has nothing to port.
#include "input_method.h"
#include "rules_parser.h"

#include <cstddef>
#include <cstdio>
#include <optional>
#include <string>

namespace {

    using lotus::EffectType;
    using lotus::Mark;
    using lotus::Rule;
    using lotus::Tone;
    using lotus::getAppendingRule;
    using lotus::matchAppendingLetters;
    using lotus::matchDsl;
    using lotus::parseRules;
    using lotus::parseTonelessRules;

    int  failures = 0;

    void check(const char* name, bool condition) {
        if (!condition) {
            std::printf("FAIL %s\n", name);
            ++failures;
        }
    }

    void checkMarkRule(const char* name, const std::vector<Rule>& rules, std::size_t index, char32_t key, Mark mark, char32_t effectOn) {
        const bool ok = index < rules.size() && rules[index].key == key && rules[index].effectType == EffectType::MarkTransformation && rules[index].getMark() == mark &&
            rules[index].effectOn == effectOn;
        if (!ok) {
            std::printf("FAIL %s (index %zu)\n", name, index);
            ++failures;
        }
    }

    void checkAppendRule(const char* name, const Rule& rule, char32_t key, char32_t effectOn) {
        const bool ok = rule.key == key && rule.effectType == EffectType::Appending && rule.effectOn == effectOn;
        if (!ok) {
            std::printf("FAIL %s\n", name);
            ++failures;
        }
    }

} // namespace

int main() {
    // TestParseToneRules
    {
        const auto rules = parseRules(U'z', "XoaDauThanh");
        check("tone rule count", rules.size() == 1U);
        check("tone rule type", !rules.empty() && rules[0].effectType == EffectType::ToneTransformation);
        check("tone rule none", !rules.empty() && rules[0].getTone() == Tone::None);

        const auto tilde = parseRules(U'x', "DauNga");
        check("tilde rule count", tilde.size() == 1U);
        check("tilde rule type", !tilde.empty() && tilde[0].effectType == EffectType::ToneTransformation);
        check("tilde rule tone", !tilde.empty() && tilde[0].getTone() == Tone::Tilde);
    }

    // TestParseTonelessRules
    {
        const auto dash = parseTonelessRules(U'd', "D_Đ");
        check("D_Đ rule count", dash.size() == 2U);
        checkMarkRule("D_Đ first rule", dash, 0U, U'd', Mark::Dash, U'd');

        const auto appending = parseTonelessRules(U'{', "_Ư");
        check("_Ư rule count", appending.size() == 1U);
        check("_Ư appending rule", !appending.empty() && appending[0].effectType == EffectType::Appending && appending[0].effectOn == U'Ư');

        const auto word = parseTonelessRules(U'w', "UOA_ƯƠĂ");
        check("UOA_ƯƠĂ rule count", word.size() == 33U);
        checkMarkRule("UOA_ƯƠĂ index 0", word, 0U, U'w', Mark::Horn, U'u');
        checkMarkRule("UOA_ƯƠĂ index 7", word, 7U, U'w', Mark::Horn, U'o');
        checkMarkRule("UOA_ƯƠĂ index 20", word, 20U, U'w', Mark::Breve, U'a');

        const auto withAppending = parseTonelessRules(U'w', "UOA_ƯƠĂ__Ư");
        check("UOA_ƯƠĂ__Ư rule count", withAppending.size() == 34U);
        checkMarkRule("UOA_ƯƠĂ__Ư index 20", withAppending, 20U, U'w', Mark::Breve, U'a');
        if (withAppending.size() > 33U) {
            checkAppendRule("UOA_ƯƠĂ__Ư index 33", withAppending[33], U'w', U'ư');
        } else {
            check("UOA_ƯƠĂ__Ư index 33", false);
        }
    }

    // TestAppendRule
    {
        const auto lower = parseTonelessRules(U'[', "__ươ");
        check("__ươ rule count", lower.size() == 1U);
        check("__ươ appended count", !lower.empty() && lower[0].appendedRules.size() == 1U);
        if (!lower.empty() && lower[0].appendedRules.size() == 1U) {
            checkAppendRule("__ươ appended rule", lower[0].appendedRules[0], U'[', U'ơ');
        }

        const auto upper = parseTonelessRules(U'{', "__ƯƠ");
        check("__ƯƠ rule count", upper.size() == 1U);
        check("__ƯƠ appended count", !upper.empty() && upper[0].appendedRules.size() == 1U);
        if (!upper.empty() && upper[0].appendedRules.size() == 1U) {
            checkAppendRule("__ƯƠ appended rule", upper[0].appendedRules[0], U'{', U'Ơ');
        }
    }

    // Hand written DSL matchers (replacements for the two Go regular expressions).
    {
        const auto simple = matchDsl("a_â");
        check("matchDsl a_â", simple.has_value() && simple->effectiveOns == "a" && simple->results == "â" && simple->appended.empty());

        const auto upper = matchDsl("UOA_ƯƠĂ");
        check("matchDsl UOA_ƯƠĂ", upper.has_value() && upper->effectiveOns == "UOA" && upper->results == "ƯƠĂ" && upper->appended.empty());

        const auto withAppended = matchDsl("uoa_ươă__ư");
        check("matchDsl with appended", withAppended.has_value() && withAppended->results == "ươă" && withAppended->appended == "__ư");

        const auto aeO = matchDsl("AEO_ÂÊÔ");
        check("matchDsl AEO_ÂÊÔ", aeO.has_value() && aeO->effectiveOns == "AEO" && aeO->results == "ÂÊÔ");

        const auto d = matchDsl("D_Đ");
        check("matchDsl D_Đ", d.has_value() && d->effectiveOns == "D" && d->results == "Đ");

        check("matchDsl __ă", !matchDsl("__ă").has_value());
        check("matchDsl empty", !matchDsl("").has_value());
        check("matchDsl no underscore", !matchDsl("abc").has_value());

        const auto appended = matchAppendingLetters("__ă");
        check("matchAppendingLetters __ă", appended.has_value() && *appended == "ă");
        const auto single = matchAppendingLetters("_ă");
        check("matchAppendingLetters _ă", single.has_value() && *single == "ă");
        check("matchAppendingLetters abc", !matchAppendingLetters("abc").has_value());
    }

    // parseInputMethod: an unknown name yields the default input method, like Go's ParseInputMethod.
    {
        const auto unknown = lotus::parseInputMethod("NonExistent");
        check("unknown input method", unknown.name.empty() && unknown.rules.empty() && unknown.keys.empty());

        const auto telex = lotus::parseInputMethod("Telex");
        check("telex name", telex.name == "Telex");
        check("telex keys", telex.keys.size() == 11U);
        check("telex rules", !telex.rules.empty());
    }

    if (failures != 0) {
        std::printf("%d failure(s)\n", failures);
        return 1;
    }
    std::printf("lotus rules parser: all checks passed\n");
    return 0;
}
