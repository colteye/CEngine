file(GLOB_RECURSE CENGINE_FORMAT_FILES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_SOURCE_DIR}/samples/viewer/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/*.h"
    "${CMAKE_SOURCE_DIR}/samples/viewer/*.h"
)

file(GLOB_RECURSE CENGINE_TIDY_FILES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_SOURCE_DIR}/samples/viewer/*.cpp"
)

find_program(CLANG_FORMAT_EXE NAMES
    clang-format
    clang-format-19
    clang-format-18
    clang-format-17
    clang-format-16
    clang-format-15
    clang-format.exe
    HINTS
        "/mnt/c/Program Files/LLVM/bin"
        "/mnt/c/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/Llvm/x64/bin"
        "/mnt/c/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/Llvm/bin"
        "/mnt/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/Llvm/x64/bin"
        "/mnt/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/Llvm/bin"
        "C:/Program Files/LLVM/bin"
        "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/Llvm/x64/bin"
        "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/Llvm/bin"
        "C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/Llvm/x64/bin"
        "C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/Llvm/bin"
)
if(CLANG_FORMAT_EXE)
    add_custom_target(format
        COMMAND "${CLANG_FORMAT_EXE}" -i ${CENGINE_FORMAT_FILES}
        COMMENT "Formatting CEngine sources with clang-format"
        VERBATIM
    )

    add_custom_target(format-check
        COMMAND "${CMAKE_COMMAND}"
                -DCLANG_FORMAT_EXE="${CLANG_FORMAT_EXE}"
                -DSOURCE_DIR="${CMAKE_SOURCE_DIR}"
                -P "${CMAKE_SOURCE_DIR}/cmake/RunClangFormatCheck.cmake"
        COMMENT "Checking CEngine source formatting"
        VERBATIM
    )
else()
    add_custom_target(format
        COMMAND "${CMAKE_COMMAND}" -E echo "clang-format was not found on PATH."
        COMMAND "${CMAKE_COMMAND}" -E false
        VERBATIM
    )
    add_custom_target(format-check
        COMMAND "${CMAKE_COMMAND}" -E echo "clang-format was not found on PATH."
        COMMAND "${CMAKE_COMMAND}" -E false
        VERBATIM
    )
endif()

find_program(CLANG_TIDY_EXE NAMES clang-tidy)
if(CLANG_TIDY_EXE)
    add_custom_target(tidy
        COMMAND "${CLANG_TIDY_EXE}" -p "${CMAKE_BINARY_DIR}" ${CENGINE_TIDY_FILES}
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Running clang-tidy static analysis"
        VERBATIM
    )
else()
    add_custom_target(tidy
        COMMAND "${CMAKE_COMMAND}" -E echo "clang-tidy was not found on PATH."
        COMMAND "${CMAKE_COMMAND}" -E false
        VERBATIM
    )
endif()

find_program(CPPCHECK_EXE NAMES cppcheck)
if(CPPCHECK_EXE)
    add_custom_target(cppcheck
        COMMAND "${CPPCHECK_EXE}"
                --enable=warning,style,performance,portability
                --std=c++17
                --project="${CMAKE_BINARY_DIR}/compile_commands.json"
                --suppress=missingIncludeSystem
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Running cppcheck static analysis"
        VERBATIM
    )
else()
    add_custom_target(cppcheck
        COMMAND "${CMAKE_COMMAND}" -E echo "cppcheck was not found on PATH."
        COMMAND "${CMAKE_COMMAND}" -E false
        VERBATIM
    )
endif()

add_custom_target(lint
    DEPENDS format-check tidy
    COMMENT "Running formatting and clang-tidy checks"
)
