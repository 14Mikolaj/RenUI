# Changelog

All notable changes to RenUI are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
RenUI is pre-1.0, so minor releases may include source or API compatibility
changes.

## [Unreleased]

## [0.1.4] - 2026-08-26

### Fixed

- Added the explicit standard-library math dependency required by portable
  Linux builds.
- Made installed-package CI discovery portable across PowerShell, Git Bash,
  and POSIX shells, with graphical smoke tests running under a virtual X
  display on headless Linux runners.

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

[Unreleased]: https://github.com/14Mikolaj/RenUI/compare/v0.1.4...HEAD
[0.1.4]: https://github.com/14Mikolaj/RenUI/releases/tag/v0.1.4
[0.1.3]: https://github.com/14Mikolaj/RenUI/releases/tag/v0.1.3
