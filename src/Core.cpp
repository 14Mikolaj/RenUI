// Core.cpp
// Shared font cache, text scaling, UI atlas resolution, colour utilities,
// close-button helpers, and atlas icon helpers.
//
// Part of RenUI's modular implementation — paired with:
//   Widgets.cpp   — Label / Panel / IconSprite / UIButton / ProgressBar
//                          / Separator / ValueBreakdownRow
//   TextInput.cpp — UITextInput
//   Window.cpp    — Tooltip / DraggableWindow / ScrollableList

#include <RenUI/RenUI.hpp>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace RenUI {

// ═══════════════════════════════════════════════════════════════════
//  SHARED FONT CACHE
// ═══════════════════════════════════════════════════════════════════

static sf::Font s_regularFont;
static sf::Font s_boldFont;
static bool s_regularFontLoaded = false;
static bool s_boldFontLoaded = false;
static std::vector<std::uint8_t> s_regularFontBytes;
static std::vector<std::uint8_t> s_boldFontBytes;
static unsigned int s_activeFontSizeQuantum = 1;
static float s_textScaleFactor = 1.0f;
static float s_uiScaleFactor = 1.0f;
static TextScaleLevel s_textScaleLevel = TextScaleLevel::Normal;
static std::uint64_t s_textMetricsRevision = 1;
static float s_baseTextRenderOversampleFactor = 1.0f;
static float s_textRenderOversampleFactor = 1.0f;
static bool s_largeTextRefinementEnabled = false;
// Constructing an SFML texture from a Windows DLL initializer can acquire the
// OpenGL context lock while the loader lock is held.  Keep it lazy so shared
// RenUI consumers always reach main before any graphics resources are made.
static std::unique_ptr<sf::Texture> s_uiElementsAtlasTexture;
static bool s_uiAtlasSlicesResolved = false;
static std::unordered_map<std::string, sf::IntRect> s_uiAtlasSlices;
static bool s_closeWindowSliceResolved = false;
static bool s_closeWindowSliceLoaded = false;
static sf::IntRect s_closeWindowSliceRect;
static Config s_config;
static bool s_initialized = false;
static std::uint64_t s_initializationRevision = 1;
static std::vector<Diagnostic> s_diagnostics;
static InitializationResult s_initializationResult;

Theme Theme::defaults() { return {}; }

std::optional<std::string> ResourceProvider::readText(std::string_view resource) {
    auto bytes = readBinary(resource);
    if (!bytes) return std::nullopt;
    return std::string(bytes->begin(), bytes->end());
}

FileSystemResourceProvider::FileSystemResourceProvider(std::filesystem::path root)
    : root_(std::move(root)) {}

std::optional<std::vector<std::uint8_t>> FileSystemResourceProvider::readBinary(
    std::string_view resource) {
    try {
        std::filesystem::path path{std::string(resource)};
        if (!root_.empty() && !path.is_absolute()) path = root_ / path;
        std::ifstream stream(path, std::ios::binary);
        if (!stream) return std::nullopt;
        return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(stream),
                                         std::istreambuf_iterator<char>());
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<std::string> FileSystemResourceProvider::readText(std::string_view resource) {
    auto bytes = readBinary(resource);
    if (!bytes) return std::nullopt;
    return std::string(bytes->begin(), bytes->end());
}

const std::filesystem::path& FileSystemResourceProvider::root() const noexcept { return root_; }
void FileSystemResourceProvider::setRoot(std::filesystem::path root) { root_ = std::move(root); }

void reportDiagnostic(Diagnostic diagnostic) {
    s_diagnostics.push_back(diagnostic);
    if (s_config.diagnosticSink) {
        try {
            s_config.diagnosticSink(diagnostic);
        } catch (...) {
            // Diagnostics must never make the UI unusable.
        }
    } else {
        const char* level = diagnostic.severity == DiagnosticSeverity::Error
            ? "ERROR" : diagnostic.severity == DiagnosticSeverity::Warning ? "WARNING" : "INFO";
        std::cerr << "[RenUI] " << level << ": " << diagnostic.message;
        if (!diagnostic.resource.empty()) std::cerr << " (" << diagnostic.resource << ')';
        std::cerr << '\n';
    }
}

std::vector<Diagnostic> getDiagnostics() { return s_diagnostics; }
void clearDiagnostics() { s_diagnostics.clear(); }

std::shared_ptr<ResourceProvider> getResourceProvider() {
    if (!s_config.resources) {
        s_config.resources = std::make_shared<FileSystemResourceProvider>();
    }
    return s_config.resources;
}

std::optional<std::vector<std::uint8_t>> readResourceBinary(std::string_view resource) {
    try {
        auto bytes = getResourceProvider()->readBinary(resource);
        if (!bytes) {
            reportDiagnostic({DiagnosticSeverity::Warning, DiagnosticCode::MissingResource,
                              "Resource was not found; using the built-in visual fallback",
                              std::string(resource)});
        }
        return bytes;
    } catch (const std::exception& error) {
        reportDiagnostic({DiagnosticSeverity::Warning, DiagnosticCode::ResourceProviderFailure,
                          error.what(), std::string(resource)});
    } catch (...) {
        reportDiagnostic({DiagnosticSeverity::Warning, DiagnosticCode::ResourceProviderFailure,
                          "Resource provider failed; using the built-in visual fallback",
                          std::string(resource)});
    }
    return std::nullopt;
}

std::optional<std::string> readResourceText(std::string_view resource) {
    try {
        auto text = getResourceProvider()->readText(resource);
        if (!text) {
            reportDiagnostic({DiagnosticSeverity::Warning, DiagnosticCode::MissingResource,
                              "Resource was not found; using the built-in visual fallback",
                              std::string(resource)});
        }
        return text;
    } catch (const std::exception& error) {
        reportDiagnostic({DiagnosticSeverity::Warning, DiagnosticCode::ResourceProviderFailure,
                          error.what(), std::string(resource)});
    } catch (...) {
        reportDiagnostic({DiagnosticSeverity::Warning, DiagnosticCode::ResourceProviderFailure,
                          "Resource provider failed; using the built-in visual fallback",
                          std::string(resource)});
    }
    return std::nullopt;
}

static std::string toLowerAscii_(const std::string& value) {
    std::string out = value;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

static bool usesConfiguredFontSizeQuantum_(const std::string& path) {
    if (path.empty() || s_config.fontSizeQuantum <= 1) return false;
    if (s_config.fontSizeQuantizationPathTokens.empty()) return true;

    const std::string normalizedPath = toLowerAscii_(path);
    return std::any_of(
        s_config.fontSizeQuantizationPathTokens.begin(),
        s_config.fontSizeQuantizationPathTokens.end(),
        [&normalizedPath](const std::string& token) {
            return !token.empty() &&
                   normalizedPath.find(toLowerAscii_(token)) != std::string::npos;
        });
}

static float computeBaseTextRenderOversampleFactor_(float uiScaleFactor) {
    const float clampedScale = std::max(1.0f, uiScaleFactor);
    return clampedScale < 1.2f ? 1.0f : std::ceil(clampedScale);
}

static float computeTextRenderOversampleFactor_(float uiScaleFactor) {
    float factor = computeBaseTextRenderOversampleFactor_(uiScaleFactor);
    if (s_largeTextRefinementEnabled &&
        s_textScaleLevel >= TextScaleLevel::Large) {
        // Pixel-derived typefaces can lose fine outline details when their
        // native design grid is rasterized directly at intermediate sizes.
        // Render from a denser glyph atlas and downsample into the unchanged
        // logical bounds instead.
        factor = std::max(factor, 3.0f);
    }
    return factor;
}

static unsigned int quantizeUiLogicalSize_(unsigned int logicalSize) {
    if (s_activeFontSizeQuantum <= 1) {
        return logicalSize;
    }

    // Keep authored logical sizes on scaled outputs. The fixed 11-step
    // quantisation can make punctuation (notably "%") too small at higher
    // resolutions where text is already being resolution-compensated.
    if (s_config.quantizeFontSizeAtNativeRenderScaleOnly &&
        s_textScaleFactor > 1.01f) {
        return logicalSize;
    }

    const unsigned int quantum = s_activeFontSizeQuantum;
    unsigned int quantized = ((logicalSize + (quantum / 2u)) / quantum) * quantum;
    if (quantized < quantum) {
        quantized = quantum;
    }
    return quantized;
}

static float textScaleMultiplier_(TextScaleLevel level) {
    switch (level) {
        case TextScaleLevel::Small:      return 0.85f;
        case TextScaleLevel::Large:      return 1.20f;
        case TextScaleLevel::ExtraLarge: return 1.40f;
        case TextScaleLevel::Normal:
        default:                     return 1.00f;
    }
}

static void refreshTextRenderOversample_() {
    const float combinedScale = s_textScaleFactor * s_uiScaleFactor;
    s_baseTextRenderOversampleFactor =
        computeBaseTextRenderOversampleFactor_(combinedScale);
    s_textRenderOversampleFactor =
        computeTextRenderOversampleFactor_(combinedScale);
}

static bool ensureUiAtlasSlicesLoaded_() {
    if (s_uiAtlasSlicesResolved) return !s_uiAtlasSlices.empty();
    s_uiAtlasSlicesResolved = true;

    // The atlas is an optional extension point. An empty path means the host
    // did not configure it, rather than that a resource went missing.
    if (s_config.atlasTexturePath.empty() || s_config.atlasMetadataPath.empty()) {
        return false;
    }

    auto textureBytes = readResourceBinary(s_config.atlasTexturePath);
    auto atlasTexture = std::make_unique<sf::Texture>();
    if (!textureBytes || textureBytes->empty() ||
        !atlasTexture->loadFromMemory(textureBytes->data(), textureBytes->size())) {
        if (textureBytes) {
            reportDiagnostic({DiagnosticSeverity::Warning, DiagnosticCode::InvalidResource,
                              "UI atlas texture could not be decoded; using primitive controls",
                              s_config.atlasTexturePath});
        }
        return false;
    }
    atlasTexture->setSmooth(false);
    const sf::Vector2u atlasSize = atlasTexture->getSize();
    s_uiElementsAtlasTexture = std::move(atlasTexture);

    auto atlasText = readResourceText(s_config.atlasMetadataPath);
    if (!atlasText) {
        return false;
    }

    try {
        // Aseprite emits each slice name before its first bounds object. This
        // deliberately small parser keeps RenUI independent of a JSON package.
        const auto slicesPosition = atlasText->find("\"slices\"");
        if (slicesPosition == std::string::npos) {
            throw std::runtime_error("missing slices array");
        }
        const std::string slicesJson = atlasText->substr(slicesPosition);
        const std::regex slicePattern(
            "\\\"name\\\"\\s*:\\s*\\\"([^\\\"]+)\\\"[\\s\\S]*?"
            "\\\"bounds\\\"\\s*:\\s*\\{\\s*"
            "\\\"x\\\"\\s*:\\s*(-?[0-9]+)\\s*,\\s*"
            "\\\"y\\\"\\s*:\\s*(-?[0-9]+)\\s*,\\s*"
            "\\\"w\\\"\\s*:\\s*([0-9]+)\\s*,\\s*"
            "\\\"h\\\"\\s*:\\s*([0-9]+)");
        for (std::sregex_iterator it(slicesJson.begin(), slicesJson.end(), slicePattern), end;
             it != end; ++it) {
            try {
                const std::int64_t x = std::stoll((*it)[2].str());
                const std::int64_t y = std::stoll((*it)[3].str());
                const std::int64_t w = std::stoll((*it)[4].str());
                const std::int64_t h = std::stoll((*it)[5].str());
                if (x < 0 || y < 0 || w <= 0 || h <= 0) continue;

                const auto ux = static_cast<std::uint64_t>(x);
                const auto uy = static_cast<std::uint64_t>(y);
                const auto uw = static_cast<std::uint64_t>(w);
                const auto uh = static_cast<std::uint64_t>(h);
                const auto textureWidth = static_cast<std::uint64_t>(atlasSize.x);
                const auto textureHeight = static_cast<std::uint64_t>(atlasSize.y);

                // Subtraction after the origin bounds checks avoids overflow
                // from hostile or corrupt metadata such as x + w wrapping.
                if (ux > textureWidth || uy > textureHeight ||
                    uw > textureWidth - ux || uh > textureHeight - uy) {
                    continue;
                }
                if (x > std::numeric_limits<int>::max() ||
                    y > std::numeric_limits<int>::max() ||
                    w > std::numeric_limits<int>::max() ||
                    h > std::numeric_limits<int>::max()) {
                    continue;
                }

                s_uiAtlasSlices[(*it)[1].str()] = sf::IntRect(
                    {static_cast<int>(x), static_cast<int>(y)},
                    {static_cast<int>(w), static_cast<int>(h)});
            } catch (const std::exception&) {
                // Ignore a malformed slice while retaining any other valid
                // entries in the metadata document.
            }
        }
    } catch (...) {
        s_uiAtlasSlices.clear();
    }

    if (s_uiAtlasSlices.empty()) {
        reportDiagnostic({DiagnosticSeverity::Warning, DiagnosticCode::InvalidResource,
                          "UI atlas metadata contained no usable slices; using primitive controls",
                          s_config.atlasMetadataPath});
    }

    return !s_uiAtlasSlices.empty();
}

static bool ensureCloseWindowSliceLoaded_() {
    if (s_closeWindowSliceResolved) return s_closeWindowSliceLoaded;
    s_closeWindowSliceResolved = true;

    if (!ensureUiAtlasSlicesLoaded_()) return false;
    auto it = s_uiAtlasSlices.find("icon_close_window");
    if (it != s_uiAtlasSlices.end()) {
        s_closeWindowSliceRect = it->second;
        s_closeWindowSliceLoaded = true;
    }

    return s_closeWindowSliceLoaded;
}

bool initFonts(const std::string& regularPath, const std::string& boldPath) {
    auto openFont = [](sf::Font& font, std::vector<std::uint8_t>& storage,
                       const std::string& path) {
        auto bytes = readResourceBinary(path);
        if (!bytes || bytes->empty()) return false;
        storage = std::move(*bytes);
        if (font.openFromMemory(storage.data(), storage.size())) return true;
        storage.clear();
        reportDiagnostic({DiagnosticSeverity::Warning, DiagnosticCode::InvalidResource,
                          "Font could not be decoded; text rendering will use a fallback",
                          path});
        return false;
    };

    s_regularFontLoaded = openFont(s_regularFont, s_regularFontBytes, regularPath);
    s_boldFontLoaded = openFont(s_boldFont, s_boldFontBytes, boldPath);
    s_activeFontSizeQuantum =
        ((s_regularFontLoaded && usesConfiguredFontSizeQuantum_(regularPath)) ||
         (s_boldFontLoaded && usesConfiguredFontSizeQuantum_(boldPath)))
            ? std::max(1u, s_config.fontSizeQuantum)
            : 1u;
    ++s_initializationRevision;
    ++s_textMetricsRevision;
    return s_regularFontLoaded && s_boldFontLoaded;
}

const sf::Font* getRegularFont() {
    return s_regularFontLoaded ? &s_regularFont : s_boldFontLoaded ? &s_boldFont : nullptr;
}
const sf::Font* getBoldFont() {
    return s_boldFontLoaded ? &s_boldFont : s_regularFontLoaded ? &s_regularFont : nullptr;
}

InitializationResult initialize(const Config& config) {
    shutdown();
    s_config = config;
    if (!s_config.resources) {
        s_config.resources = std::make_shared<FileSystemResourceProvider>();
    }
    clearDiagnostics();

    auto openFirstDecodableFont = [](
        sf::Font& destination,
        std::vector<std::uint8_t>& destinationBytes,
        const std::vector<std::string>& paths,
        const char* role) -> std::string {
        for (const auto& path : paths) {
            if (path.empty()) continue;
            try {
                auto bytes = getResourceProvider()->readBinary(path);
                if (!bytes) continue;

                if (!bytes->empty()) {
                    sf::Font candidate;
                    if (candidate.openFromMemory(bytes->data(), bytes->size())) {
                        destination = std::move(candidate);
                        destinationBytes = std::move(*bytes);
                        return path;
                    }
                }

                reportDiagnostic({
                    DiagnosticSeverity::Warning,
                    DiagnosticCode::InvalidResource,
                    std::string(role) +
                        " font candidate could not be decoded; trying the next candidate",
                    path});
            } catch (const std::exception& error) {
                reportDiagnostic({DiagnosticSeverity::Warning,
                                  DiagnosticCode::ResourceProviderFailure,
                                  error.what(), path});
            } catch (...) {
                reportDiagnostic({DiagnosticSeverity::Warning,
                                  DiagnosticCode::ResourceProviderFailure,
                                  "Resource provider failed while probing a font fallback", path});
            }
        }
        destinationBytes.clear();
        return {};
    };

    const std::string regularPath = openFirstDecodableFont(
        s_regularFont, s_regularFontBytes, s_config.regularFontPaths, "Regular");
    const std::string boldPath = openFirstDecodableFont(
        s_boldFont, s_boldFontBytes, s_config.boldFontPaths, "Bold");
    s_regularFontLoaded = !regularPath.empty();
    s_boldFontLoaded = !boldPath.empty();
    s_activeFontSizeQuantum =
        ((s_regularFontLoaded && usesConfiguredFontSizeQuantum_(regularPath)) ||
         (s_boldFontLoaded && usesConfiguredFontSizeQuantum_(boldPath)))
            ? std::max(1u, s_config.fontSizeQuantum)
            : 1u;

    if (!s_regularFontLoaded && !s_boldFontLoaded) {
        reportDiagnostic({DiagnosticSeverity::Warning, DiagnosticCode::MissingResource,
                          "No configured font could be loaded; controls remain usable without text", {}});
    }

    const bool atlasAvailable = ensureUiAtlasSlicesLoaded_();
    bool shaderAvailable = false;
    if (s_config.enableShaders && !s_config.hoverShaderPath.empty()) {
        if (!sf::Shader::isAvailable()) {
            reportDiagnostic({DiagnosticSeverity::Info, DiagnosticCode::ShaderUnavailable,
                              "Shaders are unavailable; buttons will use a solid hover fallback", {}});
        } else {
            try {
                auto shader = getResourceProvider()->readText(s_config.hoverShaderPath);
                if (shader && !shader->empty()) {
                    sf::Shader probe;
                    shaderAvailable = probe.loadFromMemory(
                        *shader, sf::Shader::Type::Fragment);
                    if (!shaderAvailable) {
                        reportDiagnostic({DiagnosticSeverity::Warning,
                                          DiagnosticCode::InvalidResource,
                                          "Hover shader could not be compiled; using a solid fallback",
                                          s_config.hoverShaderPath});
                    }
                } else {
                    reportDiagnostic({DiagnosticSeverity::Warning, DiagnosticCode::MissingResource,
                                      "Hover shader was not found; buttons will use a solid fallback",
                                      s_config.hoverShaderPath});
                }
            } catch (...) {
                reportDiagnostic({DiagnosticSeverity::Warning,
                                  DiagnosticCode::ResourceProviderFailure,
                                  "Resource provider failed while probing the hover shader",
                                  s_config.hoverShaderPath});
            }
        }
    }

    s_initialized = true;
    InitializationResult result;
    result.initialized = true;
    result.regularFontAvailable = s_regularFontLoaded;
    result.boldFontAvailable = s_boldFontLoaded;
    result.atlasAvailable = atlasAvailable;
    result.shadersAvailable = shaderAvailable;
    result.diagnostics = getDiagnostics();
    s_initializationResult = result;
    return s_initializationResult;
}

void shutdown() {
    s_regularFont = sf::Font{};
    s_boldFont = sf::Font{};
    s_regularFontBytes.clear();
    s_boldFontBytes.clear();
    s_regularFontLoaded = false;
    s_boldFontLoaded = false;
    s_activeFontSizeQuantum = 1;
    s_uiElementsAtlasTexture.reset();
    s_uiAtlasSlices.clear();
    s_uiAtlasSlicesResolved = false;
    s_closeWindowSliceResolved = false;
    s_closeWindowSliceLoaded = false;
    s_initialized = false;
    s_initializationResult = {};
    ++s_initializationRevision;
    ++s_textMetricsRevision;
}

bool isInitialized() { return s_initialized; }
InitializationResult getInitializationResult() {
    InitializationResult result = s_initializationResult;
    result.diagnostics = getDiagnostics();
    return result;
}
std::uint64_t getInitializationRevision() { return s_initializationRevision; }
Config getConfig() { return s_config; }
Theme getTheme() { return s_config.theme; }
void setTheme(const Theme& theme) { s_config.theme = theme; }

void setUIScalePercent(float percent) {
    if (!std::isfinite(percent)) percent = 100.0f;
    setUIScaleFactor(percent / 100.0f);
}

float getUIScalePercent() { return s_uiScaleFactor * 100.0f; }

void setUIScaleFactor(float factor) {
    if (!std::isfinite(factor)) factor = 1.0f;
    s_uiScaleFactor = std::clamp(factor, 0.50f, 1.75f);
    refreshTextRenderOversample_();
}

float getUIScaleFactor() { return s_uiScaleFactor; }

void setTextScaleLevel(TextScaleLevel level) {
    setTextScaleLevel(static_cast<int>(level));
}

void setTextScaleLevel(int level) {
    const int clamped = std::clamp(
        level,
        static_cast<int>(TextScaleLevel::Small),
        static_cast<int>(TextScaleLevel::ExtraLarge));
    const auto next = static_cast<TextScaleLevel>(clamped);
    if (next == s_textScaleLevel) return;
    s_textScaleLevel = next;
    refreshTextRenderOversample_();
    ++s_textMetricsRevision;
}

TextScaleLevel getTextScaleLevel() { return s_textScaleLevel; }
int getTextScaleLevelValue() { return static_cast<int>(s_textScaleLevel); }
float getTextScaleMultiplier() { return textScaleMultiplier_(s_textScaleLevel); }

unsigned int getScaledTextLogicalSize(unsigned int logicalSize) {
    const unsigned int quantized = quantizeUiLogicalSize_(logicalSize);
    const float scaled = static_cast<float>(quantized) * getTextScaleMultiplier();
    return std::max(1u, static_cast<unsigned int>(std::round(scaled)));
}

std::uint64_t getTextMetricsRevision() { return s_textMetricsRevision; }

sf::Vector2f getNormalizedUIAnchor(UIAnchor anchor) {
    switch (anchor) {
        case UIAnchor::TopLeft:      return {0.0f, 0.0f};
        case UIAnchor::TopCenter:    return {0.5f, 0.0f};
        case UIAnchor::TopRight:     return {1.0f, 0.0f};
        case UIAnchor::CenterLeft:   return {0.0f, 0.5f};
        case UIAnchor::CenterRight:  return {1.0f, 0.5f};
        case UIAnchor::BottomLeft:   return {0.0f, 1.0f};
        case UIAnchor::BottomCenter: return {0.5f, 1.0f};
        case UIAnchor::BottomRight:  return {1.0f, 1.0f};
        case UIAnchor::Center:
        default:                     return {0.5f, 0.5f};
    }
}

sf::FloatRect getScaledUIViewport(const sf::Vector2f& normalizedAnchor) {
    const sf::Vector2f anchor{
        std::clamp(normalizedAnchor.x, 0.0f, 1.0f),
        std::clamp(normalizedAnchor.y, 0.0f, 1.0f)
    };
    const float scale = getUIScaleFactor();
    return sf::FloatRect(
        {(1.0f - scale) * anchor.x, (1.0f - scale) * anchor.y},
        {scale, scale});
}

sf::FloatRect getScaledUIViewport(UIAnchor anchor) {
    return getScaledUIViewport(getNormalizedUIAnchor(anchor));
}

sf::FloatRect getVisibleUILogicalRect(const sf::Vector2f& normalizedAnchor) {
    const sf::FloatRect viewport = getScaledUIViewport(normalizedAnchor);
    const float left = std::clamp(-viewport.position.x / viewport.size.x,
                                  0.0f, 1.0f);
    const float top = std::clamp(-viewport.position.y / viewport.size.y,
                                 0.0f, 1.0f);
    const float right = std::clamp((1.0f - viewport.position.x) / viewport.size.x,
                                   0.0f, 1.0f);
    const float bottom = std::clamp((1.0f - viewport.position.y) / viewport.size.y,
                                    0.0f, 1.0f);
    return sf::FloatRect(
        {left * s_config.logicalSize.x,
         top * s_config.logicalSize.y},
        {(right - left) * s_config.logicalSize.x,
         (bottom - top) * s_config.logicalSize.y});
}

sf::FloatRect getVisibleUILogicalRect(UIAnchor anchor) {
    return getVisibleUILogicalRect(getNormalizedUIAnchor(anchor));
}

sf::View makeUIView(const sf::Vector2f& normalizedAnchor) {
    sf::View view(sf::FloatRect(
        {0.0f, 0.0f},
        s_config.logicalSize));
    view.setViewport(getScaledUIViewport(normalizedAnchor));
    return view;
}

sf::View makeUIView(UIAnchor anchor) {
    return makeUIView(getNormalizedUIAnchor(anchor));
}

sf::View makeClippedUIView(const sf::FloatRect& logicalClip,
                           const sf::View& baseView) {
    const float width = std::max(0.001f, logicalClip.size.x);
    const float height = std::max(0.001f, logicalClip.size.y);
    sf::View view(sf::FloatRect(logicalClip.position, {width, height}));

    const sf::FloatRect baseViewport = baseView.getViewport();
    const sf::Vector2f baseSize = baseView.getSize();
    const sf::Vector2f basePosition = baseView.getCenter() - baseSize * 0.5f;
    const float logicalWidth = std::max(0.001f, baseSize.x);
    const float logicalHeight = std::max(0.001f, baseSize.y);
    view.setViewport(sf::FloatRect(
        {
            baseViewport.position.x
                + ((logicalClip.position.x - basePosition.x) / logicalWidth)
                    * baseViewport.size.x,
            baseViewport.position.y
                + ((logicalClip.position.y - basePosition.y) / logicalHeight)
                    * baseViewport.size.y
        },
        {
            (width / logicalWidth) * baseViewport.size.x,
            (height / logicalHeight) * baseViewport.size.y
        }));
    return view;
}

sf::View makeClippedUIView(const sf::FloatRect& logicalClip,
                           const sf::Vector2f& normalizedAnchor) {
    return makeClippedUIView(logicalClip, makeUIView(normalizedAnchor));
}

sf::View makeClippedUIView(const sf::FloatRect& logicalClip, UIAnchor anchor) {
    return makeClippedUIView(logicalClip, getNormalizedUIAnchor(anchor));
}

void setTextScaleFactor(float factor) {
    if (!std::isfinite(factor)) factor = 1.0f;
    s_textScaleFactor = std::max(0.01f, factor);
    refreshTextRenderOversample_();
}

float getTextScaleFactor() { return s_textScaleFactor; }

void setLargeTextRefinementEnabled(bool enabled) {
    if (enabled == s_largeTextRefinementEnabled) return;
    s_largeTextRefinementEnabled = enabled;
    refreshTextRenderOversample_();
    // Supersampling can change hinted glyph advances by fractions of a
    // logical pixel, so invalidate cached measurements as well as rendering.
    ++s_textMetricsRevision;
}

bool isLargeTextRefinementEnabled() {
    return s_largeTextRefinementEnabled;
}

void applyCrispText(sf::Text& text, unsigned int logicalSize) {
    const unsigned int effectiveLogicalSize = getScaledTextLogicalSize(logicalSize);
    const bool hasSyntheticBold = (text.getStyle() & sf::Text::Bold) != 0u;
    const bool refineLargeText =
        s_largeTextRefinementEnabled &&
        s_textScaleLevel >= TextScaleLevel::Large &&
        !hasSyntheticBold;

    // SFML emboldens an outline by one raster pixel regardless of glyph size.
    // Applying the additional 3x refinement to synthetic-bold text would then
    // downscale that stroke to roughly one third of its previous logical weight.
    // Keep bold runs on the pre-refinement crisp scale so their established
    // weight and any caller-provided outlines remain readable.
    const float renderScale = std::max(
        1.0f,
        refineLargeText
            ? s_textRenderOversampleFactor
            : s_baseTextRenderOversampleFactor);
    unsigned int renderCharacterSize = std::max(
        effectiveLogicalSize,
        static_cast<unsigned int>(
            std::ceil(static_cast<float>(effectiveLogicalSize) * renderScale)));

    if (refineLargeText && s_activeFontSizeQuantum > 1u) {
        // Pixel-derived typefaces retain their intended stems and counters
        // when FreeType samples them on a native design-grid multiple.
        const unsigned int quantum = s_activeFontSizeQuantum;
        renderCharacterSize =
            ((renderCharacterSize + quantum - 1u) / quantum) * quantum;
    }

    const float actualRenderScale =
        static_cast<float>(renderCharacterSize) /
        static_cast<float>(effectiveLogicalSize);
    text.setCharacterSize(renderCharacterSize);
    text.setScale({1.0f / actualRenderScale, 1.0f / actualRenderScale});
}

sf::Vector2f snapToTextPixelGrid(const sf::Vector2f& position) {
    const float sf = std::max(1.0f, s_textScaleFactor * s_uiScaleFactor);
    return {
        std::round(position.x * sf) / sf,
        std::round(position.y * sf) / sf
    };
}

sf::Vector2f snapToViewPixelGrid(const sf::RenderWindow& window,
                                 const sf::Vector2f& worldPosition) {
    const sf::View& view = window.getView();
    const sf::Vector2i pixel = window.mapCoordsToPixel(worldPosition, view);
    return window.mapPixelToCoords(pixel, view);
}

void snapTextToPixelGrid(sf::Text& text) {
    text.setPosition(snapToTextPixelGrid(text.getPosition()));
}

sf::FloatRect getScaledLocalBounds(const sf::Text& text) {
    sf::FloatRect b = text.getLocalBounds();
    sf::Vector2f  s = text.getScale();
    return { {b.position.x * s.x, b.position.y * s.y}, {b.size.x * s.x, b.size.y * s.y} };
}

sf::Color getTextSelectionFillColor() {
    return getTheme().selectionFill;
}

// ═══════════════════════════════════════════════════════════════════
//  COLOUR UTILITIES
// ═══════════════════════════════════════════════════════════════════

static sf::Clock s_rainbowClock;

sf::Color hsvToColor(float h, float s, float v) {
    h = std::fmod(h, 360.0f);
    if (h < 0.0f) h += 360.0f;
    float c = v * s;
    float x = c * (1.0f - std::fabs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;
    float r = 0, g = 0, b = 0;
    if      (h < 60)  { r = c; g = x; }
    else if (h < 120) { r = x; g = c; }
    else if (h < 180) { g = c; b = x; }
    else if (h < 240) { g = x; b = c; }
    else if (h < 300) { r = x; b = c; }
    else              { r = c; b = x; }
    return sf::Color(static_cast<std::uint8_t>((r + m) * 255),
                     static_cast<std::uint8_t>((g + m) * 255),
                     static_cast<std::uint8_t>((b + m) * 255));
}

void drawRainbowText(sf::RenderWindow& window, const std::string& text,
                     float x, float y, unsigned int fontSize) {
    const sf::Font* font = getBoldFont();
    if (!font) {
        font = getRegularFont();
    }
    if (!font || text.empty()) return;

    float t = s_rainbowClock.getElapsedTime().asSeconds() * 360.0f;
    float charSpread = 30.0f;

    // Keep the individual rainbow glyphs on the same layout as an ordinary
    // bold text run. In particular, applyCrispText can quantize or
    // oversample the render size, so independently deriving advances from
    // the requested fontSize lets long names drift past Tooltip's bounds.
    sf::Text layoutText(*font, text, fontSize);
    layoutText.setStyle(sf::Text::Bold);
    applyCrispText(layoutText, fontSize);

    sf::Text charText(*font);
    charText.setStyle(sf::Text::Bold);
    applyCrispText(charText, fontSize);

    const sf::String& layoutString = layoutText.getString();
    for (size_t i = 0; i < layoutString.getSize(); ++i) {
        float hue = t + static_cast<float>(i) * charSpread;
        sf::Color col = hsvToColor(hue, 1.0f, 1.0f);
        charText.setString(layoutString.substring(i, 1));
        charText.setFillColor(col);
        charText.setPosition({x + layoutText.findCharacterPos(i).x, y});
        snapTextToPixelGrid(charText);
        window.draw(charText);
    }
}

// ═══════════════════════════════════════════════════════════════════
//  UI ATLAS HELPERS
// ═══════════════════════════════════════════════════════════════════

void drawCross(sf::RenderWindow& window, const sf::FloatRect& rect,
               sf::Color color, float inset, float thickness) {
    const float minimumDimension = std::max(0.0f, std::min(rect.size.x, rect.size.y));
    const float maximumInset = std::max(0.0f, (minimumDimension - 1.0f) * 0.5f);
    const float clampedInset = std::clamp(inset, 0.0f, maximumInset);
    const float clampedThickness = std::max(0.5f, thickness);

    const auto drawStroke = [&](const sf::Vector2f& start,
                                const sf::Vector2f& end) {
        const sf::Vector2f delta = end - start;
        const float length = std::hypot(delta.x, delta.y);
        if (length <= 0.0f) return;

        sf::RectangleShape stroke({length, clampedThickness});
        stroke.setOrigin({0.0f, clampedThickness * 0.5f});
        stroke.setPosition(start);
        stroke.setRotation(sf::radians(std::atan2(delta.y, delta.x)));
        stroke.setFillColor(color);
        window.draw(stroke);
    };

    const float left = rect.position.x + clampedInset;
    const float top = rect.position.y + clampedInset;
    const float right = rect.position.x + rect.size.x - clampedInset;
    const float bottom = rect.position.y + rect.size.y - clampedInset;
    drawStroke({left, top}, {right, bottom});
    drawStroke({right, top}, {left, bottom});
}

void drawGeometryGlyph(sf::RenderWindow& window, GeometryGlyph glyph,
                       const sf::FloatRect& rect, sf::Color color,
                       float thickness) {
    if (rect.size.x <= 0.0f || rect.size.y <= 0.0f) return;

    const float clampedThickness = std::max(0.5f, thickness);
    const sf::Vector2f center{
        rect.position.x + rect.size.x * 0.5f,
        rect.position.y + rect.size.y * 0.5f};
    const float minimumDimension = std::min(rect.size.x, rect.size.y);

    const auto drawStroke = [&](const sf::Vector2f& start,
                                const sf::Vector2f& end) {
        const sf::Vector2f delta = end - start;
        const float length = std::hypot(delta.x, delta.y);
        if (length <= 0.0f) return;

        sf::RectangleShape stroke({length, clampedThickness});
        stroke.setOrigin({0.0f, clampedThickness * 0.5f});
        stroke.setPosition(start);
        stroke.setRotation(sf::radians(std::atan2(delta.y, delta.x)));
        stroke.setFillColor(color);
        window.draw(stroke);
    };

    switch (glyph) {
        case GeometryGlyph::Target: {
            const float radius = minimumDimension * 0.42f;
            sf::CircleShape ring(radius, 20);
            ring.setPosition({center.x - radius, center.y - radius});
            ring.setFillColor(sf::Color::Transparent);
            ring.setOutlineColor(color);
            ring.setOutlineThickness(clampedThickness);
            window.draw(ring);

            const float dotRadius = std::max(1.0f, minimumDimension * 0.12f);
            sf::CircleShape dot(dotRadius, 12);
            dot.setPosition({center.x - dotRadius, center.y - dotRadius});
            dot.setFillColor(color);
            window.draw(dot);
            break;
        }
        case GeometryGlyph::Eye: {
            const float radius = minimumDimension * 0.5f;
            sf::CircleShape eye(radius, 20);
            eye.setScale({rect.size.x / minimumDimension, 0.75f});
            eye.setPosition({rect.position.x, center.y - radius * 0.75f});
            eye.setFillColor(sf::Color::Transparent);
            eye.setOutlineColor(color);
            eye.setOutlineThickness(clampedThickness);
            window.draw(eye);

            const float pupilRadius = std::max(1.0f, minimumDimension * 0.16f);
            sf::CircleShape pupil(pupilRadius, 12);
            pupil.setPosition({center.x - pupilRadius, center.y - pupilRadius});
            pupil.setFillColor(color);
            window.draw(pupil);
            break;
        }
        case GeometryGlyph::ChevronRight:
            drawStroke(
                {rect.position.x + rect.size.x * 0.25f,
                 rect.position.y + rect.size.y * 0.1f},
                {rect.position.x + rect.size.x * 0.75f, center.y});
            drawStroke(
                {rect.position.x + rect.size.x * 0.75f, center.y},
                {rect.position.x + rect.size.x * 0.25f,
                 rect.position.y + rect.size.y * 0.9f});
            break;
    }
}

void drawCloseWindowButton(sf::RenderWindow& window, const sf::FloatRect& rect, bool hovered) {
    if (ensureCloseWindowSliceLoaded_()) {
        sf::Sprite closeIcon(*s_uiElementsAtlasTexture, s_closeWindowSliceRect);
        float iconW = static_cast<float>(std::max(1, s_closeWindowSliceRect.size.x));
        float iconH = static_cast<float>(std::max(1, s_closeWindowSliceRect.size.y));
        float pad = hovered ? 1.0f : 2.0f;
        float scale = std::min((rect.size.x - pad * 2.0f) / iconW,
                               (rect.size.y - pad * 2.0f) / iconH);
        if (hovered) scale *= 1.05f;
        if (scale <= 0.0f) scale = 1.0f;
        closeIcon.setScale({scale, scale});

        float drawW = iconW * scale;
        float drawH = iconH * scale;
        closeIcon.setPosition({rect.position.x + (rect.size.x - drawW) * 0.5f,
                               rect.position.y + (rect.size.y - drawH) * 0.5f});
        closeIcon.setColor(hovered ? sf::Color(255, 255, 255, 255)
                                   : sf::Color(220, 220, 230, 225));
        window.draw(closeIcon);
        return;
    }

    // Primitive fallback: unlike a text "X", this remains visible when no
    // atlas and no font can be loaded.
    const Theme theme = getTheme();
    const sf::Color color = hovered ? theme.buttonDangerHover
                                    : theme.buttonDangerBorder;
    const float inset = std::max(
        2.0f, std::min(rect.size.x, rect.size.y) * (hovered ? 0.20f : 0.25f));
    drawCross(window, rect, color, inset, hovered ? 2.0f : 1.5f);
}

bool getUiAtlasSliceRect(const std::string& sliceName, sf::IntRect& outRect) {
    if (!ensureUiAtlasSlicesLoaded_()) return false;
    auto it = s_uiAtlasSlices.find(sliceName);
    if (it == s_uiAtlasSlices.end()) return false;
    outRect = it->second;
    return true;
}

void drawUiAtlasIconButton(sf::RenderWindow& window, const sf::FloatRect& rect,
                           const std::string& sliceName, bool hovered,
                           bool enabled, bool drawFrame) {
    if (drawFrame) {
        sf::RectangleShape bg(sf::Vector2f(rect.size.x, rect.size.y));
        bg.setPosition(rect.position);
        if (!enabled) {
            bg.setFillColor(sf::Color(55, 60, 72, 170));
            bg.setOutlineColor(sf::Color(92, 100, 114, 140));
        } else if (hovered) {
            bg.setFillColor(sf::Color(74, 82, 106, 220));
            bg.setOutlineColor(sf::Color(96, 106, 130, 200));
        } else {
            bg.setFillColor(sf::Color(60, 66, 86, 210));
            bg.setOutlineColor(sf::Color(96, 106, 130, 200));
        }
        bg.setOutlineThickness(1.0f);
        window.draw(bg);
    }

    sf::IntRect sliceRect;
    if (!getUiAtlasSliceRect(sliceName, sliceRect)) {
        return;
    }

    sf::Sprite icon(*s_uiElementsAtlasTexture, sliceRect);
    float iconW = static_cast<float>(std::max(1, sliceRect.size.x));
    float iconH = static_cast<float>(std::max(1, sliceRect.size.y));
    float scale = std::min(rect.size.x / iconW, rect.size.y / iconH);
    if (scale <= 0.0f) scale = 1.0f;
    icon.setScale({scale, scale});
    float drawW = iconW * scale;
    float drawH = iconH * scale;
    icon.setPosition({rect.position.x + (rect.size.x - drawW) * 0.5f,
                      rect.position.y + (rect.size.y - drawH) * 0.5f});
    if (!enabled) {
        icon.setColor(sf::Color(170, 176, 188, 180));
    } else if (hovered) {
        icon.setColor(sf::Color(245, 248, 255, 255));
    } else {
        icon.setColor(sf::Color(170, 180, 200, 140));
    }
    window.draw(icon);
}

float drawUiAtlasIcon(sf::RenderWindow& window, const std::string& sliceName,
                      float x, float y, float displaySize,
                      sf::Color tint) {
    sf::IntRect sliceRect;
    if (!getUiAtlasSliceRect(sliceName, sliceRect)) return 0.0f;

    sf::Sprite icon(*s_uiElementsAtlasTexture, sliceRect);
    float iconW = static_cast<float>(std::max(1, sliceRect.size.x));
    float iconH = static_cast<float>(std::max(1, sliceRect.size.y));
    float scale = displaySize / std::max(iconW, iconH);
    if (scale <= 0.0f) scale = 1.0f;
    icon.setScale({scale, scale});
    icon.setPosition({x, y});
    icon.setColor(tint);
    window.draw(icon);
    return iconW * scale;
}

void drawDirectionalMarker(sf::RenderWindow& window,
                           const sf::Vector2f& center,
                           float displaySize,
                           sf::Angle rotation,
                           sf::Color tint) {
    drawDirectionalMarker(window, center, displaySize, rotation,
                          "direction_indicator_grey", tint);
}

void drawDirectionalMarker(sf::RenderWindow& window,
                           const sf::Vector2f& center,
                           float displaySize,
                           sf::Angle rotation,
                           const std::string& sliceName,
                           sf::Color tint) {
    const float size = std::max(1.0f, displaySize);
    sf::IntRect sliceRect;
    if (getUiAtlasSliceRect(sliceName, sliceRect) &&
        s_uiElementsAtlasTexture) {
        sf::Sprite icon(*s_uiElementsAtlasTexture, sliceRect);
        const float iconW = static_cast<float>(std::max(1, sliceRect.size.x));
        const float iconH = static_cast<float>(std::max(1, sliceRect.size.y));
        const float scale = size / std::max(iconW, iconH);
        icon.setOrigin({iconW * 0.5f, iconH * 0.5f});
        icon.setPosition(center);
        icon.setScale({scale, scale});
        icon.setRotation(rotation);
        icon.setColor(tint);
        window.draw(icon);
        return;
    }

    // The atlas is optional. Keep the targeting/navigation affordance visible
    // with a small upward-pointing vector arrow when no image is available.
    sf::ConvexShape arrow(4);
    arrow.setPoint(0, {size * 0.5f, 0.0f});
    arrow.setPoint(1, {size, size});
    arrow.setPoint(2, {size * 0.5f, size * 0.72f});
    arrow.setPoint(3, {0.0f, size});
    arrow.setOrigin({size * 0.5f, size * 0.5f});
    arrow.setPosition(center);
    arrow.setRotation(rotation);
    arrow.setFillColor(tint);
    arrow.setOutlineColor(getTheme().panelBorder);
    arrow.setOutlineThickness(std::max(1.0f, size * 0.06f));
    window.draw(arrow);
}

// ═══════════════════════════════════════════════════════════════════
//  LIGHTWEIGHT DRAW HELPERS
// ═══════════════════════════════════════════════════════════════════

sf::FloatRect drawText(sf::RenderWindow& window, const std::string& text,
                       float x, float y, unsigned int fontSize,
                       sf::Color color, bool bold) {
    const sf::Font* font = bold ? getBoldFont() : getRegularFont();
    if (!font) return {};
    sf::Text t(*font, text, fontSize);
    if (bold) t.setStyle(sf::Text::Bold);
    applyCrispText(t, fontSize);
    t.setFillColor(color);
    t.setPosition(snapToTextPixelGrid({x, y}));
    window.draw(t);
    return getScaledLocalBounds(t);
}

sf::FloatRect drawTextCentered(sf::RenderWindow& window, const std::string& text,
                               float centerX, float y, unsigned int fontSize,
                               sf::Color color, bool bold) {
    const sf::Font* font = bold ? getBoldFont() : getRegularFont();
    if (!font) return {};
    sf::Text t(*font, text, fontSize);
    if (bold) t.setStyle(sf::Text::Bold);
    applyCrispText(t, fontSize);
    t.setFillColor(color);
    sf::FloatRect b = getScaledLocalBounds(t);
    t.setPosition(snapToTextPixelGrid({centerX - b.size.x / 2.0f - b.position.x, y}));
    window.draw(t);
    return getScaledLocalBounds(t);
}

sf::FloatRect drawTextOutlined(sf::RenderWindow& window, const std::string& text,
                               float x, float y, unsigned int fontSize,
                               sf::Color fill, sf::Color outline,
                               float outlineThickness, bool bold) {
    const sf::Font* font = bold ? getBoldFont() : getRegularFont();
    if (!font) return {};
    sf::Text t(*font, text, fontSize);
    if (bold) t.setStyle(sf::Text::Bold);
    applyCrispText(t, fontSize);
    t.setFillColor(fill);
    t.setOutlineColor(outline);
    t.setOutlineThickness(outlineThickness);
    t.setPosition(snapToTextPixelGrid({x, y}));
    window.draw(t);
    return getScaledLocalBounds(t);
}

void drawModalBackdrop(sf::RenderWindow& window, std::uint8_t alpha) {
    const sf::View callerView = window.getView();
    sf::View backdropView(sf::FloatRect(
        {0.0f, 0.0f},
        s_config.logicalSize));
    backdropView.setViewport(sf::FloatRect({0.0f, 0.0f}, {1.0f, 1.0f}));
    window.setView(backdropView);

    sf::RectangleShape dim(sf::Vector2f(
        s_config.logicalSize.x, s_config.logicalSize.y));
    dim.setPosition({0.0f, 0.0f});
    dim.setFillColor(sf::Color(0, 0, 0, alpha));
    window.draw(dim);

    window.setView(callerView);
}

void drawColorSwatch(sf::RenderWindow& window, const sf::Vector2f& position,
                     float size, sf::Color color, bool selected,
                     sf::Color selectedOutline) {
    sf::RectangleShape swatch(sf::Vector2f(size, size));
    swatch.setPosition(position);
    swatch.setFillColor(color);
    if (selected) {
        swatch.setOutlineColor(selectedOutline);
        swatch.setOutlineThickness(3.0f);
    } else {
        swatch.setOutlineColor(sf::Color(80, 80, 80));
        swatch.setOutlineThickness(1.0f);
    }
    window.draw(swatch);
}

void drawFilledRect(sf::RenderWindow& window, const sf::FloatRect& rect,
                    sf::Color fill, sf::Color outline, float outlineThickness) {
    sf::RectangleShape r(sf::Vector2f(rect.size.x, rect.size.y));
    r.setPosition(rect.position);
    r.setFillColor(fill);
    if (outlineThickness > 0.0f) {
        r.setOutlineColor(outline);
        r.setOutlineThickness(outlineThickness);
    }
    window.draw(r);
}

// ═══════════════════════════════════════════════════════════════════
//  VERTICAL STACK
// ═══════════════════════════════════════════════════════════════════

VerticalStack::VerticalStack(const sf::Vector2f& start, float gap)
    : cursor_(start), gap_(std::max(0.0f, gap)) {}

sf::Vector2f VerticalStack::place(float itemHeight) {
    const sf::Vector2f placed = cursor_;
    cursor_.y += std::max(0.0f, itemHeight) + gap_;
    return placed;
}

void VerticalStack::addSpacing(float extra) {
    cursor_.y += std::max(0.0f, extra);
}

void VerticalStack::setGap(float gap) {
    gap_ = std::max(0.0f, gap);
}

float VerticalStack::currentY() const {
    return cursor_.y;
}

} // namespace RenUI
