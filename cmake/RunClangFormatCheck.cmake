#   _____ ______             _
#  / ____|  ____|           (_)
# | |    | |__   _ __   __ _ _ _ __   ___
# | |    |  __| | '_ \ / _` | | '_ \ / _ |
# | |____| |____| | | | (_| | | | | |  __/
#  \_____|______|_| |_|\__, |_|_| |_|\___|
#                       __/ |
#                      |___/

file(GLOB_RECURSE FORMAT_FILES
    "${SOURCE_DIR}/src/*.cpp"
    "${SOURCE_DIR}/samples/viewer/*.cpp"
    "${SOURCE_DIR}/tests/*.cpp"
    "${SOURCE_DIR}/src/*.h"
    "${SOURCE_DIR}/samples/viewer/*.h"
    "${SOURCE_DIR}/tests/*.h"
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
