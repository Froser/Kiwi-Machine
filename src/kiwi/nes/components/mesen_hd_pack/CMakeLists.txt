# Copyright (C) 2026 Yisi Yu
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

set(mesen_hd_pack_sources
    hd_pack_types.h
    hires_parser.cc
    hires_parser.h
)

KIWI_ADD_LIBRARY(mesen_hd_pack STATIC ${mesen_hd_pack_sources})

target_compile_features(mesen_hd_pack PUBLIC cxx_std_20)
target_include_directories(mesen_hd_pack PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../..
)

KIWI_TARGET_FOLDER(mesen_hd_pack "kiwi/nes/components")
