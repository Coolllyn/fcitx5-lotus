/*
 * SPDX-FileCopyrightText: 2018 Luong Thanh Lam <ltlam93@gmail.com>
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Ky <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
/**
 * @file composition.cpp
 * @brief Implementation of the composition helpers.
 */
#include "composition.h"

#include "case_map.h"
#include "flattener.h"
#include "utils.h"

#include <algorithm>
#include <cstddef>
#include <memory>

namespace lotus {

    TransformationPtr makeAppendingTrans(char32_t key, bool isUpperCase) {
        auto trans             = std::make_shared<Transformation>();
        trans->isUpperCase     = isUpperCase;
        trans->rule.key        = key;
        trans->rule.effectOn   = key;
        trans->rule.effectType = EffectType::Appending;
        trans->rule.result     = key;
        return trans;
    }

    bool isSame(const TransformationPtr& first, const TransformationPtr& second) {
        return first == second;
    }

    Composition sliceComposition(const Composition& source, std::size_t begin, std::size_t end) {
        Composition out;
        if (end <= begin || begin >= source.size()) {
            return out;
        }
        out.reserve(end - begin);
        for (std::size_t i = begin; i < end && i < source.size(); ++i) {
            out.push_back(source[i]);
        }
        return out;
    }

    TransformationPtr findLastAppendingTrans(const Composition& composition) {
        for (auto it = composition.rbegin(); it != composition.rend(); ++it) {
            if ((*it)->rule.effectType == EffectType::Appending) {
                return *it;
            }
        }
        return nullptr;
    }

    Composition filterAppendingComposition(const Composition& composition) {
        Composition appending;
        appending.reserve(composition.size());
        for (const auto& trans : composition) {
            if (trans->rule.effectType == EffectType::Appending) {
                appending.push_back(trans);
            }
        }
        return appending;
    }

    TransformationPtr findRootTarget(const TransformationPtr& target) {
        if (target == nullptr) {
            return nullptr;
        }
        if (target->target == nullptr) {
            return target;
        }
        return findRootTarget(target->target);
    }

    TransformationPtr getLastToneTransformation(const Composition& composition) {
        for (auto it = composition.rbegin(); it != composition.rend(); ++it) {
            if ((*it)->rule.effectType == EffectType::ToneTransformation && (*it)->target != nullptr) {
                return *it;
            }
        }
        return nullptr;
    }

    bool isFree(const Composition& composition, const TransformationPtr& trans, EffectType effectType) {
        return std::none_of(composition.begin(), composition.end(),
                            [&trans, effectType](const TransformationPtr& node) { return isSame(node->target, trans) && node->rule.effectType == effectType; });
    }

    bool isIn(const TransformationPtr& target, const Composition& list) {
        return std::any_of(list.begin(), list.end(), [&target](const TransformationPtr& node) { return isSame(node, target); });
    }

    Composition breakComposition(const Composition& composition) {
        Composition result;
        result.reserve(composition.size());
        for (const auto& trans : composition) {
            if (trans->rule.key == 0) {
                continue;
            }
            result.push_back(makeAppendingTrans(trans->rule.key, trans->isUpperCase));
        }
        return result;
    }

    TransformationPtr generateAppendingTrans(const std::vector<Rule>& rules, char32_t lowerKey, bool isUpperCase) {
        for (const auto& rule : rules) {
            if (rule.key != lowerKey || rule.effectType != EffectType::Appending) {
                continue;
            }
            auto trans           = std::make_shared<Transformation>();
            trans->isUpperCase   = isUpperCase || case_map::isUpper(rule.effectOn);
            trans->rule          = rule;
            trans->rule.effectOn = case_map::toLower(rule.effectOn);
            trans->rule.result   = trans->rule.effectOn;
            return trans;
        }
        return makeAppendingTrans(lowerKey, isUpperCase);
    }

    std::tuple<Composition, Composition, Composition> extractCvcAppendingTrans(const Composition& composition) {
        const std::size_t n = composition.size();
        if (n == 0U) {
            return {Composition{}, Composition{}, Composition{}};
        }

        std::size_t lcStart = n;
        while (lcStart > 0U) {
            const auto& trans = composition[lcStart - 1U];
            if (trans != nullptr && trans->target == nullptr && utils::isVowel(trans->rule.result)) {
                break;
            }
            --lcStart;
        }

        std::size_t voStart = lcStart;
        while (voStart > 0U) {
            const auto& trans = composition[voStart - 1U];
            if (trans != nullptr && trans->target == nullptr && !utils::isVowel(trans->rule.result)) {
                break;
            }
            --voStart;
        }

        Composition firstConsonant = sliceComposition(composition, 0U, voStart);
        Composition vowel          = sliceComposition(composition, voStart, lcStart);
        Composition lastConsonant  = sliceComposition(composition, lcStart, n);

        if (!lastConsonant.empty() && vowel.empty() && firstConsonant.empty()) {
            firstConsonant = lastConsonant;
            vowel.clear();
            lastConsonant.clear();
        }

        if (firstConsonant.size() == 1U && !vowel.empty() &&
            ((firstConsonant[0]->rule.result == U'g' && vowel[0]->rule.result == U'i' && vowel.size() > 1U && (vowel[1]->rule.result != U'e' || lastConsonant.empty())) ||
             (firstConsonant[0]->rule.result == U'q' && vowel[0]->rule.result == U'u'))) {
            firstConsonant = sliceComposition(composition, 0U, voStart + 1U);
            vowel          = sliceComposition(composition, voStart + 1U, lcStart);
        }
        return {firstConsonant, vowel, lastConsonant};
    }

    std::tuple<Composition, Composition, Composition> extractCvcTrans(const Composition& composition) {
        const std::size_t n = composition.size();
        if (n == 0U) {
            return {Composition{}, Composition{}, Composition{}};
        }

        Composition appendingList;
        appendingList.reserve(n);
        for (const auto& trans : composition) {
            if (trans->target == nullptr) {
                appendingList.push_back(trans);
            }
        }

        const auto [fcApp, voApp, lcApp] = extractCvcAppendingTrans(appendingList);
        if (appendingList.size() == n) {
            return {fcApp, voApp, lcApp};
        }

        Composition fc = fcApp;
        Composition vo = voApp;
        Composition lc = lcApp;
        for (const auto& trans : composition) {
            if (trans->target == nullptr) {
                continue;
            }
            if (isIn(trans->target, voApp)) {
                vo.push_back(trans);
            } else if (isIn(trans->target, fcApp)) {
                fc.push_back(trans);
            } else if (isIn(trans->target, lcApp)) {
                lc.push_back(trans);
            }
        }
        return {fc, vo, lc};
    }

    std::pair<Composition, Composition> extractLastWord(const Composition& composition, std::u32string_view effectKeys) {
        const std::size_t n = composition.size();
        for (std::size_t index = n; index > 0U; --index) {
            const std::size_t    i = index - 1U;
            const std::u32string canvas =
                flatten(sliceComposition(composition, i, n), toMask(Mode::VietnameseMode) | toMask(Mode::LowerCase) | toMask(Mode::ToneLess) | toMask(Mode::MarkLess));
            if (canvas.empty()) {
                continue;
            }
            const char32_t codePoint = canvas.front();
            if (!utils::isAlpha(codePoint) && !utils::inKeyList(effectKeys, codePoint)) {
                if (i == n - 1U) {
                    return {composition, Composition{}};
                }
                return {sliceComposition(composition, 0U, i + 1U), sliceComposition(composition, i + 1U, n)};
            }
        }
        return {Composition{}, composition};
    }

    std::pair<Composition, Composition> extractLastWordWithPunctuationMarks(const Composition& composition) {
        const std::size_t n = composition.size();
        for (std::size_t index = n; index > 0U; --index) {
            const std::size_t    i      = index - 1U;
            const std::u32string canvas = flatten(sliceComposition(composition, i, n), toMask(Mode::EnglishMode));
            if (canvas.empty()) {
                continue;
            }
            if (utils::isSpace(canvas.front())) {
                if (i == n - 1U) {
                    return {composition, Composition{}};
                }
                return {sliceComposition(composition, 0U, i + 1U), sliceComposition(composition, i + 1U, n)};
            }
        }
        return {Composition{}, composition};
    }

} // namespace lotus
