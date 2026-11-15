# *******************************************************************************
# Copyright (c) 2026 Accenture
#
# This program and the accompanying materials are made available under the
# terms of the Apache License Version 2.0 which is available at
# https://www.apache.org/licenses/LICENSE-2.0
#
# SPDX-License-Identifier: Apache-2.0
# *******************************************************************************

add_library(linknm src/linknm/LinkManager.cpp)

target_include_directories(linknm PUBLIC include)

target_link_libraries(linknm PUBLIC etl commgateway)

