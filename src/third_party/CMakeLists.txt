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

add_subdirectory(gflags)
add_subdirectory(imgui)
add_subdirectory(SDL_image-release-2.6.3)
add_subdirectory(SDL_mixer-release-2.6.3)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_target_properties(gflags_nothreads_static PROPERTIES FOLDER "3rdparty/gflags")
set_target_properties(imgui PROPERTIES FOLDER "3rdparty/imgui")
set_target_properties(zlib PROPERTIES FOLDER "3rdparty/zlib")
set_target_properties(SDL2_image PROPERTIES FOLDER "3rdparty/sdl2")
set_target_properties(SDL2_mixer PROPERTIES FOLDER "3rdparty/sdl2")