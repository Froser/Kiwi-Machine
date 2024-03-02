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

OPTION(KIWI_EMBEDDED_ROMS "Embed roms data in code" ON)

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

if (EMSCRIPTEN)
    # Emscripten needs a larger wasm-ld intial memory.
    set(EM_FLAGS
            "-s PTHREAD_POOL_SIZE=5 \
-s MAXIMUM_MEMORY=1gb \
-s INITIAL_MEMORY=128MB \
-s ALLOW_MEMORY_GROWTH \
-s DEFAULT_LIBRARY_FUNCS_TO_INCLUDE=\\\[\\\"\\\$autoResumeAudioContext\\\"\\\,\\\"\\\$dynCall\\\"\\\] \
-s USE_OFFSET_CONVERTER \
-s EXPORTED_RUNTIME_METHODS=ccall \
-s EXPORTED_FUNCTIONS=_LoadROMFromTempPath\\\,_main \
--shell-file ${CMAKE_CURRENT_SOURCE_DIR}/build/web/kiwi_machine.html"
    )
    set_target_properties(${PROJECT_NAME}
            PROPERTIES
            LINK_FLAGS ${EM_FLAGS}
    )
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
endif ()

target_link_libraries(${PROJECT_NAME}
        PRIVATE
        kiwi_machine_core
)
