/*
 * SPDX-FileCopyrightText: 2018 Luong Thanh Lam <ltlam93@gmail.com>
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Ky <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
/**
 * @file flattener.cpp
 * @brief Implementation of the composition renderer.
 */
#include "flattener.h"

#include "case_map.h"
#include "utils.h"

#include <string>

namespace lotus {

    std::u32string flatten(const Composition& composition, ModeSet mode) {
        Composition effects;
        Composition appendingList;
        appendingList.reserve(composition.size());
        const bool isEnglish = mode.has(Mode::EnglishMode);
        for (const auto& trans : composition) {
            const bool isAppending = trans->rule.effectType == EffectType::Appending;
            if (isEnglish || isAppending) {
                if (trans->rule.key == 0) {
                    continue;
                }
                appendingList.push_back(trans);
            } else if (trans->target != nullptr) {
                effects.push_back(trans);
            }
        }

        std::u32string canvas;
        canvas.reserve(appendingList.size());
        for (const auto& appendingTrans : appendingList) {
            char32_t char32 = 0;
            if (isEnglish) {
                char32 = appendingTrans->rule.key;
            } else {
                char32 = appendingTrans->rule.effectOn;
                for (const auto& effect : effects) {
                    if (effect->target == appendingTrans) {
                        if (effect->rule.effectType == EffectType::MarkTransformation) {
                            if (effect->rule.mark() == Mark::Raw) {
                                char32 = appendingTrans->rule.key;
                            } else {
                                char32 = utils::addMarkToChar(char32, effect->rule.mark());
                            }
                        } else if (effect->rule.effectType == EffectType::ToneTransformation) {
                            char32 = utils::addToneToChar(char32, effect->rule.tone());
                        }
                    }
                }
            }
            if (mode.has(Mode::ToneLess)) {
                char32 = utils::addToneToChar(char32, Tone::None);
            }
            if (mode.has(Mode::MarkLess)) {
                char32 = utils::addMarkToChar(char32, Mark::None);
            }
            if (mode.has(Mode::LowerCase)) {
                char32 = case_map::toLower(char32);
            } else if (appendingTrans->isUpperCase) {
                char32 = case_map::toUpper(char32);
            }
            canvas.push_back(char32);
        }
        return canvas;
    }

} // namespace lotus
