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

project(kiwi CXX)

set(CMAKE_CXX_STANDARD 20)

unset(KIWI_ENABLE_APU_FRAME_BOUNDARY_CLOCK_FIX CACHE)
unset(KIWI_ENABLE_INES1_TV_SYSTEM_FIX CACHE)
set(KIWI_ENABLE_APU_FRAME_BOUNDARY_CLOCK_FIX ON)
set(KIWI_ENABLE_INES1_TV_SYSTEM_FIX ON)

add_subdirectory(third_party)

# Make 'kiwi' as root.
include_directories(.)

# Mark 'kiwi' in binary as an include directory
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# Include zlib directory
include_directories(${zlib_BINARY_DIR})

# To use glog, added its gen directory to include directories.
include_directories(${glog_SOURCE_DIR}/src)
include_directories(${glog_BINARY_DIR}/..)

add_subdirectory(emulator/nes)

# Sources for base
set(base_sources
        base/atomic_ref_count.h
        base/base_export.h
        base/check.h
        base/compiler_specific.h
        base/containers/adapters.h
        base/cxx20_to_address.h
        base/dcheck_is_on.h
        base/immediate_crash.h
        base/location.cc
        base/location.h
        base/logging.h
        base/files/file.cc
        base/files/file.h
        base/files/file_enumerator.cc
        base/files/file_enumerator.h
        base/files/file_path.cc
        base/files/file_path.h
        base/files/file_util.cc
        base/files/file_util.h
        base/files/scoped_file.cc
        base/files/scoped_file.h
        base/functional/bind.h
        base/functional/bind_internal.h
        base/functional/callback.h
        base/functional/callback_forward.h
        base/functional/callback_helpers.h
        base/functional/callback_internal.cc
        base/functional/callback_internal.h
        base/functional/callback_tags.h
        base/memory/scoped_refptr.h
        base/memory/scoped_policy.h
        base/memory/raw_scoped_refptr_mismatch_checker.h
        base/memory/ref_counted.cc
        base/memory/ref_counted.h
        base/message_loop/message_pump_type.h
        base/numerics/safe_conversions.h
        base/numerics/safe_conversions_arm_impl.h
        base/numerics/safe_conversions_impl.h
        base/posix/eintr_wrapper.h
        base/platform/platform_factory.cc
        base/platform/platform_factory.h
        base/strings/string_number_conversions.cc
        base/strings/string_number_conversions.h
        base/strings/string_piece.h
        base/strings/string_split.cc
        base/strings/string_split.h
        base/strings/string_split_internal.h
        base/strings/string_util.cc
        base/strings/string_util.h
        base/strings/string_util_constants.cc
        base/strings/string_util_impl_helpers.h
        base/strings/string_util_internal.h
        base/strings/stringprintf.cc
        base/strings/stringprintf.h
        base/scoped_clear_last_error.h
        base/scoped_generic.h
        base/no_destructor.h
        base/runloop.cc
        base/runloop.h
        base/task/sequenced_task_runner.cc
        base/task/sequenced_task_runner.h
        base/task/bind_post_task.h
        base/task/bind_post_task_internal.h
        base/task/single_thread_task_executor.cc
        base/task/single_thread_task_executor.h
        base/task/single_thread_task_runner.cc
        base/task/single_thread_task_runner.h
        base/task/post_task_and_reply_with_result_internal.h
        base/time/time.cc
        base/time/time.h
        base/types/always_false.h
        base/threading/thread.cc
        base/threading/thread.h
)

set(emulator_sources
        emulator/audio_device.cc
        emulator/audio_device.h
        emulator/emulator.cc
        emulator/emulator.h
        emulator/emulator_export.h
        emulator/input_device.cc
        emulator/input_device.h
        emulator/io_devices.h
        emulator/render_device.cc
        emulator/render_device.h
        emulator/types.h
)

set(gba_sources
        emulator/gba/audio_device.cc
        emulator/gba/audio_device.h
        emulator/gba/emulator.cc
        emulator/gba/emulator.h
        emulator/gba/emulator_impl.cc
        emulator/gba/emulator_impl.h
        emulator/gba/gba_export.h
        emulator/gba/io_devices.h
        emulator/gba/luminance_device.cc
        emulator/gba/luminance_device.h
        emulator/gba/motion_device.cc
        emulator/gba/motion_device.h
        emulator/gba/render_device.cc
        emulator/gba/render_device.h
        emulator/gba/rom_data.cc
        emulator/gba/rom_data.h
        emulator/gba/rtc_device.cc
        emulator/gba/rtc_device.h
        emulator/gba/rumble_device.cc
        emulator/gba/rumble_device.h
        emulator/gba/types.h
)

if (APPLE)
    list(APPEND base_sources
            base/files/file_util_apple.mm
            base/mac/scoped_block.h
            base/mac/scoped_typeref.h
    )
endif ()

if (UNIX OR KIWI_SWITCH)
    list(APPEND base_sources
            base/files/file_enumerator_posix.cc
            base/files/file_posix.cc
            base/files/file_util_posix.cc
    )
endif ()

if (WIN32)
    list(APPEND base_sources
            base/files/file_enumerator_win.cc
            base/files/file_util_win.cc
            base/files/file_win.cc
            base/scoped_clear_last_error_win.cc
            base/win/shlwapi.h
            base/win/windows_types.h
            base/win/win_handle_types.h
    )
endif ()

if (USE_SDL2)
    list(APPEND base_sources
            base/platform/sdl2/sdl2_platform_factory.cc
            base/platform/sdl2/sdl2_platform_factory.h
            base/platform/sdl2/sdl2_runloop_interface.cc
            base/platform/sdl2/sdl2_runloop_interface.h
            base/platform/sdl2/sdl2_single_thread_task_executor_interface.cc
            base/platform/sdl2/sdl2_single_thread_task_executor_interface.h
            base/platform/sdl2/sdl2_single_thread_task_runner.cc
            base/platform/sdl2/sdl2_single_thread_task_runner.h
            base/platform/sdl2/sdl2_thread_interface.cc
            base/platform/sdl2/sdl2_thread_interface.h
    )
endif ()

# sources for nes
set(nes_sources
        emulator/nes/apu.cc
        emulator/nes/apu.h
        emulator/nes/audio_device.cc
        emulator/nes/audio_device.h
        emulator/nes/cartridge.cc
        emulator/nes/cartridge.h
        emulator/nes/controller.cc
        emulator/nes/controller.h
        emulator/nes/nes_export.h
        emulator/nes/cpu_bus.cc
        emulator/nes/cpu_bus.h
        emulator/nes/cpu.cc
        emulator/nes/cpu.h
        emulator/nes/cpu_observer.cc
        emulator/nes/cpu_observer.h
        emulator/nes/debug/debug_port.cc
        emulator/nes/debug/debug_port.h
        emulator/nes/debug/disassembly.cc
        emulator/nes/debug/disassembly.h
        emulator/nes/emulator.cc
        emulator/nes/emulator.h
        emulator/nes/emulator_impl.cc
        emulator/nes/emulator_impl.h
        emulator/nes/emulator_states.cc
        emulator/nes/emulator_states.h
        emulator/nes/io_devices.cc
        emulator/nes/io_devices.h
        emulator/nes/mapper.cc
        emulator/nes/mapper.h
        emulator/nes/nes_buildflags.h
        emulator/nes/mappers/mapper000.cc
        emulator/nes/mappers/mapper000.h
        emulator/nes/mappers/mapper001.cc
        emulator/nes/mappers/mapper001.h
        emulator/nes/mappers/mapper002.cc
        emulator/nes/mappers/mapper002.h
        emulator/nes/mappers/mapper003.cc
        emulator/nes/mappers/mapper003.h
        emulator/nes/mappers/mapper004.cc
        emulator/nes/mappers/mapper004.h
        emulator/nes/mappers/mapper005.cc
        emulator/nes/mappers/mapper005.h
        emulator/nes/mappers/mapper007.cc
        emulator/nes/mappers/mapper007.h
        emulator/nes/mappers/mapper009.cc
        emulator/nes/mappers/mapper009.h
        emulator/nes/mappers/mapper010.cc
        emulator/nes/mappers/mapper010.h
        emulator/nes/mappers/mapper011.cc
        emulator/nes/mappers/mapper011.h
        emulator/nes/mappers/mapper033.cc
        emulator/nes/mappers/mapper033.h
        emulator/nes/mappers/mapper040.cc
        emulator/nes/mappers/mapper040.h
        emulator/nes/mappers/mapper048.cc
        emulator/nes/mappers/mapper048.h
        emulator/nes/mappers/mapper066.cc
        emulator/nes/mappers/mapper066.h
        emulator/nes/mappers/mapper074.cc
        emulator/nes/mappers/mapper074.h
        emulator/nes/mappers/mapper075.cc
        emulator/nes/mappers/mapper075.h
        emulator/nes/mappers/mapper087.cc
        emulator/nes/mappers/mapper087.h
        emulator/nes/mappers/mapper185.cc
        emulator/nes/mappers/mapper185.h
        emulator/nes/mappers/mapper013.cc
        emulator/nes/mappers/mapper013.h
        emulator/nes/mappers/mapper016.cc
        emulator/nes/mappers/mapper016.h
        emulator/nes/mappers/mapper018.cc
        emulator/nes/mappers/mapper018.h
        emulator/nes/mappers/mapper019.cc
        emulator/nes/mappers/mapper019.h
        emulator/nes/mappers/mapper023.cc
        emulator/nes/mappers/mapper023.h
        emulator/nes/mappers/mapper024.cc
        emulator/nes/mappers/mapper024.h
        emulator/nes/mappers/mapper032.cc
        emulator/nes/mappers/mapper032.h
        emulator/nes/mappers/mapper034.cc
        emulator/nes/mappers/mapper034.h
        emulator/nes/mappers/mapper037.cc
        emulator/nes/mappers/mapper037.h
        emulator/nes/mappers/mapper038.cc
        emulator/nes/mappers/mapper038.h
        emulator/nes/mappers/mapper041.cc
        emulator/nes/mappers/mapper041.h
        emulator/nes/mappers/mapper046.cc
        emulator/nes/mappers/mapper046.h
        emulator/nes/mappers/mapper047.cc
        emulator/nes/mappers/mapper047.h
        emulator/nes/mappers/mapper064.cc
        emulator/nes/mappers/mapper064.h
        emulator/nes/mappers/mapper065.cc
        emulator/nes/mappers/mapper065.h
        emulator/nes/mappers/mapper067.cc
        emulator/nes/mappers/mapper067.h
        emulator/nes/mappers/mapper068.cc
        emulator/nes/mappers/mapper068.h
        emulator/nes/mappers/mapper069.cc
        emulator/nes/mappers/mapper069.h
        emulator/nes/mappers/mapper070.cc
        emulator/nes/mappers/mapper070.h
        emulator/nes/mappers/mapper071.cc
        emulator/nes/mappers/mapper071.h
        emulator/nes/mappers/mapper072.cc
        emulator/nes/mappers/mapper072.h
        emulator/nes/mappers/mapper073.cc
        emulator/nes/mappers/mapper073.h
        emulator/nes/mappers/mapper077.cc
        emulator/nes/mappers/mapper077.h
        emulator/nes/mappers/mapper078.cc
        emulator/nes/mappers/mapper078.h
        emulator/nes/mappers/mapper079.cc
        emulator/nes/mappers/mapper079.h
        emulator/nes/mappers/mapper080.cc
        emulator/nes/mappers/mapper080.h
        emulator/nes/mappers/mapper085.cc
        emulator/nes/mappers/mapper085.h
        emulator/nes/mappers/mapper086.cc
        emulator/nes/mappers/mapper086.h
        emulator/nes/mappers/mapper089.cc
        emulator/nes/mappers/mapper089.h
        emulator/nes/mappers/mapper092.cc
        emulator/nes/mappers/mapper092.h
        emulator/nes/mappers/mapper093.cc
        emulator/nes/mappers/mapper093.h
        emulator/nes/mappers/mapper094.cc
        emulator/nes/mappers/mapper094.h
        emulator/nes/mappers/mapper096.cc
        emulator/nes/mappers/mapper096.h
        emulator/nes/mappers/mapper097.cc
        emulator/nes/mappers/mapper097.h
        emulator/nes/mappers/mapper104.cc
        emulator/nes/mappers/mapper104.h
        emulator/nes/mappers/mapper105.cc
        emulator/nes/mappers/mapper105.h
        emulator/nes/mappers/mapper112.cc
        emulator/nes/mappers/mapper112.h
        emulator/nes/mappers/mapper113.cc
        emulator/nes/mappers/mapper113.h
        emulator/nes/mappers/mapper115.cc
        emulator/nes/mappers/mapper115.h
        emulator/nes/mappers/mapper118.cc
        emulator/nes/mappers/mapper118.h
        emulator/nes/mappers/mapper119.cc
        emulator/nes/mappers/mapper119.h
        emulator/nes/mappers/mapper132.cc
        emulator/nes/mappers/mapper132.h
        emulator/nes/mappers/mapper133.cc
        emulator/nes/mappers/mapper133.h
        emulator/nes/mappers/mapper137.cc
        emulator/nes/mappers/mapper137.h
        emulator/nes/mappers/mapper139.cc
        emulator/nes/mappers/mapper139.h
        emulator/nes/mappers/mapper140.cc
        emulator/nes/mappers/mapper140.h
        emulator/nes/mappers/mapper141.cc
        emulator/nes/mappers/mapper141.h
        emulator/nes/mappers/mapper143.cc
        emulator/nes/mappers/mapper143.h
        emulator/nes/mappers/mapper147.cc
        emulator/nes/mappers/mapper147.h
        emulator/nes/mappers/mapper148.cc
        emulator/nes/mappers/mapper148.h
        emulator/nes/mappers/mapper150.cc
        emulator/nes/mappers/mapper150.h
        emulator/nes/mappers/mapper156.cc
        emulator/nes/mappers/mapper156.h
        emulator/nes/mappers/mapper168.cc
        emulator/nes/mappers/mapper168.h
        emulator/nes/mappers/mapper176.cc
        emulator/nes/mappers/mapper176.h
        emulator/nes/mappers/mapper177.cc
        emulator/nes/mappers/mapper177.h
        emulator/nes/mappers/mapper178.cc
        emulator/nes/mappers/mapper178.h
        emulator/nes/mappers/mapper180.cc
        emulator/nes/mappers/mapper180.h
        emulator/nes/mappers/mapper184.cc
        emulator/nes/mappers/mapper184.h
        emulator/nes/mappers/mapper186.cc
        emulator/nes/mappers/mapper186.h
        emulator/nes/mappers/mapper188.cc
        emulator/nes/mappers/mapper188.h
        emulator/nes/mappers/mapper189.cc
        emulator/nes/mappers/mapper189.h
        emulator/nes/mappers/mapper193.cc
        emulator/nes/mappers/mapper193.h
        emulator/nes/mappers/mapper206.cc
        emulator/nes/mappers/mapper206.h
        emulator/nes/mappers/mapper216.cc
        emulator/nes/mappers/mapper216.h
        emulator/nes/mappers/mapper228.cc
        emulator/nes/mappers/mapper228.h
        emulator/nes/mappers/mapper232.cc
        emulator/nes/mappers/mapper232.h
        emulator/nes/mappers/mapper234.cc
        emulator/nes/mappers/mapper234.h
        emulator/nes/mappers/mapper240.cc
        emulator/nes/mappers/mapper240.h
        emulator/nes/mappers/mapper241.cc
        emulator/nes/mappers/mapper241.h
        emulator/nes/mappers/mapper244.cc
        emulator/nes/mappers/mapper244.h
        emulator/nes/mappers/mapper245.cc
        emulator/nes/mappers/mapper245.h
        emulator/nes/mappers/mapper246.cc
        emulator/nes/mappers/mapper246.h
        emulator/nes/mappers/mapper355.cc
        emulator/nes/mappers/mapper355.h
        emulator/nes/mappers/mapper512.cc
        emulator/nes/mappers/mapper512.h
        emulator/nes/opcodes.cc
        emulator/nes/opcodes.h
        emulator/nes/palette.cc
        emulator/nes/palette.h
        emulator/nes/ppu_observer.cc
        emulator/nes/ppu_observer.h
        emulator/nes/ppu_bus.cc
        emulator/nes/ppu_bus.h
        emulator/nes/ppu_patch.cc
        emulator/nes/ppu_patch.h
        emulator/nes/ppu_texture_capture.cc
        emulator/nes/ppu_texture_capture.h
        emulator/nes/ppu.cc
        emulator/nes/ppu.h
        emulator/nes/render_device.cc
        emulator/nes/render_device.h
        emulator/nes/registers.h
        emulator/nes/rom_data.cc
        emulator/nes/rom_data.h
        emulator/nes/rom_hash.cc
        emulator/nes/rom_hash.h
        emulator/nes/types.h
)

set(sources
        ${base_sources}
        ${emulator_sources}
        ${gba_sources}
        ${nes_sources}
)
# Setup for static kiwi library
set(STATIC_PROJECT_NAME ${PROJECT_NAME}_static)
KIWI_ADD_LIBRARY(${STATIC_PROJECT_NAME} STATIC ${sources})
target_include_directories(${STATIC_PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_compile_definitions(${STATIC_PROJECT_NAME} PUBLIC
        KIWI_STATIC_LIBRARY
        KIWI_ENABLE_APU_FRAME_BOUNDARY_CLOCK_FIX=$<BOOL:${KIWI_ENABLE_APU_FRAME_BOUNDARY_CLOCK_FIX}>
        KIWI_ENABLE_INES1_TV_SYSTEM_FIX=$<BOOL:${KIWI_ENABLE_INES1_TV_SYSTEM_FIX}>
)
if (USE_SDL2)
    target_compile_definitions(${STATIC_PROJECT_NAME} PRIVATE USE_SDL2)
endif ()
target_link_libraries(${STATIC_PROJECT_NAME} PRIVATE
        mGBA::core
        nes_apu
        glog::glog
        ${KIWI_SDL2_TARGET}
        zlibstatic
)

if (WIN32)
    target_link_libraries(${STATIC_PROJECT_NAME} PRIVATE Shlwapi)
    target_compile_definitions(${STATIC_PROJECT_NAME} PUBLIC
            NOMINMAX
            UNICODE
            _UNICODE
    )
endif ()

# Disable warnings in MSVC
if (MSVC)
    target_compile_options(${STATIC_PROJECT_NAME} PUBLIC /wd4275 /wd4251 /wd4267 /wd4244)
endif ()

if (NOT EMSCRIPTEN AND NOT KIWI_SWITCH)
    # Setup for dynamic library
    KIWI_ADD_LIBRARY(${PROJECT_NAME} SHARED ${sources})
    target_include_directories(${PROJECT_NAME} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/..
    )

    target_compile_definitions(${PROJECT_NAME} PRIVATE
            KIWI_IMPL
            KIWI_ENABLE_APU_FRAME_BOUNDARY_CLOCK_FIX=$<BOOL:${KIWI_ENABLE_APU_FRAME_BOUNDARY_CLOCK_FIX}>
            KIWI_ENABLE_INES1_TV_SYSTEM_FIX=$<BOOL:${KIWI_ENABLE_INES1_TV_SYSTEM_FIX}>
    )
    if (USE_SDL2)
        target_compile_definitions(${PROJECT_NAME} PRIVATE USE_SDL2)
    endif ()
    target_link_libraries(${PROJECT_NAME} PRIVATE
            mGBA::core
            nes_apu
            glog::glog
            SDL2-static
            zlibstatic
    )

    if (WIN32)
        target_link_libraries(${PROJECT_NAME} PRIVATE Shlwapi)
        target_compile_definitions(${PROJECT_NAME} PUBLIC
                NOMINMAX
                UNICODE
                _UNICODE
        )
    endif ()

    # Disable warnings in MSVC
    if (MSVC)
        target_compile_options(${PROJECT_NAME} PUBLIC /wd4275 /wd4251 /wd4267 /wd4244)
    endif ()

    # Add testing directory
    add_subdirectory(testing)
endif ()
