# *******************************************************************************
# Copyright (c) 2026 Accenture
#
# This program and the accompanying materials are made available under the
# terms of the Apache License Version 2.0 which is available at
# https://www.apache.org/licenses/LICENSE-2.0
#
# SPDX-License-Identifier: Apache-2.0
# *******************************************************************************

add_executable(ecumodeTest src/ecumode/EcuModeManagerTest.cpp)

target_include_directories(ecumodeTest PUBLIC include)

target_link_libraries(
    ecumodeTest
    PRIVATE ecumode
            asyncMock
            asyncMockImpl
            gmock_main)

gtest_discover_tests(ecumodeTest PROPERTIES LABELS "ecumodeTest")
