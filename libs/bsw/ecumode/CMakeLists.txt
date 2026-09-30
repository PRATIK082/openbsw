# *******************************************************************************
# Copyright (c) 2026 Accenture
#
# This program and the accompanying materials are made available under the
# terms of the Apache License Version 2.0 which is available at
# https://www.apache.org/licenses/LICENSE-2.0
#
# SPDX-License-Identifier: Apache-2.0
# *******************************************************************************

add_library(ecumode src/ecumode/EcuMode.cpp
                    src/ecumode/CommStateManagerPhase5.cpp)

target_include_directories(ecumode PUBLIC include)

target_link_libraries(ecumode PUBLIC commgateway)
