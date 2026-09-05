# Changelog

All notable changes to RenUI are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
RenUI is pre-1.0, so minor releases may include source or API compatibility
changes.

## [Unreleased]

## [0.1.12] - 2026-09-03

### Added

- Added live atlas slice aliases so existing widgets can switch between
  style-specific slice families without reconstruction.
- Added an optional process-wide nine-slice skin for framed text UIButtons,
  with semantic hover, selected, focus, disabled, primary, and danger cues.

## [0.1.11] - 2026-09-02

### Added

- Added optional visual-only unavailable close artwork for nine-slice panels
  that retain corner chrome without exposing a dismiss hit target.

## [0.1.10] - 2026-09-02

### Added

- Added a live process-wide nine-slice tint for application opacity settings.

### Fixed

- Kept close artwork at a stable footprint when switching to its hover slice.

## [0.1.9] - 2026-09-02

### Added

- Added an ABI-preserving tinted nine-slice draw overload so translucent HUD
  panels can use atlas chrome without losing their composited opacity.

## [0.1.8] - 2026-09-02

### Added

- Added optional named-atlas nine-slice panel skins with proportional small-
  panel scaling, optional top-right close artwork and hover state, stable
  corner hit geometry, mapped-pointer window input, and primitive fallback
  when slices are unavailable.

## [0.1.7] - 2026-08-31

### Security

- Prevented masked `UITextInput` values from being copied to the clipboard;
  cut still removes the selected secret without exporting it.

## [0.1.6] - 2026-08-31

### Added

- Added a mapped-pointer `UITextArea::handleEvent` overload so hosts rendering
  through scaled or anchored views can route press and selection-drag input in
  the same logical coordinate space as the control.

## [0.1.5] - 2026-08-29

### Added

- Added a named-slice directional-marker overload so applications can render
  distinct rotatable navigation and targeting indicators with the existing
  atlas-independent vector fallback.

## [0.1.4] - 2026-08-26

### Fixed

- Added the explicit standard-library math dependency required by portable
  Linux builds.
- Made installed-package CI discovery portable across PowerShell, Git Bash,
  and POSIX shells, with graphical smoke tests running under a virtual X
  display and staged shared libraries available on headless Linux runners.

## [0.1.3] - 2026-08-26

### Added

- Initial public release of the C++17/SFML 3 UI library.
- Reusable controls, screen-space layout helpers, theming, UI and text scaling,
  and crisp-text utilities.
- Configurable filesystem or application-provided resource loading.
- Capability reporting, structured diagnostics, and rendering fallbacks for
  unavailable optional resources.
- Static or shared CMake builds, optional fetching of a tested SFML release,
  installable package metadata, and source/install consumer smoke tests.

[Unreleased]: https://github.com/14Mikolaj/RenUI/compare/v0.1.12...HEAD
[0.1.12]: https://github.com/14Mikolaj/RenUI/releases/tag/v0.1.12
[0.1.11]: https://github.com/14Mikolaj/RenUI/releases/tag/v0.1.11
[0.1.10]: https://github.com/14Mikolaj/RenUI/releases/tag/v0.1.10
[0.1.9]: https://github.com/14Mikolaj/RenUI/releases/tag/v0.1.9
[0.1.8]: https://github.com/14Mikolaj/RenUI/releases/tag/v0.1.8
[0.1.7]: https://github.com/14Mikolaj/RenUI/releases/tag/v0.1.7
[0.1.6]: https://github.com/14Mikolaj/RenUI/releases/tag/v0.1.6
[0.1.5]: https://github.com/14Mikolaj/RenUI/releases/tag/v0.1.5
[0.1.4]: https://github.com/14Mikolaj/RenUI/releases/tag/v0.1.4
[0.1.3]: https://github.com/14Mikolaj/RenUI/releases/tag/v0.1.3
