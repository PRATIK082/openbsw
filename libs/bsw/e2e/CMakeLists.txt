# *******************************************************************************
# Copyright (c) 2026 Accenture
#
# This program and the accompanying materials are made available under the
# terms of the Apache License Version 2.0 which is available at
# https://www.apache.org/licenses/LICENSE-2.0
#
# SPDX-License-Identifier: Apache-2.0
# *******************************************************************************

add_library(e2e src/e2e/E2e.cpp src/e2e/Crc.cpp)

target_include_directories(e2e PUBLIC include)

target_link_libraries(e2e PUBLIC etl util)

if (BUILD_EXECUTABLE STREQUAL "unitTest")
    add_subdirectory(test)
endif ()
