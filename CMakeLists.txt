cmake_minimum_required(VERSION 3.13)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(pico_sdk_import.cmake)
project(test_project)
pico_sdk_init()

add_executable(test main.cpp)
#target_compile_options(test PRIVATE -Wall -Wextra -Werror)

add_subdirectory(py)
add_subdirectory(ext/miniz)

pico_enable_stdio_usb(test 1)
pico_enable_stdio_uart(test 1)
pico_add_extra_outputs(test)
target_link_libraries(test pico_stdlib hardware_spi images miniz)
