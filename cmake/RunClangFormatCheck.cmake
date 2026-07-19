file(GLOB_RECURSE FORMAT_FILES CONFIGURE_DEPENDS
    "${SOURCE_DIR}/CEngine/src/engine/*.cpp"
    "${SOURCE_DIR}/CEngine/src/engine/*.h"
)

set(FAILED_FILES "")

foreach(FORMAT_FILE IN LISTS FORMAT_FILES)
    execute_process(
        COMMAND "${CLANG_FORMAT_EXE}" --dry-run --Werror "${FORMAT_FILE}"
        RESULT_VARIABLE FORMAT_RESULT
        OUTPUT_QUIET
        ERROR_VARIABLE FORMAT_ERROR
    )

    if(NOT FORMAT_RESULT EQUAL 0)
        list(APPEND FAILED_FILES "${FORMAT_FILE}")
        message(STATUS "${FORMAT_ERROR}")
    endif()
endforeach()

if(FAILED_FILES)
    list(JOIN FAILED_FILES "\n  " FAILED_LIST)
    message(FATAL_ERROR "clang-format check failed for:\n  ${FAILED_LIST}")
endif()
