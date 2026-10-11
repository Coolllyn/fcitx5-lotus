/*
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Ky <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
/**
 * @file types.h
 * @brief Enumerations and bit flags shared by the engine, 1:1 with the Go package bamboo.
 */

#ifndef _LOTUS_TYPES_H_
#define _LOTUS_TYPES_H_

#include <cstdint>

namespace lotus {

    /** @brief Typing modes (bit flags), 1:1 with Go's Mode. */
    enum class Mode : uint8_t {
        VietnameseMode  = 1U << 0U,
        EnglishMode     = 1U << 1U,
        ToneLess        = 1U << 2U,
        MarkLess        = 1U << 3U,
        LowerCase       = 1U << 4U,
        FullText        = 1U << 5U,
        PunctuationMode = 1U << 6U,
        InReverseOrder  = 1U << 7U,
    };

    /** @brief Engine flags (bit flags), 1:1 with Go's E* constants. */
    inline constexpr uint32_t kEfreeToneMarking         = 1U << 0U;
    inline constexpr uint32_t kEstdToneStyle            = 1U << 1U;
    inline constexpr uint32_t kEautoCorrectEnabled      = 1U << 2U;
    inline constexpr uint32_t kEw2uEnabled              = 1U << 3U;
    inline constexpr uint32_t kEbracketTransformEnabled = 1U << 4U;
    inline constexpr uint32_t kEstdFlags                = kEfreeToneMarking | kEstdToneStyle | kEautoCorrectEnabled | kEw2uEnabled;

    /** @brief "w" to "ư" mode values, 1:1 with Go's W2u* constants (-1 follows the engine flags). */
    inline constexpr int kW2uFollowFlags = -1;
    inline constexpr int kW2uDisabled    = 0;
    inline constexpr int kW2uNonStart    = 1;
    inline constexpr int kW2uEverywhere  = 2;

    /** @brief Bracket transform mode values, 1:1 with Go's BracketTransform* constants. */
    inline constexpr int kBracketTransformFollowFlags = -1;
    inline constexpr int kBracketTransformDisabled    = 0;
    inline constexpr int kBracketTransformNonStart    = 1;
    inline constexpr int kBracketTransformEverywhere  = 2;

    /** @brief Rule effect types, 1:1 with Go's EffectType. */
    enum class EffectType : uint8_t {
        Appending          = 0,
        MarkTransformation = 1,
        ToneTransformation = 2,
        Replacing          = 3,
    };

    /** @brief Marks, 1:1 with Go's Mark; the value is also the index inside a mark family row. */
    enum class Mark : uint8_t {
        None  = 0,
        Hat   = 1,
        Breve = 2,
        Horn  = 3,
        Dash  = 4,
        Raw   = 5,
    };

    /** @brief Tones, 1:1 with Go's Tone; the value is also the index inside a vowel tone group. */
    enum class Tone : uint8_t {
        None  = 0,
        Grave = 1,
        Acute = 2,
        Hook  = 3,
        Tilde = 4,
        Dot   = 5,
    };

    /**
     * @brief Converts a Mode value to the bit mask used by the engine API.
     * @param mode Mode value (possibly a combination of flags).
     * @return Bit mask.
     */
    constexpr uint32_t toMask(Mode mode) {
        return static_cast<uint32_t>(mode);
    }

} // namespace lotus

#endif // _LOTUS_TYPES_H_
