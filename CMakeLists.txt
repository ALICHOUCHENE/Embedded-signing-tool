cmake_minimum_required(VERSION 3.0) 						# setting this is required
project(signing_tool VERSION 1.0.0 LANGUAGES C)            	# this sets the project name

# -------------------------------------------------------------------
# Add compile options
# -------------------------------------------------------------------
add_compile_options(-Os -ffunction-sections -fdata-sections)
# Remove -rdynamic: CMake adds it automatically on Linux, but it exports all
# symbols to the dynamic symbol table which prevents --gc-sections from
# eliminating unused functions.
set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")
add_link_options(-Wl,--gc-sections)
add_link_options(-Wl,-Map=output.map)

# -------------------------------------------------------------------
# Source directories
# -------------------------------------------------------------------
include_directories(
    ${PROJECT_SOURCE_DIR}/Libraries/Inc
    ${PROJECT_SOURCE_DIR}/Middleware/Inc
)

# -------------------------------------------------------------------
# Sources
# -------------------------------------------------------------------
set(CORE_SOURCES
    Core/Src/main.c
)

file(GLOB_RECURSE LIBRARIES_SOURCES
    ${PROJECT_SOURCE_DIR}/Libraries/Src/*.c
)

file(GLOB_RECURSE MIDDLEWARE_SOURCES
    ${PROJECT_SOURCE_DIR}/Middleware/Src/*.c
)

set(ALL_SOURCES
    ${CORE_SOURCES}
    ${LIBRARIES_SOURCES}
    ${MIDDLEWARE_SOURCES}
)


# -------------------------------------------------------------------
# Executable
# -------------------------------------------------------------------
add_executable(signing_tool ${ALL_SOURCES})


