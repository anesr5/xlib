# CI and Platform Validation

xlib's CI matrix is defined in `.github/workflows/ci.yml`.

## Platforms

The workflow validates:

- Linux with GCC
- Linux with Clang
- Linux with Clang plus AddressSanitizer and UndefinedBehaviorSanitizer
- macOS with Clang
- Windows with MSVC
- Windows with MinGW/UCRT64

## What CI Runs

Each primary platform job:

- Configures the project with CMake
- Builds the library and examples
- Runs the CTest smoke suite
- Runs an explicit `xlib_event_example` backend smoke test
- Installs xlib into a temporary prefix
- Builds C and C++ consumers with `find_package(xlib CONFIG REQUIRED)`
- Runs the package consumer executables

## Local Presets

Common local configurations are available through `CMakePresets.json`:

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

Sanitizer builds are available on non-Windows GCC/Clang environments:

```bash
cmake --preset sanitizers
cmake --build --preset sanitizers
ctest --preset sanitizers
```

## Sanitizers

`XLIB_ENABLE_SANITIZERS=ON` enables AddressSanitizer and UndefinedBehaviorSanitizer for non-Windows GCC/Clang builds. Windows sanitizer coverage is left for future hardening because support differs substantially between MSVC and MinGW toolchains.
