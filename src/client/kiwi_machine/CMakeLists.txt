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

project(kiwi_machine CXX)

set(CMAKE_CXX_STANDARD 17)

include_directories(
        .
        ../kiwi_machine_core
)

set(Sources
        main.cc
)

if (WIN32)
    set(Resources
            build/win/kiwi_machine.rc
    )
endif ()

if (APPLE)
    if (IOS)
        set(Resources
                ../../../kiwi.png
                build/ios/Assets.xcassets
                build/ios/KiwiMachineLaunchScreen.storyboard
        )
    else ()
        set(Resources
                build/macos/kiwi.icns
        )
    endif ()
endif ()

if (WIN32)
    KIWI_ADD_EXECUTABLE(${PROJECT_NAME} WIN32 ${Sources} ${Resources})
elseif (APPLE)
    KIWI_ADD_EXECUTABLE(${PROJECT_NAME} MACOSX_BUNDLE ${Sources} ${Resources})
else ()
    KIWI_ADD_EXECUTABLE(${PROJECT_NAME} ${Sources})
endif ()

if (APPLE)
    if (IOS)
        set_target_properties(${PROJECT_NAME}
                PROPERTIES
                MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/build/ios/Info.plist
                XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon"
                XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO"
                XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO"
                XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY ""
                XCODE_ATTRIBUTE_DEVELOPMENT_TEAM ""
                XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "12.0"
                RESOURCE "${Resources}"
        )
        set_source_files_properties(build/ios/Assets.xcassets PROPERTIES
                MACOSX_PACKAGE_LOCATION Resources/Assets.xcassets
        )
    else ()
        set_target_properties(${PROJECT_NAME}
                PROPERTIES
                OUTPUT_NAME "Kiwi Machine"
                MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/build/macos/Info.plist
                RESOURCE "${Resources}"
        )

        add_custom_target(
                "KiwiMachine_MakeDMG"
                ALL
                COMMAND hdiutil create ./KiwiMachine.dmg -volname "Kiwi Machine" -ov -srcfolder "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>"
                WORKING_DIRECTORY "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>/.."
        )
    endif ()
endif ()

FIND_PACKAGE(Python3 REQUIRED)
if (DEFINED KIWI_PACKAGE_DIR)
    message(STATUS "KIWI_PACKAGE_DIR is ${KIWI_PACKAGE_DIR}")
    if (WIN32)
        add_custom_command(
                TARGET ${PROJECT_NAME}
                POST_BUILD
                COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/build/copy_packages.py
                ARGS "${KIWI_PACKAGE_DIR}" "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
                VERBATIM
        )
    elseif (APPLE)
        if (IOS)
            add_custom_command(
                    TARGET ${PROJECT_NAME}
                    POST_BUILD
                    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/build/copy_packages.py
                    ARGS "${KIWI_PACKAGE_DIR}" "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>" "$(EFFECTIVE_PLATFORM_NAME)"
                    VERBATIM
            )
        else ()
            add_custom_command(
                    TARGET ${PROJECT_NAME}
                    POST_BUILD
                    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/build/copy_packages.py
                    ARGS "${KIWI_PACKAGE_DIR}" "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>/Contents/Resources"
                    VERBATIM
            )
        endif ()
    endif ()
endif ()

if (EMSCRIPTEN)
    # Emscripten needs a larger wasm-ld initial memory.
    set(EM_FLAGS
            "\
-s MAXIMUM_MEMORY=1gb \
-s INITIAL_MEMORY=128MB \
-s ALLOW_MEMORY_GROWTH \
-s DEFAULT_LIBRARY_FUNCS_TO_INCLUDE=\\\[\\\"\\\$autoResumeAudioContext\\\"\\\,\\\"\\\$dynCall\\\"\\\] \
-s EXPORTED_RUNTIME_METHODS=ccall,FS,UTF8ToString \
-s EXPORTED_FUNCTIONS=_LoadROMFromTempPath\\\,_SetupCallbacks\\\,_SetVolume\\\,_ResetROM\\\,_JoystickButtonDown\\\,_JoystickButtonUp\\\,_SetABSwapEnabled\\\,_IsABSwapEnabled\\\,_GetFPS\\\,_SyncFilesystem\\\,_SaveState\\\,_LoadState\\\,_DeleteState\\\,_GetSaveStatesCount\\\,_HasSaveState\\\,_GetSaveStateThumbnail\\\,_main \
-s FILESYSTEM=1 \
-lidbfs.js \
-gsource-map \
--shell-file ${CMAKE_CURRENT_SOURCE_DIR}/build/web/kiwi_machine.html"
    )
    set_target_properties(${PROJECT_NAME}
            PROPERTIES
            LINK_FLAGS ${EM_FLAGS}
            LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/build/web/kiwi_machine.html
    )
    set(CMAKE_EXECUTABLE_SUFFIX ".html")

    # Run KiwiMachine WASM (Without ROMS Copy)
    if (KIWI_WASM_RELEASE)
        add_custom_target(
                "BuildKiwiMachineWASM_WithoutROMSCopy"
                COMMAND ${Python3_EXECUTABLE} "build-wasm.py" --release
                WORKING_DIRECTORY ${kiwi_machine_SOURCE_DIR}/../kiwi_machine_wasm
                VERBATIM
                DEPENDS ${PROJECT_NAME}
        )
    else ()
        add_custom_target(
                "BuildKiwiMachineWASM_WithoutROMSCopy"
                COMMAND ${Python3_EXECUTABLE} "build-wasm.py" --debug
                WORKING_DIRECTORY ${kiwi_machine_SOURCE_DIR}/../kiwi_machine_wasm
                VERBATIM
                DEPENDS ${PROJECT_NAME}
        )
    endif ()
    
    # Run KiwiMachine WASM
    if (KIWI_WASM_RELEASE)
        add_custom_target(
                "BuildKiwiMachineWASM"
                COMMAND ${Python3_EXECUTABLE} "build-wasm.py" --release --roms
                WORKING_DIRECTORY ${kiwi_machine_SOURCE_DIR}/../kiwi_machine_wasm
                VERBATIM
                DEPENDS ${PROJECT_NAME}
        )
    else ()
        add_custom_target(
                "BuildKiwiMachineWASM"
                COMMAND ${Python3_EXECUTABLE} "build-wasm.py" --debug --roms
                WORKING_DIRECTORY ${kiwi_machine_SOURCE_DIR}/../kiwi_machine_wasm
                VERBATIM
                DEPENDS ${PROJECT_NAME}
        )
    endif ()
endif ()

target_link_libraries(${PROJECT_NAME}
        PRIVATE
        kiwi_machine_core
)

if(KIWI_SWITCH)
    target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE
            "$ENV{DEVKITPRO}/libnx/include")

    if(NOT DEFINED KIWI_PACKAGE_DIR OR
            NOT EXISTS "${KIWI_PACKAGE_DIR}/main.pak")
        KIWI_SWITCH_ENVIRONMENT_ERROR(
                "KIWI_PACKAGE_DIR must contain main.pak for Switch builds.")
    endif()
    file(GLOB_RECURSE KIWI_SWITCH_PAK_FILES CONFIGURE_DEPENDS
            "${KIWI_PACKAGE_DIR}/*.pak")

    if(NOT NX_NACPTOOL_EXE)
        find_program(NX_NACPTOOL_EXE nacptool
                HINTS "$ENV{DEVKITPRO}/tools/bin")
    endif()
    if(NOT NX_ELF2NRO_EXE)
        find_program(NX_ELF2NRO_EXE elf2nro
                HINTS "$ENV{DEVKITPRO}/tools/bin")
    endif()
    if(NOT NX_NACPTOOL_EXE OR NOT NX_ELF2NRO_EXE)
        KIWI_SWITCH_ENVIRONMENT_ERROR(
                "nacptool and elf2nro are required for Switch builds.")
    endif()

    set(KIWI_SWITCH_DIST_DIR
            "${CMAKE_BINARY_DIR}/dist/switch/KiwiMachine")
    set(KIWI_SWITCH_NACP
            "${CMAKE_CURRENT_BINARY_DIR}/KiwiMachine.nacp")
    set(KIWI_SWITCH_NRO
            "${KIWI_SWITCH_DIST_DIR}/KiwiMachine.nro")
    set(KIWI_SWITCH_ELF
            "${KIWI_SWITCH_DIST_DIR}/KiwiMachine.elf")
    set(KIWI_SWITCH_ICON "$ENV{DEVKITPRO}/libnx/default_icon.jpg"
            CACHE FILEPATH "Icon embedded in the Switch NRO")
    if(NOT EXISTS "${KIWI_SWITCH_ICON}")
        KIWI_SWITCH_ENVIRONMENT_ERROR(
                "Switch NRO icon not found: ${KIWI_SWITCH_ICON}")
    endif()

    add_custom_target(kiwi_machine_nro ALL
            COMMAND ${CMAKE_COMMAND} -E make_directory
                    "${KIWI_SWITCH_DIST_DIR}"
            COMMAND ${CMAKE_COMMAND} -E remove
                    "${KIWI_SWITCH_DIST_DIR}/main.pak"
                    "${KIWI_SWITCH_DIST_DIR}/specials.pak"
            COMMAND ${CMAKE_COMMAND} -E remove_directory
                    "${KIWI_SWITCH_DIST_DIR}/textures"
            COMMAND ${NX_NACPTOOL_EXE} --create
                    "Kiwi Machine" "Yisi Yu" "${KIWI_SWITCH_APP_VERSION}"
                    "${KIWI_SWITCH_NACP}"
            COMMAND ${NX_ELF2NRO_EXE}
                    "$<TARGET_FILE:${PROJECT_NAME}>"
                    "${KIWI_SWITCH_NRO}"
                    "--icon=${KIWI_SWITCH_ICON}"
                    "--nacp=${KIWI_SWITCH_NACP}"
                    "--romfsdir=${KIWI_PACKAGE_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:${PROJECT_NAME}>"
                    "${KIWI_SWITCH_ELF}"
            DEPENDS ${PROJECT_NAME} ${KIWI_SWITCH_PAK_FILES}
            BYPRODUCTS
                    "${KIWI_SWITCH_NACP}"
                    "${KIWI_SWITCH_NRO}"
                    "${KIWI_SWITCH_ELF}"
            VERBATIM
    )
endif()
