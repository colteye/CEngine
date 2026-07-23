set(VIEWER_GENERATED_DIR "${CENGINE_GENERATED_ROOT}/viewer")
set(VIEWER_GENERATED_HEADER
    "${VIEWER_GENERATED_DIR}/viewer_entities.generated.h")
set(VIEWER_SCHEMA_BUNDLE "${VIEWER_GENERATED_DIR}/game.json")

add_custom_command(
    OUTPUT
        "${VIEWER_GENERATED_HEADER}"
        "${VIEWER_SCHEMA_BUNDLE}"
    COMMAND "${Python3_EXECUTABLE}" "${CENGINE_SCHEMA_TOOL}"
        --manifest "${CMAKE_CURRENT_SOURCE_DIR}/game.json"
        --cpp-header "${VIEWER_GENERATED_HEADER}"
        --namespace "Viewer::Generated"
        --bundle "${VIEWER_SCHEMA_BUNDLE}"
    DEPENDS
        "${CENGINE_SCHEMA_TOOL}"
        "${CENGINE_ENGINE_GAME_MANIFEST}"
        "${CMAKE_CURRENT_SOURCE_DIR}/game.json"
    VERBATIM
)
add_custom_target(CEngineViewerSchemas
    DEPENDS "${VIEWER_GENERATED_HEADER}" "${VIEWER_SCHEMA_BUNDLE}")

set(VIEWER_GAME_SOURCES
    "${VIEWER_GENERATED_HEADER}"
    entity/player_entity.cpp
    input/actions.cpp
)

if(CENGINE_BUILD_TESTS)
    add_executable(CEngineSceneRenderTests
        ${VIEWER_GAME_SOURCES}
        "${PROJECT_SOURCE_DIR}/tests/scene/subsystem_entity_tests.cpp"
    )
    add_dependencies(CEngineSceneRenderTests CEngineViewerSchemas)
    target_include_directories(CEngineSceneRenderTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(CEngineSceneRenderTests PRIVATE CEngineScene)
    add_test(NAME subsystem_entities COMMAND CEngineSceneRenderTests)
    if(EXISTS "${PROJECT_SOURCE_DIR}/assets/compiled/sponza/Sponza.cscene")
        add_test(NAME viewer_sponza_scene
            COMMAND CEngineSceneRenderTests --validate-scene
                "${PROJECT_SOURCE_DIR}/assets/compiled/sponza/Sponza.cscene"
                "${PROJECT_SOURCE_DIR}")
    endif()

    add_executable(CEngineViewerGameTests
        ${VIEWER_GAME_SOURCES}
        "${PROJECT_SOURCE_DIR}/tests/viewer/game_tests.cpp"
    )
    add_dependencies(CEngineViewerGameTests CEngineViewerSchemas)
    target_include_directories(CEngineViewerGameTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(CEngineViewerGameTests PRIVATE CEngineScene)
    add_test(NAME viewer_game COMMAND CEngineViewerGameTests)
endif()

if(CENGINE_BUILD_VIEWER)
    add_executable(CEngine ${VIEWER_GAME_SOURCES} main.cpp)
    add_dependencies(CEngine CEngineViewerSchemas)
    target_include_directories(CEngine PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(CEngine PRIVATE CEngineScene)
    if(CENGINE_ENABLE_OPENGL)
        ce_setup_opengl(VIEWER_OPENGL_TARGET)
        target_link_libraries(CEngine PRIVATE ${VIEWER_OPENGL_TARGET})
    endif()

    set_property(DIRECTORY "${PROJECT_SOURCE_DIR}"
        PROPERTY VS_STARTUP_PROJECT CEngine)
    set_target_properties(CEngine PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
        VS_DEBUGGER_WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
    if(MSVC)
        target_compile_options(CEngine PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(CEngine PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    if(CENGINE_ENABLE_OPENGL)
        add_custom_command(TARGET CEngine POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E make_directory
                "$<TARGET_FILE_DIR:CEngine>/shaders/opengl"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                ${CENGINE_OPENGL_SHADER_SOURCES}
                "$<TARGET_FILE_DIR:CEngine>/shaders/opengl"
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        )
    endif()

    if(APPLE)
        set(VIEWER_ADDON_PLATFORM "macos-arm64")
    elseif(WIN32)
        set(VIEWER_ADDON_PLATFORM "windows-x64")
    endif()
    if(VIEWER_ADDON_PLATFORM)
        set(VIEWER_ADDON
            "${CMAKE_BINARY_DIR}/blender_addon/cengine-viewer-exporter.zip")
        file(GLOB_RECURSE VIEWER_ADDON_SOURCES CONFIGURE_DEPENDS
            "${PROJECT_SOURCE_DIR}/tools/blender_addon/cengine_asset_exporter/*.py"
            "${PROJECT_SOURCE_DIR}/tools/blender_addon/cengine_asset_exporter/*.toml"
            "${PROJECT_SOURCE_DIR}/tools/blender_addon/cengine_asset_exporter/*.whl"
            "${PROJECT_SOURCE_DIR}/tools/ceasset/ceassetlib/*.py")
        add_custom_command(
            OUTPUT "${VIEWER_ADDON}"
            COMMAND "${Python3_EXECUTABLE}"
                "${PROJECT_SOURCE_DIR}/tools/blender_addon/package_addon.py"
                --output "${VIEWER_ADDON}"
                --platform "${VIEWER_ADDON_PLATFORM}"
                --game "${VIEWER_SCHEMA_BUNDLE}"
            DEPENDS
                CEngineViewerSchemas
                "${VIEWER_SCHEMA_BUNDLE}"
                "${PROJECT_SOURCE_DIR}/tools/blender_addon/package_addon.py"
                ${VIEWER_ADDON_SOURCES}
            VERBATIM
        )
        add_custom_target(CEngineViewerAddon ALL DEPENDS "${VIEWER_ADDON}")
    endif()
endif()
