cmake_minimum_required(VERSION 3.22)

project(CEngine VERSION 0.1.0 LANGUAGES C CXX)

option(CENGINE_BUILD_VIEWER "Build the interactive CEngine viewer executable." ON)
option(CENGINE_USE_SYSTEM_GLFW "Use an installed system GLFW package instead of fetching GLFW source." OFF)
option(CENGINE_FETCH_GLFW_X11 "Build the X11 backend when GLFW is fetched on Linux." ON)
option(CENGINE_FETCH_GLFW_WAYLAND "Build the Wayland backend when GLFW is fetched on Linux." OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

function(ce_setup_opengl out_target)
    add_library(CEngineOpenGL INTERFACE)

    if(WIN32)
        target_link_libraries(CEngineOpenGL INTERFACE opengl32)
    elseif(APPLE)
        find_library(OPENGL_FRAMEWORK OpenGL REQUIRED)
        target_link_libraries(CEngineOpenGL INTERFACE "${OPENGL_FRAMEWORK}")
    else()
        find_library(OPENGL_LIBRARY NAMES GL OpenGL)
        if(NOT OPENGL_LIBRARY)
            find_file(OPENGL_LIBRARY_FILE
                NAMES libGL.so libGL.so.1 libOpenGL.so libOpenGL.so.0
                PATHS
                    /usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}
                    /usr/lib
                    /usr/local/lib
                    /lib/${CMAKE_LIBRARY_ARCHITECTURE}
                    /lib
            )
            set(OPENGL_LIBRARY "${OPENGL_LIBRARY_FILE}")
        endif()

        if(NOT OPENGL_LIBRARY)
            message(FATAL_ERROR "OpenGL runtime library was not found.")
        endif()

        target_link_libraries(CEngineOpenGL INTERFACE "${OPENGL_LIBRARY}")
    endif()

    set(${out_target} CEngineOpenGL PARENT_SCOPE)
endfunction()

add_library(CEngineCore STATIC
    CEngine/include/glad/glad.c
    CEngine/include/nv_dds/nv_dds.cpp
    CEngine/src/engine/camera.cpp
    CEngine/src/engine/controls.cpp
    CEngine/src/engine/light.cpp
    CEngine/src/engine/material.cpp
    CEngine/src/engine/mesh.cpp
    CEngine/src/engine/model.cpp
    CEngine/src/engine/model_importer.cpp
    CEngine/src/engine/render_system.cpp
    CEngine/src/engine/shaders/pbr_standard.cpp
    CEngine/src/engine/shaders/shader.cpp
    CEngine/src/engine/shaders/shader_constants.cpp
    CEngine/src/engine/shaders/ssao.cpp
    CEngine/src/engine/texture.cpp
)

target_include_directories(CEngineCore
    SYSTEM PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/CEngine/include"
)

target_include_directories(CEngineCore
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/CEngine/src/engine"
)

target_link_libraries(CEngineCore PRIVATE ${CMAKE_DL_LIBS})

target_compile_definitions(CEngineCore
    PUBLIC
        GL_SILENCE_DEPRECATION
        GLFW_INCLUDE_NONE
)

if(MSVC)
    set_source_files_properties(
        CEngine/include/glad/glad.c
        CEngine/include/nv_dds/nv_dds.cpp
        PROPERTIES COMPILE_OPTIONS /W0
    )
else()
    set_source_files_properties(
        CEngine/include/glad/glad.c
        CEngine/include/nv_dds/nv_dds.cpp
        PROPERTIES COMPILE_OPTIONS -w
    )
endif()

if(MSVC)
    target_compile_options(CEngineCore PRIVATE /W4 /permissive- /Zc:__cplusplus)
else()
    target_compile_options(CEngineCore PRIVATE -Wall -Wextra -Wpedantic)
endif()

function(ce_setup_glfw out_target)
    if(CENGINE_USE_SYSTEM_GLFW)
        find_package(glfw3 QUIET CONFIG)

        if(TARGET glfw)
            set(${out_target} glfw PARENT_SCOPE)
            return()
        endif()

        if(TARGET glfw3)
            set(${out_target} glfw3 PARENT_SCOPE)
            return()
        endif()

        find_path(GLFW_INCLUDE_DIR GLFW/glfw3.h)
        find_library(GLFW_LIBRARY NAMES glfw glfw3)

        if(GLFW_INCLUDE_DIR AND GLFW_LIBRARY)
            add_library(glfw_system UNKNOWN IMPORTED)
            set_target_properties(glfw_system PROPERTIES
                IMPORTED_LOCATION "${GLFW_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${GLFW_INCLUDE_DIR}"
            )
            set(${out_target} glfw_system PARENT_SCOPE)
            return()
        endif()

        message(FATAL_ERROR "System GLFW was requested but was not found.")
    endif()

    include(FetchContent)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    if(UNIX AND NOT APPLE)
        set(GLFW_BUILD_WAYLAND ${CENGINE_FETCH_GLFW_WAYLAND} CACHE BOOL "" FORCE)
        set(GLFW_BUILD_X11 ${CENGINE_FETCH_GLFW_X11} CACHE BOOL "" FORCE)
    endif()
    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4
    )
    FetchContent_MakeAvailable(glfw)
    set(${out_target} glfw PARENT_SCOPE)
endfunction()

if(CENGINE_BUILD_VIEWER)
    ce_setup_opengl(CEngine_OPENGL_TARGET)
    ce_setup_glfw(CEngine_GLFW_TARGET)

    add_executable(CEngine
        CEngine/src/engine/main.cpp
    )

    target_link_libraries(CEngine PRIVATE CEngineCore ${CEngine_OPENGL_TARGET} ${CEngine_GLFW_TARGET})

    set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT CEngine)
    set_target_properties(CEngine PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:CEngine>"
    )

    if(MSVC)
        target_compile_options(CEngine PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(CEngine PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    foreach(asset_dir IN ITEMS missing test_model)
        add_custom_command(TARGET CEngine POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_directory
                    "${CMAKE_CURRENT_SOURCE_DIR}/CEngine/${asset_dir}"
                    "$<TARGET_FILE_DIR:CEngine>/${asset_dir}"
        )
    endforeach()

    foreach(asset_file IN ITEMS
        PBR_Standard_Vertex.vert
        PBR_Standard_Fragment.frag
        SSAO_Vertex.vert
        SSAO_Fragment.frag
        random.DDS
    )
        add_custom_command(TARGET CEngine POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "${CMAKE_CURRENT_SOURCE_DIR}/CEngine/${asset_file}"
                    "$<TARGET_FILE_DIR:CEngine>/${asset_file}"
        )
    endforeach()
endif()

include(Tooling)
