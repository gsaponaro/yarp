# Copyright (C) 2006-2019 Istituto Italiano di Tecnologia (IIT)
# All rights reserved.
#
# This software may be modified and distributed under the terms of the
# BSD-3-Clause license. See the accompanying LICENSE file for details.

cmake_minimum_required(VERSION 3.5)
project(thriftEditor_example)

find_package(YARP COMPONENTS OS idl_tools REQUIRED)
set(CMAKE_CXX_FLAGS "-Weverything -Wno-c++98-compat -Wno-padded -Wno-shadow")

yarp_add_idl(IDL_GEN_FILES settings.thrift)
add_executable(thriftEditor_example main.cpp
                                    ${IDL_GEN_FILES})
target_link_libraries(thriftEditor_example PRIVATE YARP::YARP_OS
                                                   YARP::YARP_init)
