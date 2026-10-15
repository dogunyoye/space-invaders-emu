cmake_minimum_required(VERSION 3.16)
project(SpaceInvadersEmulator CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find SDL2 package (assumes SDL2 is installed on your system or WSL environment)
find_package(SDL2 REQUIRED)
find_package(SDL2_mixer REQUIRED)

# Define your source files
set(SOURCES
    src/main.cpp
    src/Emulator8080.cpp
    src/Disassembler8080.cpp
    src/ArcadeHardware.cpp
    src/Renderer.cpp
)

# Create the executable target
add_executable(space_invaders ${SOURCES})

# Include headers directory
target_include_directories(space_invaders PRIVATE include ${SDL2_INCLUDE_DIRS} ${SDL2_MIXER_INCLUDE_DIRS})

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
    target_compile_options(space_invaders PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()