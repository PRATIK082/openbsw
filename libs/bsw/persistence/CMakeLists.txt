# *******************************************************************************
# Copyright (c) 2026 Accenture
#
# This program and the accompanying materials are made available under the
# terms of the Apache License Version 2.0 which is available at
# https://www.apache.org/licenses/LICENSE-2.0
#
# SPDX-License-Identifier: Apache-2.0
# *******************************************************************************

add_library(
    persistence src/persistence/Persistence.cpp src/persistence/FlashWearStore.cpp src/persistence/EepromStore.cpp src/persistence/Crc.cpp)

target_include_directories(persistence PUBLIC include)

target_link_libraries(persistence PUBLIC etl util)

if (BUILD_EXECUTABLE STREQUAL "unitTest")
    add_subdirectory(test)
endif ()
