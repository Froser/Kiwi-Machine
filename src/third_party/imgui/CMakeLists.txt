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

project(imgui CXX)

set(CMAKE_CXX_STANDARD 17)

set(Sources
        backends/imgui_impl_sdl2.cpp
        backends/imgui_impl_sdl2.h
        backends/imgui_impl_sdlrenderer2.cpp
        backends/imgui_impl_sdlrenderer2.h
        imconfig.h
        imgui.cpp
        imgui.h
        imgui_demo.cpp
        imgui_draw.cpp
        imgui_internal.h
        imgui_tables.cpp
        imgui_widgets.cpp
        imstb_rectpack.h
        imstb_textedit.h
        imstb_truetype.h
)

add_library(${PROJECT_NAME} STATIC ${Sources})
target_include_directories(${PROJECT_NAME} PRIVATE "." ${SDL2_SOURCE_DIR}/include)
