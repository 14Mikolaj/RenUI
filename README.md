<img width="1309" height="684" alt="image" src="https://github.com/user-attachments/assets/2271249f-31a1-4d4f-bece-a4dd469d52d2" /># RenUI

RenUI is a reusable C++17 user-interface library for SFML 3. It provides
screen-space widgets, responsive layout helpers, theming, text and UI scaling,
resource diagnostics, and graceful rendering fallbacks behind one public
header:

```cpp
#include <RenUI/RenUI.hpp>
```

The CMake target is `RenUI::RenUI`.

RenUI is currently at version **0.1.4**. It is pre-1.0 software: the library is
usable, but source and API compatibility may change between minor releases
while the public interface settles. Pin a release tag or commit when consuming
it from another project.

<img width="492" height="320" alt="image" src="https://github.com/user-attachments/assets/393a95e2-c172-4908-9ece-71558c5cf881" />
<img width="804" height="924" alt="image" src="https://github.com/user-attachments/assets/c244ce89-6a11-4ced-90d5-1cbde8752068" />
<img width="1309" height="684" alt="image" src="https://github.com/user-attachments/assets/9a1cd7a7-42dd-42a7-95d5-dbf2a47012b9" />

Features:
- Buttons and text input fields, including password/masked input and state handling
- Tileable and collapsible panels/menus
- Continuous and stepped sliders
- Hover tooltips
- Tables and structured data layouts
- Custom asset/atlas mapping
- Generalized themes and semantic colour schemes
- Scalable UI/text rendering
- Disabled, hover, active and focus states
- Reusable resource loading abstraction for loose files / packed assets
- Optional shader-backed UI effects
- SFML 3 integration with C++17

## Requirements

- CMake 3.16 or newer
- A C++17 compiler
- SFML 3 with the Graphics, Window, and System components

RenUI first looks for an SFML CMake package. If that is unavailable, it can
fetch the tested SFML release when `RENUI_FETCH_SFML=ON`, or use SFML 3 modules
discovered through `pkg-config`. SFML is not stored in this repository. Without
one of those discovery paths, configuration stops with an actionable error.

Fetching SFML from source requires CMake 3.24 or newer on Windows and CMake 3.22
or newer on other platforms. The fetched tag defaults to SFML 3.0.2 and can be
changed through the advanced `RENUI_SFML_GIT_TAG` cache setting.

## Build from a checkout

Clone the repository, then configure and build from its root:

```sh
git clone https://github.com/14Mikolaj/RenUI.git
cd RenUI
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSFML_DIR=/path/to/SFML/lib/cmake/SFML
cmake --build build --config Release
```

`SFML_DIR` is optional when SFML is already discoverable through
`CMAKE_PREFIX_PATH`, the platform package manager, or `pkg-config`.

Alternatively, let the standalone build fetch its tested SFML release:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DRENUI_FETCH_SFML=ON
cmake --build build --config Release
```

Standalone builds enable the smoke test by default. Run it with:

```sh
ctest --test-dir build -C Release --output-on-failure
```

Set `RENUI_BUILD_TESTS=OFF` to omit it. Tests default to off when RenUI is
embedded through `add_subdirectory` or `FetchContent`.

RenUI follows CMake's `BUILD_SHARED_LIBS` setting. Static builds are the
default; add `-DBUILD_SHARED_LIBS=ON` when configuring to build a shared
library.

When consuming an installed package alongside a static SFML installation, set
`SFML_STATIC_LIBRARIES=ON` before `find_package(RenUI ...)` (or pass
`-DSFML_STATIC_LIBRARIES=ON` while configuring). This tells SFML's package
configuration to select its static targets.

## Install and use as a CMake package

Install the library to a prefix:

```sh
cmake --install build --config Release --prefix /path/to/renui-install
```

In the consuming project:

```cmake
find_package(RenUI 0.1.4 CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE RenUI::RenUI)
```

Point that project's configure step at the install prefix when needed:

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/renui-install
```

The `tests/install-consumer` directory is a minimal package-consumer smoke
test. After installing RenUI, it can be configured independently with:

```sh
cmake -S tests/install-consumer -B build/install-consumer -DCMAKE_PREFIX_PATH=/path/to/renui-install
cmake --build build/install-consumer --config Release
```

On Windows, consumers of a shared build must stage the RenUI and SFML DLLs next
to the executable. CMake 3.21 or newer can automate that:

```cmake
add_custom_command(TARGET my_app POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:my_app> $<TARGET_FILE_DIR:my_app>
    COMMAND_EXPAND_LISTS)
```

## Embed in another CMake project

### Source checkout

Place RenUI below the consuming project and add it directly:

```cmake
add_subdirectory(external/RenUI)
target_link_libraries(my_app PRIVATE RenUI::RenUI)
```

### FetchContent

Pinning a release keeps builds reproducible:

```cmake
include(FetchContent)

FetchContent_Declare(
    RenUI
    GIT_REPOSITORY https://github.com/14Mikolaj/RenUI.git
    GIT_TAG v0.1.4
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(RenUI)

target_link_libraries(my_app PRIVATE RenUI::RenUI)
```

### Git submodule

Add the repository to the consuming checkout:

```sh
git submodule add https://github.com/14Mikolaj/RenUI.git external/RenUI
git submodule update --init --recursive
```

Then use the same `add_subdirectory(external/RenUI)` and
`target_link_libraries(...)` calls shown above. Commit both `.gitmodules` and
the recorded submodule revision in the consuming repository.

## Initialize and draw

Call `RenUI::initialize()` before constructing UI when possible. A
`RenUI::Config` can customize the logical canvas, theme, font candidates,
optional atlas and shader paths, diagnostic sink, and resource provider.

```cpp
#include <RenUI/RenUI.hpp>

const auto capabilities = RenUI::initialize();

RenUI::Panel card({24.f, 24.f}, {320.f, 140.f});
RenUI::Label title("Settings", 18, RenUI::getTheme().textPrimary, true);
RenUI::Button save("Save", {44.f, 108.f}, {120.f, 32.f});
title.setPosition({44.f, 42.f});

// Given an existing sf::RenderWindow named window:
window.setView(RenUI::makeUIView());
card.draw(window);
title.draw(window);
save.draw(window);

// At application shutdown, after the render loop:
RenUI::shutdown();
```

`InitializationResult` reports which capabilities were enabled. The same
diagnostics are available through `RenUI::getDiagnostics()`, and applications
can receive them as they occur through `Config::diagnosticSink`.

## API overview

The umbrella header exposes labels, panels, icon sprites, buttons, checkboxes,
sliders, single- and multi-line text inputs, progress bars, dropdowns, toggle
switches, tooltips, draggable windows, scrollable lists, value-breakdown rows,
and separators. It also includes anchored/clipped UI views, theme controls,
text measurement and scaling, drawing helpers, and layout utilities.

For responsive control rows, measure widgets with `getSize()` and pass the
sizes to `layoutFlowItems()`; the returned rectangles wrap without shrinking
intrinsically fitted labels. `DraggableWindow::setSizeKeepingTopLeft()` keeps a
stable edge for horizontal rollouts, while `HorizontalReveal` provides scalar
animation state.

Application-specific widgets and policies should live in an extension target
that links `RenUI::RenUI`, keeping the core library reusable.

## Resources and fallbacks

`FileSystemResourceProvider` is used by default. Applications backed by an
archive, virtual filesystem, or another storage system can derive from
`ResourceProvider` and supply it through `Config::resources`.

The default configuration searches platform font locations. Atlas texture,
atlas metadata, and hover shader paths are empty by default, so those optional
capabilities remain disabled until the host configures them. Missing or invalid
optional resources produce diagnostics instead of making initialization fail.

Controls remain constructible and interactive before initialization and when
optional resources are unavailable. Geometry, primitive, and solid-color
fallbacks continue to render where applicable; text cannot render until a font
is available. This allows the host to decide whether a missing capability is
acceptable or should be treated as an application-level error.

## Project links

- [Report a bug or request a feature](https://github.com/14Mikolaj/RenUI/issues/new/choose)
- [Contributing guide](https://github.com/14Mikolaj/RenUI/blob/main/CONTRIBUTING.md)
- [Security policy](https://github.com/14Mikolaj/RenUI/security/policy)
- [Changelog](https://github.com/14Mikolaj/RenUI/blob/main/CHANGELOG.md)
- [MIT license](https://github.com/14Mikolaj/RenUI/blob/main/LICENSE)
