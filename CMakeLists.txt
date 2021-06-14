cmake_minimum_required(VERSION 3.0)
project(objectpool)

set(CMAKE_CXX_STANDARD 17)

add_executable(objectpooltestsuite main.cpp lib.cpp objectpooltestsuite.hpp objectpool.hpp)
