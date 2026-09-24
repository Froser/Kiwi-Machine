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

add_subdirectory(nes)

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

if (APPLE)
    list(APPEND base_sources
            base/files/file_util_apple.mm
            base/mac/scoped_block.h
            base/mac/scoped_typeref.h
    )
endif ()

if (UNIX)
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
        nes/apu.cc
        nes/apu.h
        nes/cartridge.cc
        nes/cartridge.h
        nes/controller.cc
        nes/controller.h
        nes/nes_export.h
        nes/cpu_bus.cc
        nes/cpu_bus.h
        nes/cpu.cc
        nes/cpu.h
        nes/cpu_observer.cc
        nes/cpu_observer.h
        nes/debug/debug_port.cc
        nes/debug/debug_port.h
        nes/debug/disassembly.cc
        nes/debug/disassembly.h
        nes/emulator.cc
        nes/emulator.h
        nes/emulator_impl.cc
        nes/emulator_impl.h
        nes/emulator_states.cc
        nes/emulator_states.h
        nes/io_devices.cc
        nes/io_devices.h
        nes/mapper.cc
        nes/mapper.h
        nes/nes_buildflags.h
        nes/mappers/mapper000.cc
        nes/mappers/mapper000.h
        nes/mappers/mapper001.cc
        nes/mappers/mapper001.h
        nes/mappers/mapper002.cc
        nes/mappers/mapper002.h
        nes/mappers/mapper003.cc
        nes/mappers/mapper003.h
        nes/mappers/mapper004.cc
        nes/mappers/mapper004.h
        nes/mappers/mapper005.cc
        nes/mappers/mapper005.h
        nes/mappers/mapper007.cc
        nes/mappers/mapper007.h
        nes/mappers/mapper009.cc
        nes/mappers/mapper009.h
        nes/mappers/mapper010.cc
        nes/mappers/mapper010.h
        nes/mappers/mapper011.cc
        nes/mappers/mapper011.h
        nes/mappers/mapper033.cc
        nes/mappers/mapper033.h
        nes/mappers/mapper040.cc
        nes/mappers/mapper040.h
        nes/mappers/mapper048.cc
        nes/mappers/mapper048.h
        nes/mappers/mapper066.cc
        nes/mappers/mapper066.h
        nes/mappers/mapper074.cc
        nes/mappers/mapper074.h
        nes/mappers/mapper075.cc
        nes/mappers/mapper075.h
        nes/mappers/mapper087.cc
        nes/mappers/mapper087.h
        nes/mappers/mapper185.cc
        nes/mappers/mapper185.h
        nes/mappers/mapper013.cc
        nes/mappers/mapper013.h
        nes/mappers/mapper016.cc
        nes/mappers/mapper016.h
        nes/mappers/mapper018.cc
        nes/mappers/mapper018.h
        nes/mappers/mapper019.cc
        nes/mappers/mapper019.h
        nes/mappers/mapper023.cc
        nes/mappers/mapper023.h
        nes/mappers/mapper024.cc
        nes/mappers/mapper024.h
        nes/mappers/mapper032.cc
        nes/mappers/mapper032.h
        nes/mappers/mapper034.cc
        nes/mappers/mapper034.h
        nes/mappers/mapper037.cc
        nes/mappers/mapper037.h
        nes/mappers/mapper038.cc
        nes/mappers/mapper038.h
        nes/mappers/mapper041.cc
        nes/mappers/mapper041.h
        nes/mappers/mapper046.cc
        nes/mappers/mapper046.h
        nes/mappers/mapper047.cc
        nes/mappers/mapper047.h
        nes/mappers/mapper064.cc
        nes/mappers/mapper064.h
        nes/mappers/mapper065.cc
        nes/mappers/mapper065.h
        nes/mappers/mapper067.cc
        nes/mappers/mapper067.h
        nes/mappers/mapper068.cc
        nes/mappers/mapper068.h
        nes/mappers/mapper069.cc
        nes/mappers/mapper069.h
        nes/mappers/mapper070.cc
        nes/mappers/mapper070.h
        nes/mappers/mapper071.cc
        nes/mappers/mapper071.h
        nes/mappers/mapper072.cc
        nes/mappers/mapper072.h
        nes/mappers/mapper073.cc
        nes/mappers/mapper073.h
        nes/mappers/mapper077.cc
        nes/mappers/mapper077.h
        nes/mappers/mapper078.cc
        nes/mappers/mapper078.h
        nes/mappers/mapper079.cc
        nes/mappers/mapper079.h
        nes/mappers/mapper080.cc
        nes/mappers/mapper080.h
        nes/mappers/mapper085.cc
        nes/mappers/mapper085.h
        nes/mappers/mapper086.cc
        nes/mappers/mapper086.h
        nes/mappers/mapper089.cc
        nes/mappers/mapper089.h
        nes/mappers/mapper092.cc
        nes/mappers/mapper092.h
        nes/mappers/mapper093.cc
        nes/mappers/mapper093.h
        nes/mappers/mapper094.cc
        nes/mappers/mapper094.h
        nes/mappers/mapper096.cc
        nes/mappers/mapper096.h
        nes/mappers/mapper097.cc
        nes/mappers/mapper097.h
        nes/mappers/mapper104.cc
        nes/mappers/mapper104.h
        nes/mappers/mapper105.cc
        nes/mappers/mapper105.h
        nes/mappers/mapper112.cc
        nes/mappers/mapper112.h
        nes/mappers/mapper113.cc
        nes/mappers/mapper113.h
        nes/mappers/mapper115.cc
        nes/mappers/mapper115.h
        nes/mappers/mapper118.cc
        nes/mappers/mapper118.h
        nes/mappers/mapper119.cc
        nes/mappers/mapper119.h
        nes/mappers/mapper132.cc
        nes/mappers/mapper132.h
        nes/mappers/mapper133.cc
        nes/mappers/mapper133.h
        nes/mappers/mapper137.cc
        nes/mappers/mapper137.h
        nes/mappers/mapper139.cc
        nes/mappers/mapper139.h
        nes/mappers/mapper140.cc
        nes/mappers/mapper140.h
        nes/mappers/mapper141.cc
        nes/mappers/mapper141.h
        nes/mappers/mapper143.cc
        nes/mappers/mapper143.h
        nes/mappers/mapper147.cc
        nes/mappers/mapper147.h
        nes/mappers/mapper148.cc
        nes/mappers/mapper148.h
        nes/mappers/mapper150.cc
        nes/mappers/mapper150.h
        nes/mappers/mapper156.cc
        nes/mappers/mapper156.h
        nes/mappers/mapper168.cc
        nes/mappers/mapper168.h
        nes/mappers/mapper176.cc
        nes/mappers/mapper176.h
        nes/mappers/mapper177.cc
        nes/mappers/mapper177.h
        nes/mappers/mapper178.cc
        nes/mappers/mapper178.h
        nes/mappers/mapper180.cc
        nes/mappers/mapper180.h
        nes/mappers/mapper184.cc
        nes/mappers/mapper184.h
        nes/mappers/mapper186.cc
        nes/mappers/mapper186.h
        nes/mappers/mapper188.cc
        nes/mappers/mapper188.h
        nes/mappers/mapper189.cc
        nes/mappers/mapper189.h
        nes/mappers/mapper193.cc
        nes/mappers/mapper193.h
        nes/mappers/mapper206.cc
        nes/mappers/mapper206.h
        nes/mappers/mapper216.cc
        nes/mappers/mapper216.h
        nes/mappers/mapper228.cc
        nes/mappers/mapper228.h
        nes/mappers/mapper232.cc
        nes/mappers/mapper232.h
        nes/mappers/mapper234.cc
        nes/mappers/mapper234.h
        nes/mappers/mapper240.cc
        nes/mappers/mapper240.h
        nes/mappers/mapper241.cc
        nes/mappers/mapper241.h
        nes/mappers/mapper244.cc
        nes/mappers/mapper244.h
        nes/mappers/mapper245.cc
        nes/mappers/mapper245.h
        nes/mappers/mapper246.cc
        nes/mappers/mapper246.h
        nes/mappers/mapper355.cc
        nes/mappers/mapper355.h
        nes/mappers/mapper512.cc
        nes/mappers/mapper512.h
        nes/opcodes.cc
        nes/opcodes.h
        nes/palette.cc
        nes/palette.h
        nes/ppu_observer.cc
        nes/ppu_observer.h
        nes/ppu_bus.cc
        nes/ppu_bus.h
        nes/ppu_patch.cc
        nes/ppu_patch.h
        nes/ppu_texture_capture.cc
        nes/ppu_texture_capture.h
        nes/ppu.cc
        nes/ppu.h
        nes/registers.h
        nes/rom_data.cc
        nes/rom_data.h
        nes/rom_hash.cc
        nes/rom_hash.h
        nes/types.h
)

set(sources
        ${base_sources}
        ${nes_sources}
)
# Setup for static kiwi library
set(STATIC_PROJECT_NAME ${PROJECT_NAME}_static)
KIWI_ADD_LIBRARY(${STATIC_PROJECT_NAME} STATIC ${sources})
target_compile_definitions(${STATIC_PROJECT_NAME} PUBLIC
        KIWI_STATIC_LIBRARY
        KIWI_ENABLE_APU_FRAME_BOUNDARY_CLOCK_FIX=$<BOOL:${KIWI_ENABLE_APU_FRAME_BOUNDARY_CLOCK_FIX}>
        KIWI_ENABLE_INES1_TV_SYSTEM_FIX=$<BOOL:${KIWI_ENABLE_INES1_TV_SYSTEM_FIX}>
)
if (USE_SDL2)
    target_compile_definitions(${STATIC_PROJECT_NAME} PRIVATE USE_SDL2)
endif ()
target_link_libraries(${STATIC_PROJECT_NAME} PRIVATE nes_apu glog::glog SDL2-static zlibstatic)

if (WIN32)
    target_link_libraries(${STATIC_PROJECT_NAME} PRIVATE Shlwapi)
    target_compile_definitions(${STATIC_PROJECT_NAME} PUBLIC UNICODE _UNICODE)
endif ()

# Disable warnings in MSVC
if (MSVC)
    target_compile_options(${STATIC_PROJECT_NAME} PUBLIC /wd4275 /wd4251 /wd4267 /wd4244)
endif ()

if (NOT EMSCRIPTEN)
    # Setup for dynamic library
    KIWI_ADD_LIBRARY(${PROJECT_NAME} SHARED ${sources})

    target_compile_definitions(${PROJECT_NAME} PRIVATE
            KIWI_IMPL
            KIWI_ENABLE_APU_FRAME_BOUNDARY_CLOCK_FIX=$<BOOL:${KIWI_ENABLE_APU_FRAME_BOUNDARY_CLOCK_FIX}>
            KIWI_ENABLE_INES1_TV_SYSTEM_FIX=$<BOOL:${KIWI_ENABLE_INES1_TV_SYSTEM_FIX}>
    )
    if (USE_SDL2)
        target_compile_definitions(${PROJECT_NAME} PRIVATE USE_SDL2)
    endif ()
    target_link_libraries(${PROJECT_NAME} PRIVATE nes_apu glog::glog SDL2-static zlibstatic)

    if (WIN32)
        target_link_libraries(${PROJECT_NAME} PRIVATE Shlwapi)
        target_compile_definitions(${PROJECT_NAME} PUBLIC UNICODE _UNICODE)
    endif ()

    # Disable warnings in MSVC
    if (MSVC)
        target_compile_options(${PROJECT_NAME} PUBLIC /wd4275 /wd4251 /wd4267 /wd4244)
    endif ()

    # Add testing directory
    add_subdirectory(testing)
endif ()
