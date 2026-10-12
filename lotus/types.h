/*
 * SPDX-FileCopyrightText: 2018 Luong Thanh Lam <ltlam93@gmail.com>
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

    /**
     * @brief A set of bit flags of one enumeration.
     *
     * Replaces the plain uint32_t masks the Go engine passed around, so a mode set can no longer be
     * mixed up with an engine flag set or with an unrelated integer.
     * @tparam Flag Flag enumeration.
     */
    template <typename Flag>
    struct FlagSet {
        uint32_t bits = 0U;

        /**
         * @brief Reports whether a flag is part of the set.
         * @param flag Flag to test.
         * @return True when the flag is set.
         */
        [[nodiscard]] constexpr bool has(Flag flag) const {
            return (bits & static_cast<uint32_t>(flag)) != 0U;
        }

        /**
         * @brief Sets a flag.
         * @param flag Flag to set.
         * @return This set, so calls can chain.
         */
        constexpr FlagSet& add(Flag flag) {
            bits |= static_cast<uint32_t>(flag);
            return *this;
        }

        /**
         * @brief Clears a flag.
         * @param flag Flag to clear.
         * @return This set, so calls can chain.
         */
        constexpr FlagSet& remove(Flag flag) {
            bits &= ~static_cast<uint32_t>(flag);
            return *this;
        }
    };

    /** @brief A set of Mode flags. */
    using ModeSet = FlagSet<Mode>;

    /** @brief Engine flags (bit flags), replacing the Go E* constants. */
    enum class EngineFlag : uint8_t {
        FreeToneMarking         = 1U << 0U,
        StdToneStyle            = 1U << 1U,
        AutoCorrectEnabled      = 1U << 2U,
        W2uEnabled              = 1U << 3U,
        BracketTransformEnabled = 1U << 4U,
    };

    /** @brief A set of engine flags. */
    using EngineFlags = FlagSet<EngineFlag>;

    /**
     * @brief Combines two flags of one set.
     * @param first First flag.
     * @param second Second flag.
     * @return Set holding both flags.
     */
    template <typename Flag>
    constexpr FlagSet<Flag> operator|(Flag first, Flag second) {
        return FlagSet<Flag>{static_cast<uint32_t>(first) | static_cast<uint32_t>(second)};
    }

    /**
     * @brief Adds a flag to a set.
     * @param set Set to extend.
     * @param flag Flag to add.
     * @return Extended set.
     */
    template <typename Flag>
    constexpr FlagSet<Flag> operator|(FlagSet<Flag> set, Flag flag) {
        return FlagSet<Flag>{set.bits | static_cast<uint32_t>(flag)};
    }

    /**
     * @brief Combines two sets of the same flag type.
     * @param first First set.
     * @param second Second set.
     * @return Set holding both sets.
     */
    template <typename Flag>
    constexpr FlagSet<Flag> operator|(FlagSet<Flag> first, FlagSet<Flag> second) {
        return FlagSet<Flag>{first.bits | second.bits};
    }

    /** @brief Default engine flags, the Go EstdFlags set. */
    inline constexpr EngineFlags kEstdFlags = EngineFlag::FreeToneMarking | EngineFlag::StdToneStyle | EngineFlag::AutoCorrectEnabled | EngineFlag::W2uEnabled;

    /**
     * @brief "w" to "ư" mode, shared with the addon configuration.
     *
     * The engine additionally knows an internal "follow the engine flags" state, which it models as an
     * empty std::optional<W2uMode> instead of a fourth enumerator, so it never reaches the user
     * interface.
     */
    enum class W2uMode : uint8_t {
        Disabled   = 0,
        NonStart   = 1,
        Everywhere = 2,
    };

    /**
     * @brief Bracket transform mode, shared with the addon configuration.
     *
     * The engine models its internal "follow the engine flags" state as an empty
     * std::optional<BracketTransformMode>, see W2uMode.
     */
    enum class BracketTransformMode : uint8_t {
        Disabled   = 0,
        NonStart   = 1,
        Everywhere = 2,
    };

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
     * @brief Mode set of a single mode flag, the Go `Mode` value.
     * @param mode Mode flag.
     * @return Mode set holding that flag.
     */
    constexpr ModeSet toMask(Mode mode) {
        return ModeSet{static_cast<uint32_t>(mode)};
    }

    /**
     * @brief Mode set of a raw bit mask, for callers that carry the mask as a number.
     * @param bits Raw mask; unknown bits are kept, like Go kept them.
     * @return Mode set holding those bits.
     */
    constexpr ModeSet modeSetFromBits(uint32_t bits) {
        return ModeSet{bits};
    }

    /**
     * @brief Engine flag set of a raw bit mask, for callers that carry the flags as a number.
     * @param bits Raw mask; unknown bits are kept.
     * @return Engine flag set holding those bits.
     */
    constexpr EngineFlags engineFlagsFromBits(uint32_t bits) {
        return EngineFlags{bits};
    }

} // namespace lotus

#endif // _LOTUS_TYPES_H_
