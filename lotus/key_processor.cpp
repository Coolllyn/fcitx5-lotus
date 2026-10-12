/*
 * SPDX-FileCopyrightText: 2018 Luong Thanh Lam <ltlam93@gmail.com>
 * SPDX-FileCopyrightText: 2022-2022 CSSlayer <wengxt@gmail.com>
 * SPDX-FileCopyrightText: 2026-2026 Nguyen Hoang Ky <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
/**
 * @file key_processor.cpp
 * @brief Key event wrapper around the engine, ported 1:1 from bamboo/fcitxbambooengine.go.
 */

#include "key_processor.h"

#include "case_map.h"
#include "utf8.h"
#include "utils.h"

#include <algorithm>
#include <array>
#include <ctime>
#include <utility>

namespace lotus {

    MacroCase determineMacroCase(std::u32string_view text) {
        bool hasLower = false;
        bool hasUpper = false;
        for (const char32_t codePoint : text) {
            if (case_map::isLower(codePoint)) {
                hasLower = true;
            } else if (case_map::isUpper(codePoint)) {
                hasUpper = true;
            }
            if (hasLower && hasUpper) {
                return MacroCase::NoChange;
            }
        }
        if (hasLower) {
            return MacroCase::AllSmall;
        }
        if (hasUpper) {
            return MacroCase::AllCapital;
        }
        return MacroCase::NoChange;
    }

    char32_t getLastRune(std::u32string_view text) {
        return text.empty() ? 0 : text.back();
    }

    bool inKeyList(const std::vector<char32_t>& list, char32_t key) {
        return std::find(list.begin(), list.end(), key) != list.end();
    }

    KeyProcessor::KeyProcessor(InputMethod inputMethod, bool spellCheckWithDictsDefault) : engine_(std::move(inputMethod), kEstdFlags) {
        options_.spellCheckWithDicts = spellCheckWithDictsDefault;
    }

    void KeyProcessor::setOptions(const Options& options) {
        options_       = options;
        uint32_t flags = kEstdFlags;
        if (options.modernStyle) {
            flags &= ~kEstdToneStyle;
        } else {
            flags |= kEstdToneStyle;
        }
        if (options.freeMarking) {
            flags |= kEfreeToneMarking;
        } else {
            flags &= ~kEfreeToneMarking;
        }
        // Go compared against 0: an unset (follow flags) mode enabled the flag, Disabled cleared it.
        if (!options.w2u.has_value() || *options.w2u != W2uMode::Disabled) {
            flags |= kEw2uEnabled;
        } else {
            flags &= ~kEw2uEnabled;
        }
        if (!options.bracketTransform.has_value() || *options.bracketTransform != BracketTransformMode::Disabled) {
            flags |= kEbracketTransformEnabled;
        } else {
            flags &= ~kEbracketTransformEnabled;
        }
        engine_.setFlag(flags);
        engine_.setW2uMode(options.w2u);
        engine_.setBracketTransformMode(options.bracketTransform);
    }

    Options& KeyProcessor::options() {
        return options_;
    }

    const Options& KeyProcessor::options() const {
        return options_;
    }

    void KeyProcessor::setMacroEnabled(bool enabled) {
        options_.macroEnabled = enabled;
    }

    void KeyProcessor::setShouldRestoreKeyStrokes(bool restore) {
        shouldRestoreKeyStrokes_ = restore;
    }

    bool KeyProcessor::shouldRestoreKeyStrokes() const {
        return shouldRestoreKeyStrokes_;
    }

    void KeyProcessor::setMacroTable(std::shared_ptr<const MacroTable> table) {
        macroTable_ = std::move(table);
    }

    void KeyProcessor::setDictionary(std::shared_ptr<const Dictionary> dictionary) {
        dictionary_ = std::move(dictionary);
    }

    bool KeyProcessor::processKey(uint32_t keyVal, uint32_t stateMask) {
        // Go defers updateLastKeyWithShift, so the bookkeeping runs on every return path.
        const bool accepted = preeditProcessKeyEvent(keyVal, stateMask);
        updateLastKeyWithShift(keyVal, stateMask);
        return accepted;
    }

    bool KeyProcessor::preeditProcessKeyEvent(uint32_t keyVal, uint32_t stateMask) {
        const auto keyRune    = static_cast<char32_t>(keyVal);
        const bool hasPreedit = !preedit_.empty();

        // workaround for chrome's address bar and Google SpreadSheets
        if (!shouldRestoreKeyStrokes_ && (!isValidState(stateMask) || !canProcessKey(keyVal) || (!options_.macroEnabled && !hasPreedit && !engine_.canProcessKey(keyRune)))) {
            if (hasPreedit) {
                commitPreeditAndReset(getPreeditString());
            }
            return false;
        }

        if (keyVal == kFcitxBackSpace) {
            if (!hasPreedit) {
                return false;
            }
            if (runeCount() == 1) {
                commitPreeditAndReset(U"");
                return true;
            }
            engine_.removeLastChar(true);
            updatePreedit(getPreeditString());
            return true;
        }

        const std::u32string oldText = getPreeditString();
        if (keyVal == kFcitxTab) {
            std::optional<std::u32string> macroText = getMacroText();
            if (macroText.has_value()) {
                commitPreeditAndReset(std::move(*macroText));
            } else {
                commitPreeditAndReset(getComposedString(oldText));
                return false;
            }
            return true;
        }

        auto [newText, isWordBreakRune] = getCommitText(keyVal, stateMask, oldText);
        if (isWordBreakRune) {
            commitPreeditAndReset(std::move(newText));
            return true;
        }
        updatePreedit(newText);
        return true;
    }

    std::pair<std::u32string, bool> KeyProcessor::getCommitText(uint32_t keyVal, uint32_t stateMask, std::u32string_view oldText) {
        auto keyRune = static_cast<char32_t>(keyVal);
        // restore key strokes by pressing Shift + Space
        if (shouldRestoreKeyStrokes_) {
            shouldRestoreKeyStrokes_ = false;
            engine_.restoreLastWord(!utils::hasAnyVietnameseRune(oldText));
            return {getPreeditString(), false};
        }
        const bool canProcess = engine_.canProcessKey(keyRune);
        if (canProcess || (options_.macroEnabled && keyRune >= U'0' && keyRune <= U'9')) {
            if (!canProcess && shouldFallbackToEnglish(true)) {
                engine_.restoreLastWord(false);
            }
            if ((stateMask & kFcitxLockMask) != 0) {
                keyRune = toUpper(keyRune);
            }
            engine_.processKey(keyRune, getBambooInputMode());
            if (inKeyList(engine_.inputMethod().appendingKeys, keyRune)) {
                const std::u32string fullSeq = getProcessedString(toMask(Mode::VietnameseMode));
                std::u32string       newText;
                if (shouldFallbackToEnglish(true)) {
                    newText = getProcessedString(toMask(Mode::EnglishMode) | toMask(Mode::FullText));
                } else {
                    newText = fullSeq;
                }
                if (!fullSeq.empty() && getLastRune(fullSeq) == keyRune) {
                    // [[ => [
                    const std::u32string ret             = getPreeditString();
                    const char32_t       lastRune        = getLastRune(ret);
                    const bool           isWordBreakRune = utils::isWordBreakSymbol(lastRune);
                    // Go calls this hacking: the word break rune left behind by the doubled key is
                    // re-typed as a space in English mode.
                    if (isWordBreakRune) {
                        engine_.removeLastChar(false);
                        engine_.processKey(U' ', toMask(Mode::EnglishMode));
                    }
                    return {ret, isWordBreakRune};
                }
                if (!newText.empty() && getLastRune(newText) == keyRune) {
                    // f] => f]
                    const bool isWordBreakRune = utils::isWordBreakSymbol(keyRune);
                    if (isWordBreakRune) {
                        engine_.removeLastChar(false);
                        engine_.processKey(U' ', toMask(Mode::EnglishMode));
                    }
                    std::u32string text(oldText);
                    text.push_back(keyRune);
                    return {std::move(text), isWordBreakRune};
                }
                // ] => o?
                return {getPreeditString(), false};
            }
            return {getPreeditString(), false};
        }
        if (utils::isWordBreakSymbol(keyRune)) {
            // macro processing
            if (options_.macroEnabled && macroTable_ != nullptr) {
                if (const auto macroVal = macroTable_->get(utf8::encode(oldText))) {
                    engine_.reset();
                    std::u32string text = expandMacro(oldText, utf8::decode(*macroVal));
                    text.push_back(keyRune);
                    return {std::move(text), true};
                }
            }
            if (utils::hasAnyVietnameseRune(oldText) && mustFallbackToEnglish()) {
                engine_.restoreLastWord(false);
                std::u32string newText = getProcessedString(toMask(Mode::EnglishMode) | toMask(Mode::FullText));
                newText.push_back(keyRune);
                engine_.processKey(keyRune, toMask(Mode::EnglishMode));
                return {std::move(newText), true};
            }
            engine_.processKey(keyRune, toMask(Mode::EnglishMode));
            std::u32string text(oldText);
            text.push_back(keyRune);
            return {std::move(text), true};
        }
        return {std::u32string{}, true};
    }

    std::u32string KeyProcessor::encodeText(std::u32string_view text) const {
        // Go keeps the encoded bytes in a string; the boundary of the port is UTF-32, so the bytes
        // are decoded back to code points (the identity for the Unicode charset).
        return utf8::decode(encode(options_.outputCharset, text));
    }

    std::optional<std::u32string> KeyProcessor::getMacroText() const {
        if (!options_.macroEnabled || macroTableEmpty()) {
            return std::nullopt;
        }
        const std::u32string text     = getProcessedString(toMask(Mode::PunctuationMode));
        const auto           macroVal = macroTable_->get(utf8::encode(text));
        if (!macroVal.has_value()) {
            return std::nullopt;
        }
        return expandMacro(text, utf8::decode(*macroVal));
    }

    std::u32string KeyProcessor::getProcessedString(uint32_t mode) const {
        return engine_.processedString(mode);
    }

    std::u32string KeyProcessor::getPreeditString() const {
        if (shouldFallbackToEnglish(true)) {
            return getProcessedString(toMask(Mode::EnglishMode) | toMask(Mode::FullText));
        }
        return getProcessedString(toMask(Mode::PunctuationMode) | toMask(Mode::FullText));
    }

    std::u32string KeyProcessor::getComposedString(std::u32string_view oldText) const {
        if (utils::hasAnyVietnameseRune(oldText) && mustFallbackToEnglish()) {
            return getProcessedString(toMask(Mode::EnglishMode) | toMask(Mode::FullText));
        }
        return std::u32string(oldText);
    }

    std::size_t KeyProcessor::runeCount() const {
        return getPreeditString().size();
    }

    uint32_t KeyProcessor::getBambooInputMode() const {
        if (shouldFallbackToEnglish(false)) {
            return toMask(Mode::EnglishMode);
        }
        return toMask(Mode::VietnameseMode);
    }

    void KeyProcessor::updateLastKeyWithShift(uint32_t keyVal, uint32_t stateMask) {
        if (engine_.canProcessKey(static_cast<char32_t>(keyVal))) {
            lastKeyWithShift_ = (stateMask & kFcitxShiftMask) != 0;
        } else {
            lastKeyWithShift_ = false;
        }
    }

    bool KeyProcessor::macroTableEmpty() const {
        return macroTable_ == nullptr || macroTable_->empty();
    }

    std::string KeyProcessor::replaceAllText(std::string_view text, std::string_view needle, std::string_view replacement) {
        if (needle.empty()) {
            return std::string(text);
        }
        std::string out;
        out.reserve(text.size());
        std::size_t index = 0;
        while (index < text.size()) {
            if (text.compare(index, needle.size(), needle) == 0) {
                out.append(replacement);
                index += needle.size();
                continue;
            }
            out.push_back(text[index]);
            ++index;
        }
        return out;
    }

    std::string KeyProcessor::goTimeLayout(std::string_view format) {
        // Go: strftimeReplacer, applied left to right (no pattern is a prefix of another one).
        static constexpr std::array<std::pair<std::string_view, std::string_view>, 18> kReplacements = {
            {
                {"%H", "15"},
                {"%I", "03"},
                {"%M", "04"},
                {"%S", "05"},
                {"%p", "PM"},
                {"%P", "pm"},
                {"%d", "02"},
                {"%m", "01"},
                {"%Y", "2006"},
                {"%y", "06"},
                {"%b", "Jan"},
                {"%B", "January"},
                {"%a", "Mon"},
                {"%A", "Monday"},
                {"%D", "01/02/06"},
                {"%F", "2006-01-02"},
                {"%T", "15:04:05"},
                {"%R", "15:04"},
            },
        };
        std::string out;
        out.reserve(format.size());
        std::size_t index = 0;
        while (index < format.size()) {
            bool matched = false;
            for (const auto& [from, to] : kReplacements) {
                if (format.compare(index, from.size(), from) == 0) {
                    out.append(to);
                    index += from.size();
                    matched = true;
                    break;
                }
            }
            if (!matched) {
                out.push_back(format[index]);
                ++index;
            }
        }
        return out;
    }

    std::string KeyProcessor::strftimePattern(std::string_view layout) {
        // Longest token first so that "2006" wins over "06" and "January" over "Jan".
        static constexpr std::array<std::pair<std::string_view, std::string_view>, 14> kTokens = {
            {
                {"2006", "%Y"},
                {"January", "%B"},
                {"Jan", "%b"},
                {"Monday", "%A"},
                {"Mon", "%a"},
                {"15", "%H"},
                {"03", "%I"},
                {"04", "%M"},
                {"05", "%S"},
                {"02", "%d"},
                {"01", "%m"},
                {"06", "%y"},
                {"PM", "%p"},
                {"pm", "%P"},
            },
        };
        std::string out;
        out.reserve(layout.size());
        std::size_t index = 0;
        while (index < layout.size()) {
            bool matched = false;
            for (const auto& [token, specifier] : kTokens) {
                if (layout.compare(index, token.size(), token) == 0) {
                    out.append(specifier);
                    index += token.size();
                    matched = true;
                    break;
                }
            }
            if (!matched) {
                out.push_back(layout[index]);
                ++index;
            }
        }
        return out;
    }

    std::string KeyProcessor::formatTime(std::string_view format) {
        if (format.empty()) {
            return "";
        }
        const std::string layout = goTimeLayout(format);
        if (layout.empty()) {
            return "";
        }
        // Go renders the reference layout "15:04:05 02/01/2006" when the format still holds a '%'
        // that the replacer did not know.
        constexpr std::string_view kFallbackLayout = "15:04:05 02/01/2006";
        const std::string pattern = (std::string_view(layout) == format && format.find('%') != std::string_view::npos) ? strftimePattern(kFallbackLayout) : strftimePattern(layout);

        const std::time_t now       = std::time(nullptr);
        const std::tm*    localTime = std::localtime(&now);
        if (localTime == nullptr) {
            return "";
        }
        std::array<char, 256> buffer{};
        const std::size_t     written = std::strftime(buffer.data(), buffer.size(), pattern.c_str(), localTime);
        if (written == 0) {
            return "";
        }
        return {buffer.data(), written};
    }

    std::u32string KeyProcessor::expandMacro(std::u32string_view str, std::u32string_view macroText) const {
        constexpr std::string_view kTimeMacro = "$TIME";
        constexpr std::string_view kDateMacro = "$DATE";
        std::string                text       = utf8::encode(macroText);
        if (!options_.timeFormat.empty() && text.find(kTimeMacro) != std::string::npos) {
            text = replaceAllText(text, kTimeMacro, formatTime(options_.timeFormat));
        }
        if (!options_.dateFormat.empty() && text.find(kDateMacro) != std::string::npos) {
            text = replaceAllText(text, kDateMacro, formatTime(options_.dateFormat));
        }
        if (options_.autoCapitalizeMacro) {
            switch (determineMacroCase(str)) {
                case MacroCase::AllSmall: return utf8::decode(case_map::toLowerUtf8(text));
                case MacroCase::AllCapital: return utf8::decode(case_map::toUpperUtf8(text));
                case MacroCase::NoChange: break;
            }
        }
        return utf8::decode(text);
    }

    void KeyProcessor::updatePreedit(std::u32string_view processedStr) {
        std::u32string encodedStr = encodeText(processedStr);
        if (encodedStr.empty()) {
            preedit_.clear();
            commit_.clear();
            return;
        }
        preedit_ = std::move(encodedStr);
    }

    void KeyProcessor::commitPreeditAndReset(std::u32string text) {
        commit_ = std::move(text);
        preedit_.clear();
        engine_.reset();
    }

    bool KeyProcessor::mustFallbackToEnglish() const {
        if (!options_.autoNonVnRestore) {
            return false;
        }
        const std::u32string vnSeq = getProcessedString(toMask(Mode::VietnameseMode) | toMask(Mode::LowerCase));
        if (vnSeq.empty()) {
            return false;
        }
        // dd is allowed even in a non-Vietnamese sequence because it is a common abbreviation.
        if (options_.ddFreeStyle && vnSeq.find(U'đ') != std::u32string::npos) {
            return false;
        }
        if (options_.spellCheckWithDicts) {
            return dictionary_ == nullptr || !dictionary_->hasWord(utf8::encode(vnSeq));
        }
        return !engine_.isValid(true);
    }

    bool KeyProcessor::shouldFallbackToEnglish(bool checkVnRune) const {
        if (!options_.autoNonVnRestore) {
            return false;
        }
        const std::u32string vnSeq = getProcessedString(toMask(Mode::VietnameseMode) | toMask(Mode::LowerCase));
        if (vnSeq.empty()) {
            return false;
        }
        if (options_.macroEnabled && !macroTableEmpty()) {
            if (macroTable_->get(utf8::encode(getProcessedString(toMask(Mode::PunctuationMode)))).has_value()) {
                return false;
            }
        }
        // dd is allowed even in a non-Vietnamese sequence because it is a common abbreviation.
        if (options_.ddFreeStyle && !utils::hasAnyVietnameseVower(vnSeq) && (vnSeq.back() == U'd' || vnSeq.find(U'đ') != std::u32string::npos)) {
            return false;
        }
        if (checkVnRune && !utils::hasAnyVietnameseRune(vnSeq)) {
            return false;
        }
        // A dictionary word (or the beginning of one) must not be restored to the raw key strokes
        // while it is still being typed; an incomplete word is still rejected at the word break by
        // mustFallbackToEnglish.
        if (options_.spellCheckWithDicts && dictionary_ != nullptr && dictionary_->hasWordOrPrefix(utf8::encode(vnSeq))) {
            return false;
        }
        return !engine_.isValid(false);
    }

    bool KeyProcessor::canProcessKey(uint32_t keyVal) const {
        const auto keyRune = static_cast<char32_t>(keyVal);
        if (keyVal == kFcitxSpace || keyVal == kFcitxBackSpace || utils::isWordBreakSymbol(keyRune)) {
            return true;
        }
        if (keyVal == kFcitxTab && options_.macroEnabled && !macroTableEmpty() && !preedit_.empty()) {
            if (macroTable_->get(utf8::encode(getProcessedString(toMask(Mode::PunctuationMode)))).has_value()) {
                return true;
            }
        }
        return engine_.canProcessKey(keyRune);
    }

    bool KeyProcessor::isValidState(uint32_t stateMask) {
        constexpr uint32_t kInvalidStateMask = kFcitxControlMask | kFcitxMod1Mask | kFcitxIgnoredMask | kFcitxSuperMask | kFcitxHyperMask | kFcitxMetaMask;
        return (stateMask & kInvalidStateMask) == 0;
    }

    char32_t KeyProcessor::toUpper(char32_t keyRune) const {
        switch (keyRune) {
            case U'[':
                if (engine_.canProcessKey(keyRune)) {
                    return U'{';
                }
                break;
            case U']':
                if (engine_.canProcessKey(keyRune)) {
                    return U'}';
                }
                break;
            case U'{':
                if (engine_.canProcessKey(keyRune)) {
                    return U'[';
                }
                break;
            case U'}':
                if (engine_.canProcessKey(keyRune)) {
                    return U']';
                }
                break;
            default: break;
        }
        return keyRune;
    }

    const std::u32string& KeyProcessor::preedit() const {
        return preedit_;
    }

    std::u32string KeyProcessor::takeCommit() {
        // Go copies commitText, clears the field and then encodes the copy (EnginePullCommit).
        return encodeText(std::exchange(commit_, std::u32string{}));
    }

    void KeyProcessor::commitPreedit() {
        commitPreeditAndReset(getPreeditString());
    }

    void KeyProcessor::reset() {
        commitPreeditAndReset(U"");
    }

    void KeyProcessor::rebuildFromText(std::u32string_view text) {
        engine_.rebuildFromText(text);
        // Go stores the raw rendering here (EngineRebuildFromText does not encode it), unlike
        // updatePreedit which stores the charset encoded text.
        preedit_ = getPreeditString();
        commit_.clear();
    }

    Engine& KeyProcessor::engine() {
        return engine_;
    }

} // namespace lotus
