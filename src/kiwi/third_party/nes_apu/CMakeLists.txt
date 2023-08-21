# Copyright (C) 2023 Yisi Yu
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

project(nes_apu CXX)

set(CMAKE_CXX_STANDARD 17)

set(Sources
        apu_snapshot.cpp
        apu_snapshot.h
        blargg_common.h
        blargg_source.h
        Blip_Buffer.cpp
        Blip_Buffer.h
        Blip_Synth.h
        Multi_Buffer.cpp
        Multi_Buffer.h
        Nes_Apu.cpp
        Nes_Apu.h
        Nes_Namco.cpp
        Nes_Namco.h
        Nes_Oscs.cpp
        Nes_Oscs.h
        Nes_Vrc6.cpp
        Nes_Vrc6.h
        Nonlinear_Buffer.cpp
        Nonlinear_Buffer.h
)

add_library(${PROJECT_NAME} STATIC ${Sources})
