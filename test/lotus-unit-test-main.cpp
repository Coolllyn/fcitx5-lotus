/*
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Ky <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Runs every engine unit test group in one process each; ctest still owns
 * one entry per group, so labels, timeouts and crash isolation are unchanged.
 */
#include <cstdio>
#include <cstring>

int run_utf8();
int run_data();
int run_utils();
int run_encoder();
int run_rules_parser();
int run_flattener();
int run_spelling();
int run_engine_utils();
int run_engine_smoke();
int run_engine();
int run_rebuild();
int run_macro_dictionary();
int run_key_processor();
int run_golden_smoke();
int run_numeric_macro_case();

int main(int argc, char** argv) {
    const struct {
        const char* name;
        int (*fn)();
    } tests[] = {
        {"utf8", run_utf8},
        {"data", run_data},
        {"utils", run_utils},
        {"encoder", run_encoder},
        {"rules_parser", run_rules_parser},
        {"flattener", run_flattener},
        {"spelling", run_spelling},
        {"engine_utils", run_engine_utils},
        {"engine_smoke", run_engine_smoke},
        {"engine", run_engine},
        {"rebuild", run_rebuild},
        {"macro_dictionary", run_macro_dictionary},
        {"key_processor", run_key_processor},
        {"golden_smoke", run_golden_smoke},
        {"numeric_macro_case", run_numeric_macro_case},
    };

    if (argc != 3 || std::strcmp(argv[1], "--run") != 0) {
        std::fprintf(stderr, "usage: %s --run <test>\navailable:", argv[0]);
        for (const auto& t : tests) {
            std::fprintf(stderr, " %s", t.name);
        }
        std::fprintf(stderr, "\n");
        return 2;
    }
    for (const auto& t : tests) {
        if (std::strcmp(argv[2], t.name) == 0) {
            return t.fn();
        }
    }
    std::fprintf(stderr, "unknown test: %s\n", argv[2]);
    return 2;
}
