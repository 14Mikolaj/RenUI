#pragma once

#include <RenUI/Diagnostics.hpp>
#include <RenUI/Resources.hpp>
#include <RenUI/Theme.hpp>

#include <SFML/System/Vector2.hpp>

#include <memory>
#include <cstdint>
#include <string>
#include <vector>

namespace RenUI {

struct Config {
    sf::Vector2f logicalSize{1920.f, 1080.f};
    std::vector<std::string> regularFontPaths{
#ifdef _WIN32
        "C:/Windows/Fonts/consola.ttf",
#elif defined(__APPLE__)
        "/System/Library/Fonts/Menlo.ttc",
#else
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/truetype/liberation2/LiberationMono-Regular.ttf"
#endif
    };
    std::vector<std::string> boldFontPaths{
#ifdef _WIN32
        "C:/Windows/Fonts/consolab.ttf",
#elif defined(__APPLE__)
        "/System/Library/Fonts/Menlo.ttc",
#else
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf",
        "/usr/share/fonts/truetype/liberation2/LiberationMono-Bold.ttf"
#endif
    };
    std::string atlasTexturePath;
    std::string atlasMetadataPath;
    std::string hoverShaderPath;
    std::shared_ptr<ResourceProvider> resources;
    DiagnosticSink diagnosticSink;
    Theme theme{Theme::defaults()};
    bool enableShaders{true};
    unsigned int fontSizeQuantum{1};
    bool quantizeFontSizeAtNativeRenderScaleOnly{true};
    std::vector<std::string> fontSizeQuantizationPathTokens;
};

struct InitializationResult {
    bool initialized{false};
    bool regularFontAvailable{false};
    bool boldFontAvailable{false};
    bool atlasAvailable{false};
    bool shadersAvailable{false};
    std::vector<Diagnostic> diagnostics;

    explicit operator bool() const noexcept { return initialized; }
};

RENUI_API InitializationResult initialize(const Config& config = {});
RENUI_API void shutdown();
RENUI_API bool isInitialized();
RENUI_API InitializationResult getInitializationResult();
RENUI_API std::uint64_t getInitializationRevision();
RENUI_API Config getConfig();
RENUI_API Theme getTheme();
RENUI_API void setTheme(const Theme& theme);
RENUI_API std::shared_ptr<ResourceProvider> getResourceProvider();

RENUI_API std::optional<std::vector<std::uint8_t>> readResourceBinary(
    std::string_view resource);
RENUI_API std::optional<std::string> readResourceText(std::string_view resource);
RENUI_API void reportDiagnostic(Diagnostic diagnostic);

} // namespace RenUI
