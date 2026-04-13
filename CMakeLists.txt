cmake_minimum_required(VERSION 3.10)
project(simplecore C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Enable warnings
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

include_directories(include)

# Collect all source files
set(SOURCES
    src/main.c
    src/cpu.c
    src/alu.c
    src/memory.c
    src/bus.c
    src/io.c
    src/assembler.c
)

add_executable(simplecore ${SOURCES})

# Install target
install(TARGETS simplecore DESTINATION bin)
