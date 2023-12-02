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

FIND_PACKAGE(Python3 REQUIRED)

include_directories(
        .
        ../kiwi_machine_core
)

set(Sources
        main.cc
)

if (WIN32)
    set(RESOURCES
            build/win/kiwi_machine.rc
    )
endif ()

if (APPLE)
    set(RESOURCES
            build/macos/kiwi.icns
    )
endif ()

if (WIN32)
    KIWI_ADD_EXECUTABLE(${PROJECT_NAME} WIN32 ${Sources} ${RESOURCES})
elseif (APPLE)
    KIWI_ADD_EXECUTABLE(${PROJECT_NAME} MACOSX_BUNDLE ${Sources} ${RESOURCES})
else ()
    KIWI_ADD_EXECUTABLE(${PROJECT_NAME} ${Sources})
endif ()

if (APPLE)
    if (IOS)
        set_target_properties(${PROJECT_NAME}
                PROPERTIES
                MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/build/ios/Info.plist
                RESOURCE ${RESOURCES}
        )
    else ()
        set_target_properties(${PROJECT_NAME}
                PROPERTIES
                MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/build/macos/Info.plist
                RESOURCE ${RESOURCES}
        )

        add_custom_target(
                "KiwiMachine_MakeDMG"
                ALL
                COMMAND hdiutil create ./KiwiMachine.dmg -volname "Kiwi Machine" -ov -srcfolder ./kiwi_machine.app
                WORKING_DIRECTORY "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>/.."
        )
    endif ()
endif ()

target_link_libraries(${PROJECT_NAME}
        PRIVATE
        kiwi_machine_core
)
