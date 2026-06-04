cmake_minimum_required(VERSION 3.20)

find_package(Python 3.9 COMPONENTS Interpreter Development.Module REQUIRED)
find_package(nanobind CONFIG REQUIRED)

nanobind_add_module(ratvalue NB_STATIC
    module.cpp
    bind_capm.cpp
    bind_beta.cpp
    bind_kd.cpp
    bind_wacc.cpp
    bind_normalize.cpp
    bind_fcff.cpp
    bind_fcfe.cpp
    bind_growth.cpp
    bind_dcf.cpp
    bind_ddm.cpp
    bind_terminal.cpp
    bind_apv.cpp
    bind_excess_returns.cpp
    bind_relative.cpp
    bind_capital_structure.cpp
    bind_startup.cpp
    bind_distress.cpp
    bind_bank.cpp
)

target_link_libraries(ratvalue PRIVATE RatValue::Static)

target_compile_options(ratvalue PRIVATE -Wall -Wextra)

install(TARGETS ratvalue LIBRARY DESTINATION .)
