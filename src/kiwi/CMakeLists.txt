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
        base/auto_reset.h
        base/base_export.h
        base/base64.cc
        base/base64.h
        base/check.h
        base/compiler_specific.h
        base/containers/adapters.h
        base/containers/buffer_iterator.h
        base/containers/checked_iterators.h
        base/containers/linked_list.cc
        base/containers/linked_list.h
        base/containers/util.h
        base/command_line.cc
        base/command_line.h
        base/cxx20_to_address.h
        base/dcheck_is_on.h
        base/environment.cc
        base/environment.h
        base/file_version_info.h
        base/immediate_crash.h
        base/location.cc
        base/location.h
        base/logging.h
        base/native_library.cc
        base/native_library.h
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
        base/functional/callback_helpers.cc
        base/functional/callback_helpers.h
        base/functional/callback_internal.cc
        base/functional/callback_internal.h
        base/functional/callback_tags.h
        base/hash/hash.cc
        base/hash/hash.h
        base/memory/scoped_refptr.h
        base/memory/scoped_policy.h
        base/memory/ptr_util.h
        base/memory/raw_scoped_refptr_mismatch_checker.h
        base/memory/ref_counted.cc
        base/memory/ref_counted.h
        base/memory/weak_ptr.cc
        base/memory/weak_ptr.h
        base/message_loop/message_pump_type.h
        base/no_destructor.h
        base/numerics/safe_conversions.h
        base/numerics/safe_conversions_arm_impl.h
        base/numerics/safe_conversions_impl.h
        base/posix/eintr_wrapper.h
        base/platform/platform_factory.cc
        base/platform/platform_factory.h
        base/scoped_clear_last_error.h
        base/scoped_generic.h
        base/scoped_native_library.cc
        base/scoped_native_library.h
        base/scoped_observation.h
        base/scoped_observation_traits.h
        base/strings/string_number_conversions.cc
        base/strings/string_number_conversions.h
        base/strings/string_piece.h
        base/strings/string_split.cc
        base/strings/string_split.h
        base/strings/string_split_internal.h
        base/strings/string_tokenizer.h
        base/strings/string_util.cc
        base/strings/string_util.h
        base/strings/string_util_constants.cc
        base/strings/string_util_impl_helpers.h
        base/strings/string_util_internal.h
        base/strings/stringprintf.cc
        base/strings/stringprintf.h
        base/strings/sys_string_conversions.h
        base/strings/utf_string_conversions.cc
        base/strings/utf_string_conversions.h
        base/strings/string_util_win.cc
        base/strings/string_util_win.h
        base/synchronization/atomic_flag.cc
        base/synchronization/atomic_flag.h
        base/synchronization/lock_impl.h
        base/synchronization/lock.h
        base/synchronization/waitable_event.cc
        base/synchronization/waitable_event.h
        base/system/sys_info.cc
        base/system/sys_info.h
        base/runloop.cc
        base/runloop.h
        base/task/default_delayed_task_handle_delegate.cc
        base/task/default_delayed_task_handle_delegate.h
        base/task/sequenced_task_runner.cc
        base/task/sequenced_task_runner.h
        base/task/bind_post_task.h
        base/task/bind_post_task_internal.h
        base/task/delayed_task_handle.cc
        base/task/delayed_task_handle.h
        base/task/single_thread_task_executor.cc
        base/task/single_thread_task_executor.h
        base/task/single_thread_task_runner.cc
        base/task/single_thread_task_runner.h
        base/task/post_task_and_reply_with_result_internal.h
        base/time/tick_clock.cc
        base/time/tick_clock.h
        base/time/time.cc
        base/time/time.h
        base/timer/timer.cc
        base/timer/timer.h
        base/template_util.h
        base/third_party/cityhash/city.cc
        base/third_party/cityhash/city.h
        base/thread_annotations.h
        base/threading/platform_thread_ref.cc
        base/threading/platform_thread_ref.h
        base/threading/thread.cc
        base/threading/thread.h
        base/types/always_false.h
        base/types/expected.h
        base/observer_list.h
        base/observer_list_internal.cc
        base/observer_list_internal.h
        base/observer_list_types.cc
        base/observer_list_types.h
        base/process/environment_internal.cc
        base/process/environment_internal.h
        base/process/kill.cc
        base/process/kill.h
        base/process/process_handle.cc
        base/process/process_handle.h
        base/process/process_iterator.cc
        base/process/process_iterator.h
        base/process/process_metrics.cc
        base/process/process_metrics.h
        base/process/process.h
        base/process/launch.cc
        base/process/launch.h
        base/uuid.cc
        base/uuid.h
        base/version.cc
        base/version.h
)

if (MACOSX)
    list(APPEND base_sources
            base/apple/bridging.h
            base/apple/dispatch_source_mach.cc
            base/apple/dispatch_source_mach.h
            base/apple/scoped_cftyperef.h
            base/apple/scoped_dispatch_object.h
            base/apple/scoped_mach_port.cc
            base/apple/scoped_mach_port.h
            base/apple/scoped_nsautorelease_pool.h
            base/apple/scoped_nsautorelease_pool.mm
            base/apple/scoped_typeref.h
            base/files/file_util_apple.mm
            base/mac/mach_port_rendezvous.cc
            base/mac/mach_port_rendezvous.h
            base/mac/scoped_block.h
            base/mac/scoped_ioobject.h
            base/mac/scoped_mach_msg_destroy.h
            base/mac/scoped_typeref.h
            base/native_library_mac.mm
            base/process/launch_mac.cc
            base/process/process_iterator_mac.cc
            base/process/process_handle_mac.cc
            base/process/process_metrics_apple.mm
            base/system/sys_info_mac.mm
            base/strings/sys_string_conversions_apple.mm
    )
endif ()

if (UNIX)
    list(APPEND base_sources
            base/files/file_enumerator_posix.cc
            base/files/file_posix.cc
            base/files/file_util_posix.cc
            base/process/process_posix.cc
            base/process/process_handle_posix.cc
            base/process/kill_posix.cc
            base/synchronization/lock_impl_posix.cc
    )
endif ()

if (WIN32)
    list(APPEND base_sources
            base/files/file_enumerator_win.cc
            base/files/file_util_win.cc
            base/files/file_win.cc
            base/file_version_info_win.cc
            base/file_version_info_win.h
            base/native_library_win.cc
            base/process/process_metrics_win.cc
            base/scoped_clear_last_error_win.cc
            base/strings/sys_string_conversions_win.cc
            base/system/sys_info_win.cc
            base/win/access_token.cc
            base/win/access_token.h
            base/win/registry.cc
            base/win/registry.h
            base/win/resource_util.cc
            base/win/resource_util.h
            base/win/scoped_pdh_query.cc
            base/win/scoped_pdh_query.h
            base/win/shlwapi.h
            base/win/windows_types.h
            base/win/win_handle_types.h
            base/win/windows_version.cc
            base/win/windows_version.h
            base/win/com_init_balancer.cc
            base/win/com_init_balancer.h
            base/win/propvarutil.h
            base/win/scoped_com_initializer.cc
            base/win/scoped_com_initializer.h
            base/win/scoped_variant.cc
            base/win/scoped_variant.h
            base/win/scoped_windows_thread_environment.h
            base/win/variant_conversions.h
            base/win/resource_exhaustion.cc
            base/win/resource_exhaustion.h
            base/win/scoped_bstr.cc
            base/win/scoped_bstr.h
            base/win/scoped_co_mem.h
            base/win/scoped_handle.cc
            base/win/scoped_handle.h
            base/synchronization/lock_impl_win.cc
            base/win/scoped_handle_verifier.cc
            base/win/scoped_handle_verifier.h
            base/win/current_module.h
            base/process/process_iterator_win.cc
            base/process/process_handle_win.cc
            base/process/process_win.cc
            base/win/access_token.cc
            base/win/access_token.h
            base/win/scoped_process_information.cc
            base/win/scoped_process_information.h
            base/process/launch_win.cc
            base/process/kill_win.cc
            base/win/startup_information.cc
            base/win/startup_information.h
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
target_link_libraries(${STATIC_PROJECT_NAME} PRIVATE nes_apu glog::glog SDL2-static zlibstatic modp_b64)
target_link_libraries(${STATIC_PROJECT_NAME} PUBLIC glog::glog)
target_include_directories(${STATIC_PROJECT_NAME}
        PUBLIC
        ${glog_SOURCE_DIR}/src
        ${glog_BINARY_DIR}/..
        ${glog_BINARY_DIR}
        ${kiwi_BINARY_DIR}
)

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
    target_link_libraries(${PROJECT_NAME} PRIVATE nes_apu SDL2-static zlibstatic modp_b64)
    target_link_libraries(${PROJECT_NAME} PUBLIC glog::glog)
    target_include_directories(${PROJECT_NAME}
            PUBLIC
            ${glog_SOURCE_DIR}/src
            ${glog_BINARY_DIR}/..
            ${glog_BINARY_DIR}
            ${kiwi_BINARY_DIR}
    )

    if (WIN32)
        target_link_libraries(${PROJECT_NAME} PRIVATE Shlwapi)
        target_compile_definitions(${PROJECT_NAME} PUBLIC UNICODE _UNICODE)
    endif ()

    # Disable warnings in MSVC
    if (MSVC)
        target_compile_options(${PROJECT_NAME} PUBLIC /wd4275 /wd4251 /wd4267 /wd4244)
    endif ()
endif ()
