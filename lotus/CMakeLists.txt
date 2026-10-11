# SPDX-FileCopyrightText: 2026 Nguyen Hoang Ky <nhktmdzhg@gmail.com>
#
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Portable C++17 Vietnamese input engine, ported from bamboo/bamboo-core (MIT, kept in CREDITS).
add_library(lotus_engine STATIC
    utf8.cpp
)
set_target_properties(lotus_engine PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
)
target_compile_options(lotus_engine PRIVATE -Wall -Wextra)
target_include_directories(lotus_engine PUBLIC ${PROJECT_SOURCE_DIR})
add_library(Lotus::Engine ALIAS lotus_engine)
