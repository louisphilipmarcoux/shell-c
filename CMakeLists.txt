cmake_minimum_required(VERSION 3.13)

project(shell VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB_RECURSE SOURCE_FILES src/*.c src/*.h)

add_executable(shell ${SOURCE_FILES})

target_compile_options(shell PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wformat=2 -Wno-unused-parameter)
target_link_libraries(shell PRIVATE readline)

option(ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
if(ENABLE_SANITIZERS)
    target_compile_options(shell PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(shell PRIVATE -fsanitize=address,undefined)
endif()

install(TARGETS shell DESTINATION bin)
