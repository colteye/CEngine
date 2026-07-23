# C++ Style

All first-party C++ under `src`, `samples/viewer`, and `tests` uses `.cpp` and
`.h` file extensions and is checked by the repository's formatting and lint
targets. Generated files, build output, fetched dependencies, and `third_party`
code are outside this policy.

## Formatting

`.clang-format` uses Clang-Format's built-in `Microsoft` style without local
formatting overrides. This provides the requested Allman brace placement,
including opening braces on new lines, and four-space indentation.

Run:

```sh
cmake --build --preset debug --target format
cmake --build --preset debug --target format-check
```

## Static analysis and naming

Clang-Tidy has no Microsoft-style preset equivalent to Clang-Format's
`BasedOnStyle: Microsoft`. The repository therefore enables Clang diagnostics,
the static analyzer, and the complete `bugprone`, `performance`, `portability`,
`modernize`, `readability`, and `cppcoreguidelines` groups. Every enabled
diagnostic is an error.

The naming rules provide the consistency that a formatting preset cannot:

- Namespaces, types, enumerators, functions, and compile-time constants use
  `CamelCase`.
- Variables and data members use `lower_case`.
- Private and protected data members have a trailing underscore.
- Macros use `UPPER_CASE`.

The small exclusion list in `.clang-tidy` covers rules that conflict with the
engine's low-level C/C++ APIs, duplicate other checks, impose a different
function-signature convention, force standard-library algorithm or ranges
rewrites in place of straightforward loops, or report inside dependency
headers. New exceptions should be narrow and justified.

Run:

```sh
cmake --build --preset debug --target tidy
cmake --build --preset debug --target lint
```

`lint` combines `format-check` and `tidy`.
