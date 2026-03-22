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
                MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/build/macos/Info.plist
                RESOURCE "${Resources}"
        )

        add_custom_target(
                "KiwiMachine_MakeDMG"
                ALL
                COMMAND hdiutil create ./KiwiMachine.dmg -volname "Kiwi Machine" -ov -srcfolder ./kiwi_machine.app
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
-s EXPORTED_RUNTIME_METHODS=ccall,FS \
-s EXPORTED_FUNCTIONS=_LoadROMFromTempPath\\\,_SetupCallbacks\\\,_CallMenu\\\,_SetVolume\\\,_JoystickButtonDown\\\,_JoystickButtonUp\\\,_GetFPS\\\,_SyncFilesystem\\\,_main \
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
