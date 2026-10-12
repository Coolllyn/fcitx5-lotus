/*
 * SPDX-FileCopyrightText: 2018 Luong Thanh Lam <ltlam93@gmail.com>
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Ky <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
/**
 * @file dictionary.h
 * @brief Custom dictionary lookups, ported 1:1 from bamboo/dictionary.go.
 *
 * The words are kept sorted in byte order (Go's sort.Strings) and duplicate-free, so one
 * std::lower_bound answers both questions the engine asks: an exact match at a word break and a
 * prefix match while a word is still being typed. The engine shares one dictionary between input
 * contexts through a std::shared_ptr, so the class owns no raw pointer and stays copyable.
 */

#ifndef _LOTUS_DICTIONARY_H_
#define _LOTUS_DICTIONARY_H_

#include <string>
#include <string_view>
#include <vector>

namespace lotus {

    /**
     * @brief Sorted word list backing the custom dictionary, like Go's dictionary.
     */
    class Dictionary {
      public:
        /**
         * @brief Replaces the words with the given list, like Go's newDictionary.
         * @param words Candidate words; empty ones are dropped, the rest are sorted byte-wise and
         *              deduplicated.
         */
        void loadFromWords(std::vector<std::string> words);

        /**
         * @brief Replaces the words with those of a dictionary file, like Go's NewDictionary.
         *
         * The text is split on '\n' with the trailing '\r' of a CRLF line removed (Go reads the file
         * with bufio.Reader.ReadLine), every line is lowercased, and empty lines are dropped.
         * @param wholeFile Whole content of the dictionary file.
         */
        void loadFromText(std::string_view wholeFile);

        /**
         * @brief Reports whether a word is in the dictionary, like Go's dictionary.hasWord.
         * @param word Word to look up; an empty word never matches.
         * @return True when the word is stored. A nil Go dictionary reports false; a null
         *         std::shared_ptr is the C++ equivalent, and an empty dictionary also reports false.
         */
        [[nodiscard]] bool hasWord(std::string_view word) const;

        /**
         * @brief Reports whether a word is in the dictionary or starts a longer dictionary word,
         *        like Go's dictionary.hasWordOrPrefix.
         * @param word Word to look up; an empty word never matches.
         * @return True when a stored word equals word or begins with it.
         */
        [[nodiscard]] bool hasWordOrPrefix(std::string_view word) const;

        /**
         * @brief Words currently stored, sorted byte-wise and duplicate-free.
         * @return Stored words; this mirrors the unexported words field that Go's
         *         dictionary_test.go prints when reporting a miss.
         */
        [[nodiscard]] const std::vector<std::string>& words() const;

      private:
        /**
         * @brief Orders a stored word against a lookup key byte-wise, like Go's string ordering.
         * @param stored Stored word.
         * @param candidate Word being looked up.
         * @return True when stored sorts before candidate.
         */
        static bool sortsBefore(const std::string& stored, std::string_view candidate);

        // Go: []string, kept sorted so the lookups can binary search.
        std::vector<std::string> words_;
    };

} // namespace lotus

#endif // _LOTUS_DICTIONARY_H_
