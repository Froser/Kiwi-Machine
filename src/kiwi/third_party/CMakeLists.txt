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

add_subdirectory(glog)
add_subdirectory(SDL2)
add_subdirectory(nes_apu)
add_subdirectory(zlib-1.3)
add_subdirectory(zlib-1.3/contrib/minizip)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_target_properties(glog PROPERTIES FOLDER "3rdparty/glog")
set_target_properties(glog_internal PROPERTIES FOLDER "3rdparty/glog")
set_target_properties(SDL2-static PROPERTIES FOLDER "3rdparty/sdl2")
set_target_properties(sdl_headers_copy PROPERTIES FOLDER "3rdparty/sdl2")
set_target_properties(SDL2main PROPERTIES FOLDER "3rdparty/sdl2")
set_target_properties(SDL2_test PROPERTIES FOLDER "3rdparty/sdl2")
set_target_properties(uninstall PROPERTIES FOLDER "3rdparty/sdl2")
set_target_properties(nes_apu PROPERTIES FOLDER "3rdparty/nes_apu")
set_target_properties(zlibstatic PROPERTIES FOLDER "3rdparty/zlib")
set_target_properties(example PROPERTIES FOLDER "3rdparty/zlib")
set_target_properties(minigzip PROPERTIES FOLDER "3rdparty/zlib")
set_target_properties(zlib PROPERTIES FOLDER "3rdparty/zlib")
set_target_properties(minizip PROPERTIES FOLDER "3rdparty/minizip")
