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

# Emscripten and Switch homebrew do not support shared libraries.
if(EMSCRIPTEN OR KIWI_SWITCH)
    set(ZLIB_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(ZLIB_BUILD_STATIC ON CACHE BOOL "" FORCE)
    set(MINIZIP_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(MINIZIP_BUILD_STATIC ON CACHE BOOL "" FORCE)
endif()
if(KIWI_SWITCH)
    set(HAVE_DBGHELP OFF CACHE INTERNAL "" FORCE)
    set(ZLIB_BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(ZLIB_INSTALL OFF CACHE BOOL "" FORCE)
    set(MINIZIP_BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(MINIZIP_INSTALL OFF CACHE BOOL "" FORCE)
endif()

if(KIWI_SWITCH)
    KIWI_SWITCH_PREPARE_GLOG("${CMAKE_CURRENT_BINARY_DIR}/glog")
endif()
add_subdirectory(glog)
if(KIWI_SWITCH)
    KIWI_SWITCH_CONFIGURE_GLOG()
endif()
if(NOT KIWI_SWITCH)
    add_subdirectory(SDL2)
endif()
add_subdirectory(nes_apu)
add_subdirectory(zlib-1.3.2)
add_subdirectory(zlib-1.3.2/contrib/minizip)

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
KIWI_TARGET_FOLDER(static_minigzip "3rdparty/zlib")
KIWI_TARGET_FOLDER(zlib "3rdparty/zlib")
KIWI_TARGET_FOLDER(zlib_example "3rdparty/zlib")
KIWI_TARGET_FOLDER(zlib_static_example "3rdparty/zlib")
KIWI_TARGET_FOLDER(zlib_static_example64 "3rdparty/zlib")
KIWI_TARGET_FOLDER(infcover "3rdparty/zlib")
KIWI_TARGET_FOLDER(minizip "3rdparty/minizip")
KIWI_TARGET_FOLDER(minizipstatic "3rdparty/minizip")
KIWI_TARGET_FOLDER(libminizip "3rdparty/minizip")
KIWI_TARGET_FOLDER(libminizipstatic "3rdparty/minizip")
KIWI_TARGET_FOLDER(miniunzipstatic "3rdparty/minizip")
