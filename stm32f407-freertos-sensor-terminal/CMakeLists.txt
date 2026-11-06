cmake_minimum_required(VERSION 3.16)
project(stm32f407_freertos_terminal C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(terminal_core
    Core/Src/terminal.c
    Core/Src/modbus_rtu.c
    Core/Src/app_port.c
    Drivers/Src/sht3x.c
    Drivers/Src/w25qxx.c
)
target_include_directories(terminal_core PUBLIC Core/Inc Drivers/Inc)

include(CTest)
if(BUILD_TESTING)
    add_executable(terminal_core_tests tests/test_core.c)
    target_link_libraries(terminal_core_tests PRIVATE terminal_core)
    add_test(NAME terminal_core_tests COMMAND terminal_core_tests)
endif()
