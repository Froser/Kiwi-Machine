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

# Sources for base
set(base_sources
        base/atomic_ref_count.h
        base/base_export.h
        base/check.h
        base/compiler_specific.h
        base/containers/adapters.h
        base/cxx20_to_address.h
        base/dcheck_is_on.h
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
        base/scoped_generic.h
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
        base/runloop.cc
        base/runloop.h
)

if (MACOSX)
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
        nes/bus.cc
        nes/bus.h
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
        nes/mappers/mapper007.cc
        nes/mappers/mapper007.h
        nes/mappers/mapper040.cc
        nes/mappers/mapper040.h
        nes/mappers/mapper066.cc
        nes/mappers/mapper066.h
        nes/mappers/mapper074.cc
        nes/mappers/mapper074.h
        nes/mappers/mapper087.cc
        nes/mappers/mapper087.h
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
        nes/ppu.cc
        nes/ppu.h
        nes/registers.h
        nes/rom_data.cc
        nes/rom_data.h
        nes/types.h
)

set(sources
        ${base_sources}
        ${nes_sources}
)
# Setup for static kiwi library
set(STATIC_PROJECT_NAME ${PROJECT_NAME}_static)
KIWI_ADD_LIBRARY(${STATIC_PROJECT_NAME} STATIC ${sources})
target_compile_definitions(${STATIC_PROJECT_NAME} PUBLIC KIWI_STATIC_LIBRARY)
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

    target_compile_definitions(${PROJECT_NAME} PRIVATE KIWI_IMPL)
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
endif ()
