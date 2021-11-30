cmake_minimum_required(VERSION 3.10)

project(obscontrol VERSION 0.1.0)

configure_file(obscontrol.h.in obscontrol.h)
add_executable(obscontrol obscontrol.cpp)
target_include_directories(obscontrol PUBLIC "${PROJECT_BINARY_DIR}")
set(CMAKE_CPP_STANDARD 11)
set(CMAKE_CPP_STANDARD_REQUIRED True)
install(TARGETS obscontrol DESTINATION bin)
