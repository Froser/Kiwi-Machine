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
KIWI_TARGET_FOLDER(glog "3rdparty/glog")
KIWI_TARGET_FOLDER(glog_internal "3rdparty/glog")
KIWI_TARGET_FOLDER(SDL2-static "3rdparty/sdl2")
KIWI_TARGET_FOLDER(sdl_headers_copy "3rdparty/sdl2")
KIWI_TARGET_FOLDER(SDL2main "3rdparty/sdl2")
KIWI_TARGET_FOLDER(SDL2_test "3rdparty/sdl2")
KIWI_TARGET_FOLDER(uninstall "3rdparty/sdl2")
KIWI_TARGET_FOLDER(nes_apu "3rdparty/nes_apu")
KIWI_TARGET_FOLDER(zlibstatic "3rdparty/zlib")
KIWI_TARGET_FOLDER(example "3rdparty/zlib")
KIWI_TARGET_FOLDER(minigzip "3rdparty/zlib")
KIWI_TARGET_FOLDER(zlib "3rdparty/zlib")
KIWI_TARGET_FOLDER(minizip "3rdparty/minizip")
