cmake_minimum_required(VERSION 3.22)

project(CEngine VERSION 0.1.0 LANGUAGES C CXX)

option(CENGINE_BUILD_VIEWER "Build the interactive CEngine viewer executable." ON)
option(CENGINE_USE_SYSTEM_GLFW "Use an installed system GLFW package instead of fetching GLFW source." OFF)
option(CENGINE_FETCH_GLFW_X11 "Build the X11 backend when GLFW is fetched on Linux." ON)
option(CENGINE_FETCH_GLFW_WAYLAND "Build the Wayland backend when GLFW is fetched on Linux." OFF)
option(CENGINE_ENABLE_VULKAN "Build the Vulkan render backend when the Vulkan SDK is available." ON)

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

ce_setup_glfw(CEngine_GLFW_TARGET)

set(CENGINE_OPENGL_SHADER_SOURCES
    src/opengl/shaders/pbr_standard.vert
    src/opengl/shaders/pbr_standard.frag
    src/opengl/shaders/ssao.vert
    src/opengl/shaders/ssao.frag
)

set(CENGINE_CORE_SOURCES
    third_party/glad/src/glad.c
    third_party/nv_dds/src/nv_dds.cpp
    src/camera.cpp
    src/controls.cpp
    src/light.cpp
    src/material.cpp
    src/mesh.cpp
    src/model.cpp
    src/model_importer.cpp
    src/opengl/opengl_render_backend.cpp
    src/opengl/opengl_texture.cpp
    src/opengl/shaders/pbr_standard.cpp
    src/opengl/shaders/shader.cpp
    src/opengl/shaders/ssao.cpp
    src/render_system.cpp
    src/shader_constants.cpp
)

if(CENGINE_ENABLE_VULKAN)
    find_package(Vulkan QUIET)

    if(Vulkan_FOUND)
        list(APPEND CENGINE_CORE_SOURCES
            src/vulkan/vulkan_render_backend.cpp
        )
    else()
        message(WARNING "CENGINE_ENABLE_VULKAN is ON, but Vulkan was not found. Building without the Vulkan backend.")
    endif()
endif()

add_library(CEngineCore STATIC ${CENGINE_CORE_SOURCES})

target_include_directories(CEngineCore
    SYSTEM PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glad/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glm"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/nlohmann/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/nv_dds/include"
)

target_include_directories(CEngineCore
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(CEngineCore PUBLIC ${CEngine_GLFW_TARGET})
target_link_libraries(CEngineCore PRIVATE ${CMAKE_DL_LIBS})

if(CENGINE_ENABLE_VULKAN AND Vulkan_FOUND)
    target_link_libraries(CEngineCore PUBLIC Vulkan::Vulkan)
    target_compile_definitions(CEngineCore PUBLIC CENGINE_ENABLE_VULKAN)
endif()

target_compile_definitions(CEngineCore
    PUBLIC
        GL_SILENCE_DEPRECATION
        GLFW_INCLUDE_NONE
)

if(MSVC)
    set_source_files_properties(
        third_party/glad/src/glad.c
        third_party/nv_dds/src/nv_dds.cpp
        PROPERTIES COMPILE_OPTIONS /W0
    )
else()
    set_source_files_properties(
        third_party/glad/src/glad.c
        third_party/nv_dds/src/nv_dds.cpp
        PROPERTIES COMPILE_OPTIONS -w
    )
endif()

if(MSVC)
    target_compile_options(CEngineCore PRIVATE /W4 /permissive- /Zc:__cplusplus)
else()
    target_compile_options(CEngineCore PRIVATE -Wall -Wextra -Wpedantic)
endif()


if(CENGINE_BUILD_VIEWER)
    ce_setup_opengl(CEngine_OPENGL_TARGET)
    add_executable(CEngine
        samples/viewer/main.cpp
    )

    target_link_libraries(CEngine PRIVATE CEngineCore ${CEngine_OPENGL_TARGET})

    set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT CEngine)
    set_target_properties(CEngine PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:CEngine>"
    )

    if(MSVC)
        target_compile_options(CEngine PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(CEngine PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_custom_command(TARGET CEngine POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/assets"
                "$<TARGET_FILE_DIR:CEngine>/assets"
    )

    add_custom_command(TARGET CEngine POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory
                "$<TARGET_FILE_DIR:CEngine>/shaders/opengl"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                ${CENGINE_OPENGL_SHADER_SOURCES}
                "$<TARGET_FILE_DIR:CEngine>/shaders/opengl"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )
endif()

include(Tooling)
