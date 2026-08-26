# Contributing to RenUI

Thanks for helping improve RenUI. Bug reports, focused feature proposals,
documentation corrections, tests, and code contributions are welcome.

## Before opening a change

- Search the [existing issues](https://github.com/14Mikolaj/RenUI/issues) and
  pull requests first.
- Use the issue forms for reproducible bugs and feature proposals.
- For a security vulnerability, follow the
  [security policy](https://github.com/14Mikolaj/RenUI/security/policy) instead
  of opening a public issue.
- Keep changes focused. Application-specific widgets and policies are better
  maintained as extension targets that link `RenUI::RenUI`.

For a substantial API or behavior change, open an issue before investing in an
implementation so the intended scope and compatibility impact can be discussed.

## Development setup

RenUI requires CMake 3.16 or newer, a C++17 compiler, and SFML 3 Graphics,
Window, and System.

```sh
git clone https://github.com/14Mikolaj/RenUI.git
cd RenUI
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSFML_DIR=/path/to/SFML/lib/cmake/SFML
cmake --build build --config Debug
```

Omit `SFML_DIR` if SFML is already discoverable. Keep build output in an
out-of-source directory such as `build/`.

To fetch the tested SFML release instead, configure with
`-DRENUI_FETCH_SFML=ON`. That path requires CMake 3.24 or newer on Windows and
CMake 3.22 or newer on other platforms.

Run the standalone smoke test after building:

```sh
ctest --test-dir build -C Debug --output-on-failure
```

## Verify package consumption

When a change affects CMake, installation, exported headers, or public linking,
install RenUI and build the included consumer project:

```sh
cmake --install build --config Debug --prefix /path/to/renui-install
cmake -S tests/install-consumer -B build/install-consumer -DCMAKE_PREFIX_PATH=/path/to/renui-install
cmake --build build/install-consumer --config Debug
```

Run the generated `renui_install_consumer` executable as an additional smoke
check. Its optional first argument sets a resource root and enables the late
platform-font-loading checks.

## Coding and API guidance

- Match the existing C++17 and CMake style.
- Keep functions and classes focused, and avoid unrelated formatting changes.
- Preserve the `RenUI` namespace, installed include layout, and
  `RenUI::RenUI` target unless an accepted change requires otherwise.
- Keep optional resources optional. New failure paths should provide useful
  diagnostics and preserve a sensible fallback where possible.
- Add or update documentation and smoke coverage for externally visible
  behavior.

The authoritative version is `RENUI_VERSION` in `CMakeLists.txt`. Maintainers
decide when a contribution requires a version bump. User-visible changes should
also be recorded under `Unreleased` in `CHANGELOG.md`.

## Pull requests

In the pull request description, explain the problem, the chosen approach, and
how the result was verified. Link relevant issues and call out public API,
binary compatibility, dependency, or resource-behavior changes explicitly.

By contributing, you agree that your contribution is provided under the
[MIT License](https://github.com/14Mikolaj/RenUI/blob/main/LICENSE).
