cmake_minimum_required(VERSION 3.22)

project(CEngine VERSION 0.1.0 LANGUAGES C CXX)

include(CTest)

option(CENGINE_BUILD_VIEWER "Build the interactive CEngine viewer executable." ON)
option(CENGINE_ENABLE_OPENGL "Build the OpenGL 3.3 render backend." ON)
option(CENGINE_USE_SYSTEM_GLFW "Use an installed system GLFW package instead of fetching GLFW source." OFF)
option(CENGINE_FETCH_GLFW_X11 "Build the X11 backend when GLFW is fetched on Linux." ON)
option(CENGINE_FETCH_GLFW_WAYLAND "Build the Wayland backend when GLFW is fetched on Linux." OFF)
option(CENGINE_ENABLE_VULKAN "Build the Vulkan render backend when the Vulkan SDK is available." OFF)
option(CENGINE_ENABLE_JOLT_PHYSICS "Build the Jolt Physics backend." ON)
option(CENGINE_BUILD_TESTS "Build lightweight CEngine test executables." ON)

if(CENGINE_ENABLE_OPENGL AND CENGINE_ENABLE_VULKAN)
    message(FATAL_ERROR "Select exactly one render backend per build; OpenGL and Vulkan cannot both be enabled.")
endif()
if(NOT CENGINE_ENABLE_OPENGL AND NOT CENGINE_ENABLE_VULKAN)
    message(FATAL_ERROR "Select exactly one render backend per build; enable OpenGL or Vulkan.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(CENGINE_GENERATED_ROOT "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(CENGINE_SCHEMA_TOOL "${CMAKE_CURRENT_SOURCE_DIR}/tools/schema/generate.py")
set(CENGINE_ENGINE_GAME_MANIFEST
    "${CMAKE_CURRENT_SOURCE_DIR}/schemas/engine.game.json")
set(CENGINE_ENGINE_GENERATED_DIR "${CENGINE_GENERATED_ROOT}/engine")
set(CENGINE_ENGINE_GENERATED_HEADER
    "${CENGINE_ENGINE_GENERATED_DIR}/engine_entities.generated.h")
set(CENGINE_ENGINE_SCHEMA_BUNDLE
    "${CENGINE_ENGINE_GENERATED_DIR}/game.json")

add_custom_command(
    OUTPUT
        "${CENGINE_ENGINE_GENERATED_HEADER}"
        "${CENGINE_ENGINE_SCHEMA_BUNDLE}"
    COMMAND "${Python3_EXECUTABLE}" "${CENGINE_SCHEMA_TOOL}"
        --manifest "${CENGINE_ENGINE_GAME_MANIFEST}"
        --cpp-header "${CENGINE_ENGINE_GENERATED_HEADER}"
        --namespace "CEngine::Generated::EngineEntities"
        --bundle "${CENGINE_ENGINE_SCHEMA_BUNDLE}"
    DEPENDS
        "${CENGINE_SCHEMA_TOOL}"
        "${CENGINE_ENGINE_GAME_MANIFEST}"
    VERBATIM
)
add_custom_target(CEngineEngineSchemas
    DEPENDS
        "${CENGINE_ENGINE_GENERATED_HEADER}"
        "${CENGINE_ENGINE_SCHEMA_BUNDLE}"
)

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
        # GLFW 3.4, pinned to an immutable revision.
        GIT_TAG 7b6aead9fb88b3623e3b3725ebb42670cbe4c579
    )
    FetchContent_MakeAvailable(glfw)
    set(${out_target} glfw PARENT_SCOPE)
endfunction()

ce_setup_glfw(CEngine_GLFW_TARGET)

if(CENGINE_ENABLE_JOLT_PHYSICS)
    include(FetchContent)
    # CEngine only consumes Jolt's CPU physics library.  Jolt enables its
    # optional GPU compute backends by default; on macOS that can make an
    # otherwise OpenGL-only build require Vulkan's glslc shader compiler.
    set(JPH_USE_DX12 OFF CACHE BOOL "" FORCE)
    set(JPH_USE_VK OFF CACHE BOOL "" FORCE)
    set(JPH_USE_MTL OFF CACHE BOOL "" FORCE)
    set(JPH_USE_CPU_COMPUTE ON CACHE BOOL "" FORCE)
    set(DEBUG_RENDERER_IN_DEBUG_AND_RELEASE OFF CACHE BOOL "" FORCE)
    set(ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    set(DOUBLE_PRECISION OFF CACHE BOOL "" FORCE)
    set(GENERATE_DEBUG_SYMBOLS ON CACHE BOOL "" FORCE)
    set(OVERRIDE_CXX_FLAGS OFF CACHE BOOL "" FORCE)
    set(CROSS_PLATFORM_DETERMINISTIC OFF CACHE BOOL "" FORCE)
    set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "" FORCE)
    set(FLOATING_POINT_EXCEPTIONS_ENABLED OFF CACHE BOOL "" FORCE)
    set(CPP_EXCEPTIONS_ENABLED OFF CACHE BOOL "" FORCE)
    set(CPP_RTTI_ENABLED OFF CACHE BOOL "" FORCE)
    set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF CACHE BOOL "" FORCE)
    set(OBJECT_LAYER_BITS 16 CACHE STRING "" FORCE)
    FetchContent_Declare(
        JoltPhysics
        GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
        # Jolt 5.6.0, pinned to an immutable revision.
        GIT_TAG e77f175595e64cb44218cc9d9d56fc365ad0e36a
        SOURCE_SUBDIR Build
    )
    FetchContent_MakeAvailable(JoltPhysics)
endif()

if(CENGINE_ENABLE_OPENGL)
    set(CENGINE_OPENGL_SHADER_SOURCES
        src/renderer/opengl/glsl/deferred_lighting.frag
        src/renderer/opengl/glsl/environment_cube.vert
        src/renderer/opengl/glsl/equirectangular_to_cube.frag
        src/renderer/opengl/glsl/irradiance_convolution.frag
        src/renderer/opengl/glsl/prefilter_environment.frag
        src/renderer/opengl/glsl/skybox.vert
        src/renderer/opengl/glsl/skybox.frag
        src/renderer/opengl/glsl/depth_only.vert
        src/renderer/opengl/glsl/depth_only.frag
        src/renderer/opengl/glsl/fullscreen_blit.frag
        src/renderer/opengl/glsl/pbr_lighting.glsl
        src/renderer/opengl/glsl/pbr_geometry_pass.vert
        src/renderer/opengl/glsl/pbr_geometry_pass.frag
        src/renderer/opengl/glsl/point_shadow_depth.vert
        src/renderer/opengl/glsl/point_shadow_depth.frag
        src/renderer/opengl/glsl/shadow_sampling.glsl
        src/renderer/opengl/glsl/pbr_standard.vert
        src/renderer/opengl/glsl/pbr_standard.frag
        src/renderer/opengl/glsl/ssao.vert
        src/renderer/opengl/glsl/ssao.frag
		src/renderer/opengl/glsl/ssao_composite.frag
    )
endif()

set(CENGINE_ASSET_SOURCES
    src/assets/asset_store.cpp
    src/assets/asset_store_load.cpp
    src/assets/asset_io.cpp
    src/assets/casset_loader.cpp
    src/assets/cscene_reader.cpp
    src/assets/material_loader.cpp
    src/assets/mesh_loader.cpp
    src/assets/physics_loader.cpp
    src/assets/skeleton_loader.cpp
    src/assets/texture_loader.cpp
)

add_library(CEngineAssets STATIC ${CENGINE_ASSET_SOURCES})

add_library(CEngineScene STATIC
    "${CENGINE_ENGINE_GENERATED_HEADER}"
    src/assets/scene_loader.cpp
    src/entity/fog_entity.cpp
    src/entity/entity.cpp
    src/entity/entity_factory.cpp
    src/entity/light_entity.cpp
    src/entity/physics_constraint_entity.cpp
    src/entity/post_process_entity.cpp
    src/entity/prop_entity.cpp
    src/entity/skybox_entity.cpp
    src/scene/scene.cpp
)
add_dependencies(CEngineScene CEngineEngineSchemas)
target_link_libraries(CEngineScene PUBLIC CEngineAssets)
target_include_directories(CEngineScene PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CENGINE_GENERATED_ROOT}"
)
target_include_directories(CEngineScene SYSTEM PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glm")
if(MSVC)
    target_compile_options(CEngineScene PRIVATE /W4 /permissive- /Zc:__cplusplus)
else()
    target_compile_options(CEngineScene PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_include_directories(CEngineAssets
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_include_directories(CEngineAssets
    SYSTEM PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glm"
)

if(MSVC)
    target_compile_options(CEngineAssets PRIVATE /W4 /permissive- /Zc:__cplusplus)
else()
    target_compile_options(CEngineAssets PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CENGINE_BUILD_TESTS)
    add_executable(CEngineSceneTests tests/scene/entity_tests.cpp)
    target_link_libraries(CEngineSceneTests PRIVATE CEngineScene)
    if(MSVC)
        target_compile_options(CEngineSceneTests PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(CEngineSceneTests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME scene_entities COMMAND CEngineSceneTests)
    add_executable(CEngineAssetTests
        src/renderer/mesh_instance.cpp
        tests/assets/asset_runtime_tests.cpp
    )
    target_link_libraries(CEngineAssetTests PRIVATE CEngineAssets)
    target_include_directories(CEngineAssetTests
        SYSTEM PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glm"
    )
    if(MSVC)
        target_compile_options(CEngineAssetTests PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(CEngineAssetTests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME asset_runtime COMMAND CEngineAssetTests)
    add_executable(CEngineAssetStoreTests tests/assets/asset_store_tests.cpp)
    target_link_libraries(CEngineAssetStoreTests PRIVATE CEngineAssets)
    if(MSVC)
        target_compile_options(CEngineAssetStoreTests PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(CEngineAssetStoreTests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME asset_store COMMAND CEngineAssetStoreTests)
    add_executable(CEngineCSceneReaderTests tests/assets/cscene_reader_tests.cpp)
    target_link_libraries(CEngineCSceneReaderTests PRIVATE CEngineAssets)
    if(MSVC)
        target_compile_options(CEngineCSceneReaderTests PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(CEngineCSceneReaderTests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME cscene_reader COMMAND CEngineCSceneReaderTests)
    set(CENGINE_PYTHON_CSCENE_FIXTURE "${CMAKE_CURRENT_BINARY_DIR}/python_fixture.cscene")
    add_test(NAME generate_python_cscene_fixture
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/ceasset/tests/generate_cscene_fixture.py"
            --output "${CENGINE_PYTHON_CSCENE_FIXTURE}")
    set_tests_properties(generate_python_cscene_fixture PROPERTIES FIXTURES_SETUP python_cscene)
    add_test(NAME read_python_cscene_fixture
        COMMAND CEngineCSceneReaderTests "${CENGINE_PYTHON_CSCENE_FIXTURE}")
    set_tests_properties(read_python_cscene_fixture PROPERTIES FIXTURES_REQUIRED python_cscene)
    add_test(NAME load_python_cscene_fixture
        COMMAND CEngineSceneTests "${CENGINE_PYTHON_CSCENE_FIXTURE}")
    set_tests_properties(load_python_cscene_fixture PROPERTIES FIXTURES_REQUIRED python_cscene)
    if(CENGINE_ENABLE_OPENGL)
        add_executable(CEngineDirectionalShadowTests
            src/renderer/opengl/directional_shadow_cascade.cpp
            tests/renderer/directional_shadow_cascade_tests.cpp
        )
        target_include_directories(CEngineDirectionalShadowTests
            PRIVATE
                "${CMAKE_CURRENT_SOURCE_DIR}/src"
            SYSTEM PRIVATE
                "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glm"
        )
        if(MSVC)
            target_compile_options(CEngineDirectionalShadowTests PRIVATE /W4 /permissive- /Zc:__cplusplus)
        else()
            target_compile_options(CEngineDirectionalShadowTests PRIVATE -Wall -Wextra -Wpedantic)
        endif()
        add_test(NAME directional_shadow_cascades COMMAND CEngineDirectionalShadowTests)
    endif()
endif()

set(CENGINE_CORE_SOURCES
    src/renderer/camera.cpp
    src/input/glfw/glfw_input_backend.cpp
    src/input/input_system.cpp
    src/renderer/mesh_instance.cpp
    src/renderer/render_system.cpp
)

if(CENGINE_ENABLE_JOLT_PHYSICS)
    list(APPEND CENGINE_CORE_SOURCES
        src/physics/jolt/jolt_physics_system.cpp
    )
endif()

if(CENGINE_ENABLE_OPENGL)
    list(APPEND CENGINE_CORE_SOURCES
        third_party/glad/src/glad.c
        src/renderer/opengl/directional_shadow_cascade.cpp
        src/renderer/opengl/opengl_render_backend.cpp
        src/renderer/opengl/opengl_shadow_system.cpp
        src/renderer/opengl/opengl_texture.cpp
        src/renderer/opengl/shaders/deferred_lighting.cpp
        src/renderer/opengl/shaders/depth_only.cpp
        src/renderer/opengl/shaders/fullscreen_blit.cpp
        src/renderer/opengl/shaders/lighting_bindings.cpp
        src/renderer/opengl/shaders/pbr_geometry_pass.cpp
        src/renderer/opengl/shaders/point_shadow_depth.cpp
        src/renderer/opengl/shaders/pbr_standard.cpp
        src/renderer/opengl/shaders/shader.cpp
        src/renderer/opengl/shaders/ssao.cpp
    )
endif()

if(CENGINE_ENABLE_VULKAN)
    find_package(Vulkan REQUIRED)
    list(APPEND CENGINE_CORE_SOURCES
        src/renderer/vulkan/vulkan_render_backend.cpp
    )
endif()

add_library(CEngineCore STATIC ${CENGINE_CORE_SOURCES})

target_include_directories(CEngineCore
    SYSTEM PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glm"
)

if(CENGINE_ENABLE_OPENGL)
    target_include_directories(CEngineCore
        SYSTEM PUBLIC
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glad/include"
    )
endif()

target_include_directories(CEngineCore
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(CEngineCore PUBLIC ${CEngine_GLFW_TARGET})
target_link_libraries(CEngineCore PUBLIC CEngineAssets)
target_link_libraries(CEngineCore PRIVATE ${CMAKE_DL_LIBS})
target_link_libraries(CEngineScene PUBLIC CEngineCore)

if(CENGINE_ENABLE_JOLT_PHYSICS)
    target_link_libraries(CEngineCore PUBLIC Jolt)
    target_compile_definitions(CEngineCore PUBLIC CENGINE_ENABLE_JOLT_PHYSICS)
    if(DEFINED JoltPhysics_SOURCE_DIR)
        target_include_directories(CEngineCore SYSTEM PUBLIC "${JoltPhysics_SOURCE_DIR}")
    endif()
endif()

if(CENGINE_ENABLE_VULKAN)
    target_link_libraries(CEngineCore PUBLIC Vulkan::Vulkan)
    target_compile_definitions(CEngineCore PUBLIC CENGINE_ENABLE_VULKAN)
endif()

target_compile_definitions(CEngineCore
    PUBLIC
        GLFW_INCLUDE_NONE
)

if(CENGINE_ENABLE_OPENGL)
    target_compile_definitions(CEngineCore PUBLIC CENGINE_ENABLE_OPENGL GL_SILENCE_DEPRECATION)
endif()

if(MSVC)
    set(CENGINE_SILENCED_THIRD_PARTY_SOURCES)
    if(CENGINE_ENABLE_OPENGL)
        list(APPEND CENGINE_SILENCED_THIRD_PARTY_SOURCES third_party/glad/src/glad.c)
    endif()
    if(CENGINE_ENABLE_JOLT_PHYSICS)
        set_source_files_properties(src/physics/jolt/jolt_physics_system.cpp PROPERTIES COMPILE_OPTIONS /wd4003)
    endif()
    if(CENGINE_SILENCED_THIRD_PARTY_SOURCES)
        set_source_files_properties(${CENGINE_SILENCED_THIRD_PARTY_SOURCES} PROPERTIES COMPILE_OPTIONS /W0)
    endif()
else()
    set(CENGINE_SILENCED_THIRD_PARTY_SOURCES)
    if(CENGINE_ENABLE_OPENGL)
        list(APPEND CENGINE_SILENCED_THIRD_PARTY_SOURCES third_party/glad/src/glad.c)
    endif()
    if(CENGINE_ENABLE_JOLT_PHYSICS)
        set_source_files_properties(src/physics/jolt/jolt_physics_system.cpp PROPERTIES COMPILE_OPTIONS -Wno-pedantic)
    endif()
    if(CENGINE_SILENCED_THIRD_PARTY_SOURCES)
        set_source_files_properties(${CENGINE_SILENCED_THIRD_PARTY_SOURCES} PROPERTIES COMPILE_OPTIONS -w)
    endif()
endif()

if(MSVC)
    target_compile_options(CEngineCore PRIVATE /W4 /permissive- /Zc:__cplusplus)
else()
    target_compile_options(CEngineCore PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CENGINE_BUILD_VIEWER OR CENGINE_BUILD_TESTS)
    add_subdirectory(samples/viewer)
endif()

include(Tooling)
