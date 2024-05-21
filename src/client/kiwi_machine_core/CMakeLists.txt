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

project(kiwi_machine_core CXX)

set(CMAKE_CXX_STANDARD 17)

OPTION(KIWI_EMBEDDED_ROMS "Embed roms data in code" ON)

FIND_PACKAGE(Python3 REQUIRED)

include_directories(
        .

        # Generation directory
        ${CMAKE_CURRENT_BINARY_DIR}/

        # GFlags
        ../..
        ${CMAKE_CURRENT_BINARY_DIR}/../third_party/gflags/include
)

# Include zlib directory
include_directories(${zlib_BINARY_DIR})

list(APPEND
        CMAKE_MODULE_PATH
        "${CMAKE_CURRENT_SOURCE_DIR}/../../../build/cmake"
)
find_package(Kiwi REQUIRED)

set(Sources
        build/kiwi_defines.h
        debug/debug_port.cc
        debug/debug_port.h
        debug/debug_roms.cc
        debug/debug_roms.h
        kiwi_main.h
        kiwi_main.cc
        models/nes_audio.cc
        models/nes_audio.h
        models/nes_config.cc
        models/nes_config.h
        models/nes_frame.cc
        models/nes_frame.h
        models/nes_runtime.cc
        models/nes_runtime.h
        preset_roms/preset_roms.h
        preset_roms/preset_roms.cc
        resources/audio_resources.cc
        resources/audio_resources.h
        resources/font_resources.cc
        resources/font_resources.h
        resources/string_resources.cc
        resources/string_resources.h
        resources/image_resources.cc
        resources/image_resources.h
        ui/widgets/about_widget.cc
        ui/widgets/about_widget.h
        ui/widgets/canvas.cc
        ui/widgets/canvas.h
        ui/widgets/canvas_observer.cc
        ui/widgets/canvas_observer.h
        ui/widgets/card_widget.cc
        ui/widgets/card_widget.h
        ui/widgets/disassembly_widget.cc
        ui/widgets/disassembly_widget.h
        ui/widgets/demo_widget.cc
        ui/widgets/demo_widget.h
        ui/widgets/export_widget.cc
        ui/widgets/export_widget.h
        ui/widgets/flex_item_widget.cc
        ui/widgets/flex_item_widget.h
        ui/widgets/flex_items_widget.cc
        ui/widgets/flex_items_widget.h
        ui/widgets/frame_rate_widget.cc
        ui/widgets/frame_rate_widget.h
        ui/widgets/in_game_menu.cc
        ui/widgets/in_game_menu.h
        ui/widgets/joystick_button.cc
        ui/widgets/joystick_button.h
        ui/widgets/group_widget.cc
        ui/widgets/group_widget.h
        ui/widgets/kiwi_bg_widget.cc
        ui/widgets/kiwi_bg_widget.h
        ui/widgets/kiwi_item_widget.cc
        ui/widgets/kiwi_item_widget.h
        ui/widgets/kiwi_items_widget.cc
        ui/widgets/kiwi_items_widget.h
        ui/widgets/loading_widget.cc
        ui/widgets/loading_widget.h
        ui/widgets/memory_widget.cc
        ui/widgets/memory_widget.h
        ui/widgets/menu_bar.cc
        ui/widgets/menu_bar.h
        ui/widgets/nametable_widget.cc
        ui/widgets/nametable_widget.h
        ui/widgets/palette_widget.cc
        ui/widgets/palette_widget.h
        ui/widgets/pattern_widget.cc
        ui/widgets/pattern_widget.h
        ui/widgets/side_menu.cc
        ui/widgets/side_menu.h
        ui/widgets/splash.cc
        ui/widgets/splash.h
        ui/widgets/stack_widget.cc
        ui/widgets/stack_widget.h
        ui/widgets/toast.cc
        ui/widgets/toast.h
        ui/widgets/touch_button.cc
        ui/widgets/touch_button.h
        ui/widgets/virtual_joystick.cc
        ui/widgets/virtual_joystick.h
        ui/widgets/widget.cc
        ui/widgets/widget.h
        ui/application.cc
        ui/application.h
        ui/main_window.cc
        ui/main_window.h
        ui/styles.cc
        ui/styles.h
        ui/window_base.cc
        ui/window_base.h
        utility/audio_effects.cc
        utility/audio_effects.h
        utility/fonts.cc
        utility/fonts.h
        utility/images.cc
        utility/images.h
        utility/key_mapping_util.cc
        utility/key_mapping_util.h
        utility/localization.cc
        utility/localization.h
        utility/math.cc
        utility/math.h
        utility/timer.cc
        utility/timer.h
        utility/zip_reader.cc
        utility/zip_reader.h
)

if (IOS OR ANDROID)
    set(KIWI_MOBILE TRUE)
else ()
    set(KIWI_MOBILE FALSE)
endif ()

if (KIWI_MOBILE)
    list(APPEND Sources
            ui/main_window_mobile.cc
            ui/widgets/in_game_menu_mobile.cc
    )
endif ()

if (IOS)
    list(APPEND Sources
            ui/window_base_ios.mm
    )
endif ()

if (EMSCRIPTEN)
    list(APPEND Sources
            utility/emscripten/bridge_api.cc
            utility/emscripten/bridge_api.h
    )
endif ()

if (${KIWI_EMBEDDED_ROMS} AND NOT EMSCRIPTEN)
    set(USE_EMBEDDED_ROMS ON)
else ()
    set(USE_EMBEDDED_ROMS OFF)
endif ()
message(STATUS "Use embedded roms: " ${USE_EMBEDDED_ROMS})

# Resources generation:
# Use '__always_buildx.h' as a fake output, to make the command run every time.
if (EMSCRIPTEN)
    set(USE_WASM_IGNORE ON)
endif ()

add_custom_command(
        OUTPUT resources/image_resources.h resources/image_resources.cc __always_build0.h
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/build/gen_image_resources.py
        ARGS ${CMAKE_CURRENT_BINARY_DIR}/resources ${USE_WASM_IGNORE}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        VERBATIM
)

add_custom_command(
        OUTPUT resources/audio_resources.h resources/audio_resources.cc __always_build1.h
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/build/gen_audio_resources.py
        ARGS ${CMAKE_CURRENT_BINARY_DIR}/resources ${USE_WASM_IGNORE}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        VERBATIM
)

add_custom_command(
        OUTPUT resources/font_resources.h resources/font_resources.cc __always_build2.h
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/build/gen_font_resources.py
        ARGS ${CMAKE_CURRENT_BINARY_DIR}/resources ${USE_WASM_IGNORE}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        VERBATIM
)

add_custom_command(
        OUTPUT resources/string_resources.h resources/string_resources.cc __always_build3.h
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/build/gen_string_resources.py
        ARGS ${CMAKE_CURRENT_BINARY_DIR}/resources
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        VERBATIM
)

add_custom_command(
        OUTPUT preset_roms/preset_roms.cc __always_build_roms.h
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/build/gen_preset_roms_data.py
        ARGS ${CMAKE_CURRENT_BINARY_DIR}/preset_roms ${USE_EMBEDDED_ROMS}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build
        VERBATIM
)

KIWI_ADD_LIBRARY(${PROJECT_NAME} ${Sources})

target_link_libraries(${PROJECT_NAME}
        PRIVATE
        Kiwi::kiwi_static imgui
        gflags_nothreads_static
        SDL2_image
        SDL2_mixer
        minizip
)
target_include_directories(${PROJECT_NAME} PRIVATE Kiwi::kiwi_static)
target_include_directories(${PROJECT_NAME}
        PRIVATE
        "../../third_party/imgui"
)

if (${USE_EMBEDDED_ROMS})
    target_compile_definitions(${PROJECT_NAME} INTERFACE Kiwi::kiwi_static)
else ()
    target_compile_definitions(${PROJECT_NAME} INTERFACE Kiwi::kiwi_static PRIVATE KIWI_USE_EXTERNAL_PAK)
endif ()
