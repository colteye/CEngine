#   _____ ______             _
#  / ____|  ____|           (_)
# | |    | |__   _ __   __ _ _ _ __   ___
# | |    |  __| | '_ \ / _` | | '_ \ / _ \
# | |____| |____| | | | (_| | | | | |  __/
#  \_____|______|_| |_|\__, |_|_| |_|\___|
#                       __/ |
#                      |___/

cmake_minimum_required(VERSION 3.24)

project(CEngine VERSION 0.1.0 LANGUAGES C CXX)

include(CTest)

option(CENGINE_BUILD_VIEWER "Build the interactive CEngine viewer executable." ON)
option(CENGINE_ENABLE_OPENGL "Build the OpenGL 3.3 render backend." ON)
option(CENGINE_ENABLE_VULKAN "Build the Vulkan render backend when the Vulkan SDK is available." OFF)
option(CENGINE_ENABLE_JOLT_PHYSICS "Build the Jolt Physics backend." ON)
option(CENGINE_ENABLE_AUDIO "Build the miniaudio mixer/3D backend on SDL3 audio." ON)
option(CENGINE_USE_SYSTEM_SDL3 "Use an installed SDL3 package instead of fetching pinned source." OFF)
option(CENGINE_BUILD_TESTS "Build lightweight CEngine test executables." ON)

if(CENGINE_ENABLE_OPENGL AND CENGINE_ENABLE_VULKAN)
    message(FATAL_ERROR "Select exactly one render backend per build; OpenGL and Vulkan cannot both be enabled.")
endif()
if(NOT CENGINE_ENABLE_OPENGL AND NOT CENGINE_ENABLE_VULKAN)
    message(FATAL_ERROR "Select exactly one render backend per build; enable OpenGL or Vulkan.")
endif()

set(CMAKE_CXX_STANDARD 20)
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

function(ce_setup_sdl3 out_target)
    if(CENGINE_USE_SYSTEM_SDL3)
        find_package(SDL3 3.4 REQUIRED CONFIG)
        if(TARGET SDL3::SDL3)
            set(${out_target} SDL3::SDL3 PARENT_SCOPE)
        elseif(TARGET SDL3::SDL3-static)
            set(${out_target} SDL3::SDL3-static PARENT_SCOPE)
        else()
            message(FATAL_ERROR "The SDL3 package did not export an SDL3 target.")
        endif()
        return()
    endif()

    include(FetchContent)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
    set(SDL_UNINSTALL OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SDL_AUDIO ${CENGINE_ENABLE_AUDIO} CACHE BOOL "" FORCE)
    set(SDL_VIDEO ON CACHE BOOL "" FORCE)
    set(SDL_GPU OFF CACHE BOOL "" FORCE)
    set(SDL_RENDER OFF CACHE BOOL "" FORCE)
    set(SDL_CAMERA OFF CACHE BOOL "" FORCE)
    set(SDL_JOYSTICK OFF CACHE BOOL "" FORCE)
    set(SDL_HAPTIC OFF CACHE BOOL "" FORCE)
    set(SDL_HIDAPI OFF CACHE BOOL "" FORCE)
    set(SDL_POWER OFF CACHE BOOL "" FORCE)
    set(SDL_SENSOR OFF CACHE BOOL "" FORCE)
    set(SDL_DIALOG OFF CACHE BOOL "" FORCE)
    set(SDL_TRAY OFF CACHE BOOL "" FORCE)
    set(SDL_DISKAUDIO OFF CACHE BOOL "" FORCE)
    set(SDL_OPENGL ${CENGINE_ENABLE_OPENGL} CACHE BOOL "" FORCE)
    set(SDL_OPENGLES OFF CACHE BOOL "" FORCE)
    set(SDL_VULKAN ${CENGINE_ENABLE_VULKAN} CACHE BOOL "" FORCE)
    FetchContent_Declare(
        SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        # SDL 3.4.10, pinned to an immutable revision.
        GIT_TAG 8e37db5e797b6167f3a00d697d816a684bd259c7
    )
    FetchContent_MakeAvailable(SDL3)
    set(${out_target} SDL3::SDL3-static PARENT_SCOPE)
endfunction()

ce_setup_sdl3(CENGINE_SDL3_TARGET)

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

include(FetchContent)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(ozz_build_tools OFF CACHE BOOL "" FORCE)
set(ozz_build_fbx OFF CACHE BOOL "" FORCE)
set(ozz_build_gltf OFF CACHE BOOL "" FORCE)
set(ozz_build_data OFF CACHE BOOL "" FORCE)
set(ozz_build_samples OFF CACHE BOOL "" FORCE)
set(ozz_build_howtos OFF CACHE BOOL "" FORCE)
set(ozz_build_tests OFF CACHE BOOL "" FORCE)
set(ozz_build_postfix OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    ozz_animation
    GIT_REPOSITORY https://github.com/guillaumeblanc/ozz-animation.git
    # ozz-animation 0.16.0, pinned to an immutable revision.
    GIT_TAG 6cbdc790123aa4731d82e255df187b3a8a808256
)
FetchContent_MakeAvailable(ozz_animation)

# RmlUi is an implementation detail of UISystem. Keep the dependency surface
# deliberately small: FreeType supplies font rasterization, while the optional
# sample, scripting, vector-image, and animation integrations stay disabled.
set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
set(SKIP_INSTALL_ALL ON CACHE BOOL "" FORCE)
FetchContent_Declare(
    freetype
    GIT_REPOSITORY https://github.com/freetype/freetype.git
    # FreeType 2.14.3, pinned to an immutable revision.
    GIT_TAG 0a0221a1347e2f1e07c395263540026e9a0aa7c7
)
FetchContent_MakeAvailable(freetype)
if(NOT TARGET Freetype::Freetype)
    add_library(Freetype::Freetype ALIAS freetype-interface)
endif()

set(RMLUI_SAMPLES OFF CACHE BOOL "" FORCE)
set(RMLUI_TESTS OFF CACHE BOOL "" FORCE)
set(RMLUI_LUA_BINDINGS OFF CACHE BOOL "" FORCE)
set(RMLUI_LOTTIE_PLUGIN OFF CACHE BOOL "" FORCE)
set(RMLUI_SVG_PLUGIN OFF CACHE BOOL "" FORCE)
set(RMLUI_FONT_ENGINE freetype CACHE STRING "" FORCE)
set(RMLUI_PRECOMPILED_HEADERS OFF CACHE BOOL "" FORCE)
set(RMLUI_COMPILER_OPTIONS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    RmlUi
    GIT_REPOSITORY https://github.com/mikke89/RmlUi.git
    # RmlUi 6.2, pinned to an immutable revision.
    GIT_TAG 2230d1a6e8e0848ed87a5761e2a5160b2a175ba4
)
FetchContent_MakeAvailable(RmlUi)

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
        src/renderer/opengl/glsl/skinning.glsl
        src/renderer/opengl/glsl/pbr_standard.vert
        src/renderer/opengl/glsl/pbr_standard.frag
        src/renderer/opengl/glsl/ssao.vert
        src/renderer/opengl/glsl/ssao.frag
		src/renderer/opengl/glsl/ssao_composite.frag
    )
endif()

set(CENGINE_ASSET_SOURCES
    "${CENGINE_ENGINE_GENERATED_HEADER}"
    src/assets/animation_asset.cpp
    src/assets/audio_asset.cpp
    src/assets/store.cpp
    src/assets/particle_asset.cpp
    src/assets/reader.cpp
    src/assets/casset_asset.cpp
    src/assets/material_asset.cpp
    src/assets/mesh_asset.cpp
    src/assets/physics_asset.cpp
    src/assets/skeleton_asset.cpp
    src/assets/texture_asset.cpp
)

add_library(CEngineAssets STATIC ${CENGINE_ASSET_SOURCES})
add_dependencies(CEngineAssets CEngineEngineSchemas)

add_library(CEngineScene STATIC
    "${CENGINE_ENGINE_GENERATED_HEADER}"
    src/assets/scene_asset.cpp
    src/entity/audio_entities.cpp
    src/entity/camera_entity.cpp
    src/entity/collider_entity.cpp
    src/entity/fog_entity.cpp
    src/entity/entity.cpp
    src/entity/entity_factory.cpp
    src/entity/light_entity.cpp
    src/entity/logic_entities.cpp
    src/entity/physics_constraint_entity.cpp
    src/entity/post_process_entity.cpp
    src/entity/prop_entity.cpp
    src/entity/skybox_entity.cpp
    src/entity/trigger_entity.cpp
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
        "${CENGINE_GENERATED_ROOT}"
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
    set(CENGINE_PYTHON_CSCENE_FIXTURE "${CMAKE_CURRENT_BINARY_DIR}/python_fixture.cscene")
    add_test(NAME generate_python_asset_fixtures
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/ceasset/tests/generate_cscene_fixture.py"
            --output "${CENGINE_PYTHON_CSCENE_FIXTURE}")
    set_tests_properties(generate_python_asset_fixtures PROPERTIES FIXTURES_SETUP python_assets)
    add_test(NAME asset_runtime
        COMMAND CEngineAssetTests "${CMAKE_CURRENT_BINARY_DIR}")
    set_tests_properties(asset_runtime PROPERTIES FIXTURES_REQUIRED python_assets)
    add_executable(CEngineAnimationSystemTests
        tests/animation/animation_system_tests.cpp
    )
    target_link_libraries(CEngineAnimationSystemTests PRIVATE CEngineCore)
    if(MSVC)
        target_compile_options(CEngineAnimationSystemTests PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(CEngineAnimationSystemTests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME animation_system
        COMMAND CEngineAnimationSystemTests "${CMAKE_CURRENT_BINARY_DIR}")
    set_tests_properties(animation_system PROPERTIES FIXTURES_REQUIRED python_assets)
    add_executable(CEngineStoreTests tests/assets/store_tests.cpp)
    target_link_libraries(CEngineStoreTests PRIVATE CEngineAssets)
    if(MSVC)
        target_compile_options(CEngineStoreTests PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(CEngineStoreTests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME store COMMAND CEngineStoreTests "${CMAKE_CURRENT_BINARY_DIR}")
    set_tests_properties(store PROPERTIES FIXTURES_REQUIRED python_assets)
    add_test(NAME load_python_cscene_fixture
        COMMAND CEngineSceneTests "${CENGINE_PYTHON_CSCENE_FIXTURE}")
    set_tests_properties(load_python_cscene_fixture PROPERTIES FIXTURES_REQUIRED python_assets)
    add_executable(CEngineWindowSystemTests tests/window/window_system_tests.cpp)
    target_link_libraries(CEngineWindowSystemTests PRIVATE CEngineCore)
    if(MSVC)
        target_compile_options(CEngineWindowSystemTests PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(CEngineWindowSystemTests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME window_system COMMAND CEngineWindowSystemTests)
    add_executable(CEngineUiSystemTests tests/ui/ui_system_tests.cpp)
    target_link_libraries(CEngineUiSystemTests PRIVATE CEngineCore)
    if(MSVC)
        target_compile_options(CEngineUiSystemTests PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(CEngineUiSystemTests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME ui_system
        COMMAND CEngineUiSystemTests "${CMAKE_CURRENT_SOURCE_DIR}/tests/ui/data")
    if(CENGINE_ENABLE_OPENGL)
        add_executable(CEngineUiRendererTests
            tests/renderer/ui_renderer_tests.cpp
        )
        target_link_libraries(CEngineUiRendererTests PRIVATE CEngineCore)
        if(MSVC)
            target_compile_options(CEngineUiRendererTests PRIVATE /W4 /permissive- /Zc:__cplusplus)
        else()
            target_compile_options(CEngineUiRendererTests PRIVATE -Wall -Wextra -Wpedantic)
        endif()
        add_test(NAME ui_renderer COMMAND CEngineUiRendererTests)
        set_tests_properties(ui_renderer PROPERTIES SKIP_RETURN_CODE 77)
        add_executable(CEngineShadowSystemTests
            tests/renderer/shadow_system_tests.cpp
        )
        target_include_directories(CEngineShadowSystemTests
            PRIVATE
                "${CMAKE_CURRENT_SOURCE_DIR}/src"
            SYSTEM PRIVATE
                "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glm"
        )
        if(MSVC)
            target_compile_options(CEngineShadowSystemTests PRIVATE /W4 /permissive- /Zc:__cplusplus)
        else()
            target_compile_options(CEngineShadowSystemTests PRIVATE -Wall -Wextra -Wpedantic)
        endif()
        target_link_libraries(CEngineShadowSystemTests PRIVATE CEngineCore)
        add_test(NAME shadow_system COMMAND CEngineShadowSystemTests)
    endif()
    if(CENGINE_ENABLE_AUDIO)
        add_executable(CEngineAudioSystemTests tests/audio/audio_system_tests.cpp)
        target_link_libraries(CEngineAudioSystemTests PRIVATE CEngineCore)
        if(MSVC)
            target_compile_options(CEngineAudioSystemTests PRIVATE /W4 /permissive- /Zc:__cplusplus)
        else()
            target_compile_options(CEngineAudioSystemTests PRIVATE -Wall -Wextra -Wpedantic)
        endif()
        add_test(NAME audio_system COMMAND CEngineAudioSystemTests)
        add_executable(CEngineAudioBackendTests tests/audio/audio_backend_tests.cpp)
        target_link_libraries(CEngineAudioBackendTests PRIVATE CEngineCore)
        if(MSVC)
            target_compile_options(CEngineAudioBackendTests PRIVATE /W4 /permissive- /Zc:__cplusplus)
        else()
            target_compile_options(CEngineAudioBackendTests PRIVATE -Wall -Wextra -Wpedantic)
        endif()
        add_test(NAME audio_backend COMMAND CEngineAudioBackendTests)
        set_tests_properties(audio_backend PROPERTIES ENVIRONMENT "SDL_AUDIODRIVER=dummy")
    endif()
endif()

set(CENGINE_CORE_SOURCES
    src/animation/animation_system.cpp
    src/animation/ozz/ozz_animation_backend.cpp
    src/physics/physics_system.cpp
    src/renderer/camera.cpp
    src/input/sdl/sdl_input_backend.cpp
    src/input/input_system.cpp
    src/renderer/mesh_instance.cpp
    src/renderer/render_system.cpp
    src/ui/rmlui/rmlui_file_interface.cpp
    src/ui/rmlui/rmlui_render_interface.cpp
    src/ui/rmlui/rmlui_runtime.cpp
    src/ui/rmlui/rmlui_system_interface.cpp
    src/ui/ui_system.cpp
    src/window/sdl/sdl_window_backend.cpp
    src/window/window_system.cpp
)

if(CENGINE_ENABLE_AUDIO)
    list(APPEND CENGINE_CORE_SOURCES
        src/audio/audio_system.cpp
        src/audio/miniaudio/audio_backend.cpp
        src/audio/miniaudio/miniaudio_impl.cpp
        third_party/miniaudio/extras/nodes/ma_reverb_node/ma_reverb_node.c
    )
endif()

if(CENGINE_ENABLE_JOLT_PHYSICS)
    list(APPEND CENGINE_CORE_SOURCES
        src/physics/jolt/jolt_physics_backend.cpp
    )
endif()

if(CENGINE_ENABLE_OPENGL)
    list(APPEND CENGINE_CORE_SOURCES
        third_party/glad/src/glad.c
        src/renderer/opengl/render_backend.cpp
        src/renderer/opengl/shadow_system.cpp
        src/renderer/opengl/texture.cpp
        src/renderer/opengl/ui_renderer.cpp
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

target_link_libraries(CEngineCore PUBLIC ${CENGINE_SDL3_TARGET})
target_link_libraries(CEngineCore PUBLIC CEngineAssets)
target_link_libraries(CEngineCore PRIVATE ozz_animation_offline)
target_link_libraries(CEngineCore PRIVATE RmlUi::Core)
target_link_libraries(CEngineCore PRIVATE ${CMAKE_DL_LIBS})
target_link_libraries(CEngineScene PUBLIC CEngineCore)

if(CENGINE_ENABLE_AUDIO)
    target_include_directories(CEngineCore SYSTEM PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniaudio"
    )
    target_compile_definitions(CEngineCore
        PUBLIC CENGINE_ENABLE_AUDIO
        PRIVATE
            MA_NO_DEVICE_IO
            MA_NO_ENCODING
            MA_NO_GENERATION
            MA_NO_RESOURCE_MANAGER
    )
endif()

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

if(CENGINE_ENABLE_OPENGL)
    target_compile_definitions(CEngineCore PUBLIC CENGINE_ENABLE_OPENGL GL_SILENCE_DEPRECATION)
endif()

if(MSVC)
    set(CENGINE_SILENCED_THIRD_PARTY_SOURCES)
    if(CENGINE_ENABLE_OPENGL)
        list(APPEND CENGINE_SILENCED_THIRD_PARTY_SOURCES third_party/glad/src/glad.c)
    endif()
    if(CENGINE_ENABLE_JOLT_PHYSICS)
        set_source_files_properties(src/physics/jolt/jolt_physics_backend.cpp PROPERTIES COMPILE_OPTIONS /wd4003)
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
        set_source_files_properties(src/physics/jolt/jolt_physics_backend.cpp PROPERTIES COMPILE_OPTIONS -Wno-pedantic)
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
