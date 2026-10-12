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
#include <unordered_map>

namespace lotus {

    std::u32string flatten(const Composition& composition, uint32_t mode) {
        std::unordered_map<TransformationPtr, Composition> appendingMap;
        Composition                                        appendingList;

        for (const auto& trans : composition) {
            const bool isEnglish   = (mode & toMask(Mode::EnglishMode)) != 0U;
            const bool isAppending = trans->rule.effectType == EffectType::Appending;
            if (isEnglish || isAppending) {
                if (trans->rule.key == 0) {
                    continue;
                }
                appendingList.push_back(trans);
            } else if (trans->target != nullptr) {
                appendingMap[trans->target].push_back(trans);
            }
        }

        std::u32string canvas;
        canvas.reserve(appendingList.size());
        for (const auto& appendingTrans : appendingList) {
            char32_t   char32 = 0;
            const auto found  = appendingMap.find(appendingTrans);
            if ((mode & toMask(Mode::EnglishMode)) != 0U) {
                char32 = appendingTrans->rule.key;
            } else {
                char32 = appendingTrans->rule.effectOn;
                if (found != appendingMap.end()) {
                    for (const auto& trans : found->second) {
                        if (trans->rule.effectType == EffectType::MarkTransformation) {
                            if (trans->rule.mark() == Mark::Raw) {
                                char32 = appendingTrans->rule.key;
                            } else {
                                char32 = utils::addMarkToChar(char32, trans->rule.mark());
                            }
                        } else if (trans->rule.effectType == EffectType::ToneTransformation) {
                            char32 = utils::addToneToChar(char32, trans->rule.tone());
                        }
                    }
                }
            }
            if ((mode & toMask(Mode::ToneLess)) != 0U) {
                char32 = utils::addToneToChar(char32, Tone::None);
            }
            if ((mode & toMask(Mode::MarkLess)) != 0U) {
                char32 = utils::addMarkToChar(char32, Mark::None);
            }
            if ((mode & toMask(Mode::LowerCase)) != 0U) {
                char32 = case_map::toLower(char32);
            } else if (appendingTrans->isUpperCase) {
                char32 = case_map::toUpper(char32);
            }
            canvas.push_back(char32);
        }
        return canvas;
    }

} // namespace lotus
