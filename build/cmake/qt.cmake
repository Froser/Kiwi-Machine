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

find_package(Qt6 COMPONENTS Core Gui Widgets)

OPTION(USE_QT6 "Use Qt6 as underlying implementation." OFF)

if (USE_QT6)
    function(KIWI_USE_QT6 TargetName)
        set_target_properties(${TargetName} PROPERTIES AUTOMOC ON)
        target_compile_definitions(${TargetName} PRIVATE USE_QT6)
        target_link_libraries(${TargetName} PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)
        target_include_directories(${TargetName} PRIVATE ${Qt6Core_DIR} ${Qt6Gui_DIR} ${Qt6Widgets_DIR})
    endfunction()
endif ()

if (${Qt6_FOUND})
    function(KIWI_DEPLOY_QT6 TargetName)
        set(QT6_BIN_DIR "${Qt6_DIR}/../../../bin")
        if (WIN32)
            add_custom_command(
                    TARGET ${TargetName}
                    POST_BUILD
                    COMMAND ${QT6_BIN_DIR}/windeployqt.exe
                    ARGS --no-opengl-sw --no-translations $<IF:$<CONFIG:Release>,--release,--debug> $<TARGET_FILE:${TargetName}>
                    VERBATIM
            )
        endif ()
    endfunction()
endif ()