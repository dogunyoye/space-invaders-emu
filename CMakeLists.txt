cmake_minimum_required(VERSION 3.16)
project(SpaceInvadersEmulator CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find SDL2 package (assumes SDL2 is installed on your system or WSL environment)
find_package(SDL2 REQUIRED)
find_package(SDL2_mixer REQUIRED)

set(WARNING_FLAGS -Wall -Wextra -Wno-unused-parameter)

# Emulator core: CPU, arcade I/O, disassembler, AI, save states. No SDL
# dependency, so tests build without SDL headers on the include path.
add_library(emu_core STATIC
    src/Emulator8080.cpp
    src/ArcadeHardware.cpp
    src/Disassembler8080.cpp
    src/AIPlayer.cpp
    src/SaveState.cpp
)
target_include_directories(emu_core PUBLIC include)

# SDL frontend
add_executable(space_invaders
    src/main.cpp
    src/Assets.cpp
    src/Renderer.cpp
    src/Audio.cpp
    src/Input.cpp
)
target_include_directories(space_invaders PRIVATE ${SDL2_INCLUDE_DIRS} ${SDL2_MIXER_INCLUDE_DIRS})
target_link_libraries(space_invaders PRIVATE emu_core)

# Link SDL2 libraries (prefer imported targets, fall back to variables)
if (TARGET SDL2::SDL2)
    target_link_libraries(space_invaders PRIVATE SDL2::SDL2)
else()
    target_link_libraries(space_invaders PRIVATE ${SDL2_LIBRARIES})
endif()

if (TARGET SDL2_mixer::SDL2_mixer)
    target_link_libraries(space_invaders PRIVATE SDL2_mixer::SDL2_mixer)
elseif (TARGET SDL2::Mixer)
    target_link_libraries(space_invaders PRIVATE SDL2::Mixer)
else()
    target_link_libraries(space_invaders PRIVATE ${SDL2_MIXER_LIBRARIES})
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(emu_core PRIVATE ${WARNING_FLAGS})
    target_compile_options(space_invaders PRIVATE ${WARNING_FLAGS})
endif()

option(BUILD_TESTS "Build unit tests" ON)
if (BUILD_TESTS)
    enable_testing()
    set(TEST_SOURCES_cpu_tests tests/test_cpu.cpp)
    set(TEST_SOURCES_hw_tests tests/test_hardware.cpp)
    set(TEST_SOURCES_ai_tests tests/test_ai.cpp)
    foreach(t cpu_tests hw_tests ai_tests)
        add_executable(${t} ${TEST_SOURCES_${t}})
        target_link_libraries(${t} PRIVATE emu_core)
        if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(${t} PRIVATE ${WARNING_FLAGS})
        endif()
        add_test(NAME ${t} COMMAND ${t})
    endforeach()

    # Full instruction-set check against the bundled CP/M diagnostic.
    add_test(NAME cputest_com
             COMMAND space_invaders --test ${CMAKE_CURRENT_SOURCE_DIR}/roms/CPUTEST.COM)
    set_tests_properties(cputest_com PROPERTIES PASS_REGULAR_EXPRESSION "CPU TESTS OK")
endif()
