cmake_minimum_required(VERSION 2.8)

project(angrylion-rdp-plus)

set(CMAKE_CXX_STANDARD 14)
set(COMPILE_DEFINITIONS -O3 -Wall -Wextra -Winline)

find_package(Git REQUIRED)
find_package(PythonInterp 3.2 REQUIRED)
find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)

# RDP core
set(PATH_CORE "core")
set(PATH_VERSION_IN "${PATH_CORE}/version.h.in")
set(PATH_VERSION_OUT "${PATH_CORE}/version.h")

add_custom_command(
    OUTPUT ${PATH_VERSION_OUT}
    COMMAND
        ${PYTHON_EXECUTABLE}
        "${CMAKE_SOURCE_DIR}/make_version.py"
        "${CMAKE_SOURCE_DIR}/${PATH_VERSION_IN}"
        "${CMAKE_SOURCE_DIR}/${PATH_VERSION_OUT}"
    COMMENT "Generate Git version"
)

file(GLOB SOURCES_CORE "${PATH_CORE}/*.c" "${PATH_CORE}/*.cpp")
add_library(core STATIC ${SOURCES_CORE} ${PATH_VERSION_OUT})

if ( NOT MINGW )
    set_target_properties(core PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif ( NOT MINGW )

# Mupen64Plus GFX plugin
set(PATH_PLUGIN_M64P "plugin-mupen64plus")

include_directories(${CMAKE_SOURCE_DIR})

file(GLOB SOURCES_PLUGIN_M64P "${PATH_PLUGIN_M64P}/*.c")
add_library(mupen64plus-video-angrylionplus SHARED ${SOURCES_PLUGIN_M64P})

set_target_properties(mupen64plus-video-angrylionplus PROPERTIES PREFIX "")

target_link_libraries(mupen64plus-video-angrylionplus core ${OPENGL_LIBRARIES})

# RDP retracer
set(PATH_RETRACER "retrace")

include_directories(${PATH_CORE})

file(GLOB SOURCES_RETRACE "${PATH_RETRACER}/*.c")
add_executable(retrace ${SOURCES_RETRACE})

target_link_libraries(retrace core pthread SDL2)
