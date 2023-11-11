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

include (FindPackageHandleStandardArgs)

set(Kiwi_INCLUDE_DIR
        ${CMAKE_CURRENT_LIST_DIR}/../../include
        ${CMAKE_CURRENT_LIST_DIR}/../../src/kiwi
)

set(Kiwi_LIBRARY kiwi)
set(Kiwi_STATIC_LIBRARY kiwi_static)

find_package_handle_standard_args(
        Kiwi
        REQUIRED_VARS Kiwi_INCLUDE_DIR Kiwi_LIBRARY Kiwi_STATIC_LIBRARY
)

if (NOT TARGET Kiwi::kiwi)
    add_library(Kiwi::kiwi INTERFACE IMPORTED)
    set_property(TARGET Kiwi::kiwi PROPERTY
            INTERFACE_INCLUDE_DIRECTORIES ${Kiwi_INCLUDE_DIR}
    )
    set_property(TARGET Kiwi::kiwi PROPERTY
            INTERFACE_LINK_LIBRARIES ${Kiwi_LIBRARY} SDL2
    )
endif (NOT TARGET Kiwi::kiwi)

if (NOT TARGET Kiwi::kiwi_static)
    add_library(Kiwi::kiwi_static INTERFACE IMPORTED)
    set_property(TARGET Kiwi::kiwi_static PROPERTY
            INTERFACE_INCLUDE_DIRECTORIES ${Kiwi_INCLUDE_DIR}
    )
    set_property(TARGET Kiwi::kiwi_static PROPERTY
            INTERFACE_LINK_LIBRARIES ${Kiwi_STATIC_LIBRARY} SDL2-static
    )
    set_property(TARGET Kiwi::kiwi_static PROPERTY
            INTERFACE_COMPILE_DEFINITIONS KIWI_STATIC_LIBRARY
    )
endif (NOT TARGET Kiwi::kiwi_static)