cmake_minimum_required(VERSION 3.20)
project(RatValue VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(SANITIZE "Enable address/undefined sanitizers" OFF)

find_package(RatMoney REQUIRED)
find_package(GTest    REQUIRED)

include(GNUInstallDirs)

set(LIB_SOURCES capm.cpp wacc.cpp fcff.cpp dcf.cpp ddm.cpp fcfe.cpp growth.cpp beta.cpp
                terminal.cpp apv.cpp relative.cpp kd.cpp capital_structure.cpp
                excess_returns.cpp startup.cpp distress.cpp bank.cpp normalize.cpp)
set(LIB_HEADERS types.h capm.h wacc.h fcff.h dcf.h ddm.h fcfe.h growth.h beta.h
                terminal.h apv.h relative.h kd.h capital_structure.h
                excess_returns.h startup.h distress.h bank.h normalize.h)

add_library(ratvalue_static STATIC ${LIB_SOURCES})
set_target_properties(ratvalue_static PROPERTIES
    OUTPUT_NAME           ratvalue
    POSITION_INDEPENDENT_CODE ON)
target_compile_options(ratvalue_static PRIVATE -Wall -Wextra)
target_include_directories(ratvalue_static PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/ratvalue>)
target_link_libraries(ratvalue_static PUBLIC RatMoney::ratmoney_static)
add_library(RatValue::Static ALIAS ratvalue_static)

if(SANITIZE)
  target_compile_options(ratvalue_static PUBLIC
    -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(ratvalue_static PUBLIC
    -fsanitize=address,undefined)
endif()

add_executable(main_bin main.cpp)
target_link_libraries(main_bin PRIVATE RatValue::Static)

option(BUILD_PYTHON "Build Python bindings via nanobind" OFF)
if(BUILD_PYTHON)
    add_subdirectory(python)
endif()

enable_testing()
add_executable(test_bin test.cpp)
target_link_libraries(test_bin PRIVATE RatValue::Static GTest::gtest_main)
add_test(NAME all_tests COMMAND test_bin)
