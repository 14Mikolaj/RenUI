// RenUI.hpp
// Standalone, reusable UI element library.
// Provides reusable widgets: panels, buttons, text inputs, labels, tooltips,
// scrollable lists, progress bars, and draggable windows.
// All widgets render in screen-space using an application-provided SFML view.

#pragma once

#include <RenUI/Export.hpp>
#include <RenUI/Config.hpp>
#include <RenUI/Diagnostics.hpp>
#include <RenUI/Resources.hpp>
#include <RenUI/Theme.hpp>
#include <RenUI/Version.hpp>

#include <SFML/Graphics.hpp>
#include <string>
#include <string_view>
#include <vector>
#include <functional>
#include <memory>
#include <optional>
#include <cstdint>
#include <algorithm>
#include <chrono>

// ═══════════════════════════════════════════════════════════════════
//  SHARED FONT CACHE
// ═══════════════════════════════════════════════════════════════════
// Avoids every widget loading its own copy of the same font file.

namespace RenUI {

// Direct font initialization convenience. Config-based initialize() is
// preferred for fallback candidates and diagnostics.
RENUI_API bool initFonts(const std::string& regularPath, const std::string& boldPath);

// Retrieve cached fonts. Returns nullptr while no font is available. Widgets
// automatically rebind their cached text when initialize()/shutdown() changes
// the active font revision.
RENUI_API const sf::Font* getRegularFont();
RENUI_API const sf::Font* getBoldFont();

// ── User-facing UI and text scaling ──
// UI scale changes widget geometry through scaled SFML view viewports. It is
// intentionally separate from text scale, which changes only logical glyph
// size. Percent values are clamped to the supported UI range (50..175).
RENUI_API void  setUIScalePercent(float percent);
RENUI_API float getUIScalePercent();
RENUI_API void  setUIScaleFactor(float factor);
RENUI_API float getUIScaleFactor();

enum class TextScaleLevel : int {
    Small = 1,
    Normal = 2,
    Large = 3,
    ExtraLarge = 4
};

RENUI_API void setTextScaleLevel(TextScaleLevel level);
RENUI_API void setTextScaleLevel(int level);
RENUI_API TextScaleLevel getTextScaleLevel();
RENUI_API int getTextScaleLevelValue();
RENUI_API float getTextScaleMultiplier();

// Resolves an authored font size through the semantic text-size preference.
// Exposed so responsive widgets can reserve the correct amount of space.
RENUI_API unsigned int getScaledTextLogicalSize(unsigned int logicalSize);

// Incremented whenever semantic text metrics change. Cached wrapping/layout
// code can rebuild lazily without reacting to resolution-only oversampling.
RENUI_API std::uint64_t getTextMetricsRevision();

// A scaled UI view keeps its authored 0..WINDOW_WIDTH/HEIGHT coordinate space,
// but scales its viewport around an anchor. This preserves edge placement and
// scales the padding within an interface cluster. Arbitrary normalized anchors are
// clamped component-wise to 0..1.
enum class UIAnchor {
    TopLeft,
    TopCenter,
    TopRight,
    CenterLeft,
    Center,
    CenterRight,
    BottomLeft,
    BottomCenter,
    BottomRight
};

RENUI_API sf::Vector2f getNormalizedUIAnchor(UIAnchor anchor);
RENUI_API sf::FloatRect getScaledUIViewport(UIAnchor anchor);
RENUI_API sf::FloatRect getScaledUIViewport(const sf::Vector2f& normalizedAnchor);
// Portion of authored 0..WINDOW_WIDTH/HEIGHT coordinates that remains visible
// after the scaled viewport is clipped by the physical window.
RENUI_API sf::FloatRect getVisibleUILogicalRect(UIAnchor anchor);
RENUI_API sf::FloatRect getVisibleUILogicalRect(const sf::Vector2f& normalizedAnchor);
RENUI_API sf::View makeUIView(UIAnchor anchor = UIAnchor::Center);
RENUI_API sf::View makeUIView(const sf::Vector2f& normalizedAnchor);
RENUI_API sf::View makeClippedUIView(const sf::FloatRect& logicalClip,
                           UIAnchor anchor = UIAnchor::Center);
RENUI_API sf::View makeClippedUIView(const sf::FloatRect& logicalClip,
                           const sf::Vector2f& normalizedAnchor);
// Compose a logical clip with an existing UI view. This is useful inside a
// reusable widget that does not know which anchor its caller selected.
RENUI_API sf::View makeClippedUIView(const sf::FloatRect& logicalClip,
                           const sf::View& baseView);

// ── Text oversampling for crisp rendering at non-native resolutions ──
// Call when the window resolution changes.
// factor = physicalWindowWidth / logicalViewWidth  (e.g. 2560/1920 = 1.333)
RENUI_API void  setTextScaleFactor(float factor);
RENUI_API float getTextScaleFactor();

// Apply crisp oversampling to an sf::Text: sets character size to
// logicalSize * scaleFactor and applies inverse scale so the text
// occupies the same logical space while using a higher-res glyph. Assign the
// intended sf::Text style before calling. Large-text refinement deliberately
// keeps synthetic Bold on the baseline crisp scale because SFML's fixed
// raster-pixel emboldening would otherwise become too thin after downsampling.
RENUI_API void applyCrispText(sf::Text& text, unsigned int logicalSize);

// When enabled, Large and Extra Large semantic text is rasterized from the
// font outline at a minimum 3x resolution, aligned to the active font's native
// size quantum, then downsampled into the same logical geometry. This preserves
// fine glyph details at otherwise awkward pixel sizes without changing UI scale.
// Synthetic-bold runs retain their baseline raster scale and visual weight.
RENUI_API void setLargeTextRefinementEnabled(bool enabled);
RENUI_API bool isLargeTextRefinementEnabled();

// Snap an sf::Text position to the current pixel grid implied by the
// oversampling factor (e.g. 1/1.5 logical-unit increments at 150% scale).
// Call after setting the final text position to avoid label-dependent blur.
RENUI_API void snapTextToPixelGrid(sf::Text& text);

// Snap any logical-space position to the text pixel grid.
RENUI_API sf::Vector2f snapToTextPixelGrid(const sf::Vector2f& position);

// Snap a world-space position to the active view's screen pixel grid.
// Use this for moving world overlays (nametags, floating text) so they
// stay sharp without camera-relative jitter.
RENUI_API sf::Vector2f snapToViewPixelGrid(const sf::RenderWindow& window,
                                 const sf::Vector2f& worldPosition);

// Like getLocalBounds() but accounts for the text's scale, so the
// returned rect represents the visual size in the logical view.
RENUI_API sf::FloatRect getScaledLocalBounds(const sf::Text& text);

// Measure and wrap presentational text using the active semantic text scale.
// Whitespace between words is collapsed, explicit newlines force a line, and
// tokens wider than maxWidth are split at character boundaries. maxLines == 0
// is unlimited; otherwise truncated output ends with a measured "..." that
// fits the final line. Returns no lines when maxWidth/font metrics are invalid.
RENUI_API std::vector<std::string> wrapTextLines(const std::string& text,
                                       float maxWidth,
                                       unsigned int fontSize,
                                       bool bold = false,
                                       std::size_t maxLines = 0);

// Shared highlight fill used by text selection rendering in all text inputs.
RENUI_API sf::Color getTextSelectionFillColor();

// ═══════════════════════════════════════════════════════════════════
//  COLLECTION VIEW MODE
// ═══════════════════════════════════════════════════════════════════
enum class CollectionViewMode { List, Grid };

// ═══════════════════════════════════════════════════════════════════
//  COLOUR UTILITIES
// ═══════════════════════════════════════════════════════════════════

// HSV → RGB conversion (h in [0,360), s,v in [0,1])
RENUI_API sf::Color hsvToColor(float h, float s, float v);

// Draw text with animated per-character rainbow colouring
RENUI_API void drawRainbowText(sf::RenderWindow& window, const std::string& text,
                     float x, float y, unsigned int fontSize);

// Draw a font-independent X inside rect. Useful for close, clear, and absent
// state affordances when no icon atlas is configured.
RENUI_API void drawCross(sf::RenderWindow& window, const sf::FloatRect& rect,
                         sf::Color color = getTheme().textPrimary,
                         float inset = 3.0f, float thickness = 1.5f);

// Small font- and atlas-independent glyphs for fallback affordances. The
// caller supplies semantics; RenUI only owns their consistent geometry.
enum class GeometryGlyph { Target, Eye, ChevronRight };
RENUI_API void drawGeometryGlyph(
    sf::RenderWindow& window, GeometryGlyph glyph, const sf::FloatRect& rect,
    sf::Color color = getTheme().textPrimary, float thickness = 1.25f);

// Draw the standard icon-only close control using the ui atlas slice
// "icon_close_window". Falls back to a legacy X glyph when unavailable.
// This is the shared renderer for all panel/menu/window close affordances.
RENUI_API void drawCloseWindowButton(sf::RenderWindow& window, const sf::FloatRect& rect, bool hovered);

// Optional named-atlas skin for a panel. All nine panel slices must resolve
// before the skin is drawn; otherwise callers retain their primitive fallback.
struct NineSliceCloseButtonData {
    std::string normalSlice;
    std::string hoveredSlice;
    // The reference slice is not drawn. Its width is the right inset and its
    // height is the top inset of the close artwork inside the top-right corner.
    std::string insetReferenceSlice;
    // Optional artwork shown when a panel has close chrome but is not
    // dismissible. It remains visual-only and never creates a hit target.
    std::string unavailableSlice;
};

struct NineSlicePanelData {
    std::string topLeftSlice;
    std::string topSlice;
    std::string topRightSlice;
    std::string leftSlice;
    std::string centerSlice;
    std::string rightSlice;
    std::string bottomLeftSlice;
    std::string bottomSlice;
    std::string bottomRightSlice;
    std::optional<NineSliceCloseButtonData> closeButton;
    bool closeButtonAvailable{true};
};

// Process-wide tint used by the no-tint nine-slice draw path and skinned
// Panel/DraggableWindow instances. Useful for live application opacity
// preferences without rebuilding every existing window.
RENUI_API void setNineSlicePanelTint(sf::Color tint);
RENUI_API sf::Color getNineSlicePanelTint();

// Draw a panel skin from the shared UI atlas. Corners retain their source size
// unless the destination is too small, while edges and the centre stretch.
// Returns false without drawing when any required panel slice is unavailable.
RENUI_API bool drawNineSlicePanel(sf::RenderTarget& target,
                                  const sf::FloatRect& bounds,
                                  const NineSlicePanelData& data,
                                  bool closeHovered = false);
RENUI_API bool drawNineSlicePanel(sf::RenderTarget& target,
                                  const sf::FloatRect& bounds,
                                  const NineSlicePanelData& data,
                                  bool closeHovered,
                                  sf::Color tint);

// The complete top-right corner is the stable close hit target. Returns no
// bounds when the close artwork is unavailable, disabled, or cannot resolve.
RENUI_API std::optional<sf::FloatRect> getNineSlicePanelCloseBounds(
    const sf::FloatRect& bounds, const NineSlicePanelData& data);

// Resolve a named slice from the shared ui atlas.
// Returns false when the atlas or slice is unavailable.
RENUI_API bool getUiAtlasSliceRect(const std::string& sliceName, sf::IntRect& outRect);

// Map a stable logical slice name to a style-specific atlas slice. Aliases
// are resolved at draw time, so existing widgets can switch styles live.
// Passing an empty target removes that alias.
RENUI_API void setUiAtlasSliceAlias(const std::string& logicalName,
                                    const std::string& targetName);
RENUI_API void clearUiAtlasSliceAliases();

// Optional process-wide skin used by framed UIButtons with non-empty labels.
// Icon-only and explicitly frameless buttons retain their existing renderer.
RENUI_API void setDefaultTextButtonNineSliceData(
    std::optional<NineSlicePanelData> data);
RENUI_API const NineSlicePanelData* getDefaultTextButtonNineSliceData();

// Draw a generic icon button using a named slice from the shared ui atlas.
// Falls back to a simple panel when the slice cannot be resolved.
RENUI_API void drawUiAtlasIconButton(sf::RenderWindow& window, const sf::FloatRect& rect,
                           const std::string& sliceName, bool hovered,
                           bool enabled = true, bool drawFrame = true);

// Draw a bare atlas icon (no button frame) at the given position.
// Returns the drawn width so callers can chain multiple icons/text inline.
RENUI_API float drawUiAtlasIcon(sf::RenderWindow& window, const std::string& sliceName,
                      float x, float y, float displaySize,
                      sf::Color tint = sf::Color::White);

// Draw a centered, rotatable direction indicator. Uses the configured
// "direction_indicator_grey" atlas slice when available and otherwise draws
// a vector arrow, so navigation/targeting UI remains usable without assets.
RENUI_API void drawDirectionalMarker(sf::RenderWindow& window,
                                     const sf::Vector2f& center,
                                     float displaySize,
                                     sf::Angle rotation,
                                     sf::Color tint = sf::Color::White);

// Named-slice variant for semantically distinct markers that share the same
// rotation and vector fallback contract (for example navigation vs aiming).
RENUI_API void drawDirectionalMarker(sf::RenderWindow& window,
                                     const sf::Vector2f& center,
                                     float displaySize,
                                     sf::Angle rotation,
                                     const std::string& sliceName,
                                     sf::Color tint = sf::Color::White);

// ═══════════════════════════════════════════════════════════════════
//  LIGHTWEIGHT DRAW HELPERS  (one-shot convenience renderers)
// ═══════════════════════════════════════════════════════════════════
// These wrap the common `sf::Text ... applyCrispText ... setFillColor
// ... setPosition ... draw` and `sf::RectangleShape ...` patterns that
// appear throughout application UI code. They render
// immediately and avoid the need to construct a widget object for a
// one-off label/swatch/backdrop. For persistent UI, still prefer the
// Label / Panel / ... widget classes.

// Draw a single-line text blob at the given top-left position.
// Returns the drawn bounds (post-scale) so callers can stack text.
RENUI_API sf::FloatRect drawText(sf::RenderWindow& window, const std::string& text,
                       float x, float y, unsigned int fontSize,
                       sf::Color color, bool bold = false);

// Draw a single-line text blob horizontally centred on `centerX`.
// Returns the drawn bounds.
RENUI_API sf::FloatRect drawTextCentered(sf::RenderWindow& window, const std::string& text,
                               float centerX, float y, unsigned int fontSize,
                               sf::Color color, bool bold = false);

// Like drawText but with a stroke outline (commonly used for overlay text
// where text must be legible against varying backgrounds: ping readout,
// effect stack/duration labels, ammo counter, hotbar badges).
RENUI_API sf::FloatRect drawTextOutlined(sf::RenderWindow& window, const std::string& text,
                               float x, float y, unsigned int fontSize,
                               sf::Color fill,
                               sf::Color outline = sf::Color::Black,
                               float outlineThickness = 1.0f,
                               bool bold = false);

// Draw a full-screen dim backdrop (used behind modals / popups).
// `alpha` 0..255. Temporarily uses an unscaled full-window logical view so
// scaled modal content still dims every physical pixel, then restores the
// caller's active view.
RENUI_API void drawModalBackdrop(sf::RenderWindow& window, std::uint8_t alpha = 180);

// Draw a colour-picker swatch. When `selected` is true the outline is
// highlighted (yellow, thick). Used by character customization colour
// pickers, dye selectors, etc.
RENUI_API void drawColorSwatch(sf::RenderWindow& window, const sf::Vector2f& position,
                     float size, sf::Color color, bool selected,
                     sf::Color selectedOutline = sf::Color::Yellow);

// Draw a filled + outlined rectangle in one call (the bare-bones panel
// form used for tiny bg plates behind text / inline highlights that
// don't warrant a full Panel widget).
RENUI_API void drawFilledRect(sf::RenderWindow& window, const sf::FloatRect& rect,
                    sf::Color fill,
                    sf::Color outline = sf::Color::Transparent,
                    float outlineThickness = 0.0f);

// Simple vertical layout cursor for auto-stacking UI blocks. Each call to
// place() returns the current anchor and advances by height + gap.
class RENUI_API VerticalStack {
public:
    VerticalStack(const sf::Vector2f& start, float gap = 8.0f);

    sf::Vector2f place(float itemHeight);
    void addSpacing(float extra);
    void setGap(float gap);
    float currentY() const;

private:
    sf::Vector2f cursor_;
    float gap_ = 8.0f;
};

// Bounds-aware vertical form layout. Unlike a collection of hard-coded Y
// coordinates, every placed block advances the cursor, so labels, inputs,
// button rows, hints, and status text cannot occupy the same space.
struct FormFieldLayout {
    sf::FloatRect label;
    sf::FloatRect input;
};

class RENUI_API FormLayout {
public:
    FormLayout(const sf::FloatRect& bounds, float gap = 8.0f);

    sf::FloatRect place(float height, float gapAfter = -1.0f);
    FormFieldLayout placeField(float labelHeight = 16.0f,
                               float inputHeight = 28.0f,
                               float labelToInputGap = 2.0f,
                               float gapAfter = -1.0f);
    std::vector<sf::FloatRect> placeRow(const std::vector<float>& widths,
                                       float height,
                                       float itemGap = 10.0f,
                                       float gapAfter = -1.0f);
    void addSpacing(float extra);
    float currentY() const;
    float remainingHeight() const;
    const sf::FloatRect& bounds() const;

private:
    sf::FloatRect bounds_;
    float cursorY_ = 0.0f;
    float gap_ = 8.0f;
};

// Places intrinsically-sized controls left-to-right without shrinking them.
// A control moves to the next row when it no longer fits in availableWidth;
// this preserves text-fit guarantees made by widgets such as UIButton.
struct FlowLayoutResult {
    std::vector<sf::FloatRect> items;
    float height = 0.0f;
};

RENUI_API FlowLayoutResult layoutFlowItems(
    const sf::Vector2f& start,
    float availableWidth,
    const std::vector<sf::Vector2f>& itemSizes,
    float itemGap = 4.0f,
    float rowGap = 4.0f);

// Interpolates a vertical extent between two values. UI containers can use
// value() as their visible height and clip their content to it.
class RENUI_API VerticalReveal {
public:
    explicit VerticalReveal(float value = 0.0f);

    void snapTo(float value);
    void setTarget(float target, float durationSeconds = 0.3f);

    float value() const;
    float target() const { return target_; }
    bool isAnimating() const;

private:
    float start_ = 0.0f;
    float target_ = 0.0f;
    float durationSeconds_ = 0.3f;
    std::chrono::steady_clock::time_point startedAt_{};
    bool animating_ = false;
};

// The reveal interpolator operates on a scalar extent. This semantic alias
// makes horizontal rollouts readable without duplicating animation state or
// changing the established VerticalReveal API.
using HorizontalReveal = VerticalReveal;

// ═══════════════════════════════════════════════════════════════════
//  LABEL
// ═══════════════════════════════════════════════════════════════════

class RENUI_API Label {
public:
    Label();
    Label(const std::string& str, unsigned int fontSize,
          sf::Color color = getTheme().textPrimary,
          bool bold = false);

    void setText(const std::string& str);
    void setFontSize(unsigned int size);
    void setColor(sf::Color color);
    void setPosition(float x, float y);
    void setPosition(const sf::Vector2f& pos);
    sf::Vector2f getPosition() const;
    sf::FloatRect getBounds() const;
    void setOutline(sf::Color color, float thickness = 1.0f);
    void setUnderlined(bool underlined);
    bool isUnderlined() const;
    void draw(sf::RenderWindow& window) const;

    const sf::Text& getText() const;

private:
    mutable std::optional<sf::Text> text_;
    mutable std::uint64_t textInitializationRevision_{0};
    std::string value_;
    unsigned int fontSize_ = 12;
    sf::Color color_{sf::Color::White};
    sf::Vector2f position_{};
    sf::Color outlineColor_{sf::Color::Transparent};
    float outlineThickness_{0.f};
    bool bold_{false};
    bool underlined_{false};
    bool ensureText_() const;
};

// ═══════════════════════════════════════════════════════════════════
//  PANEL  (rectangle with optional border)
// ═══════════════════════════════════════════════════════════════════

class RENUI_API Panel {
public:
    Panel();
    Panel(const sf::Vector2f& position, const sf::Vector2f& size,
          sf::Color fillColor = getTheme().panelBackground,
          sf::Color outlineColor = getTheme().panelBorder,
            float outlineThickness = 2.0f);
        Panel(const sf::Vector2f& position, const sf::Vector2f& size,
            sf::Color fillColor, sf::Color outlineColor,
            float outlineThickness,
            std::optional<NineSlicePanelData> nineSliceData);
    Panel(const sf::Vector2f& position, const sf::Vector2f& size,
            const NineSlicePanelData& nineSliceData);

    void setPosition(float x, float y);
    void setPosition(const sf::Vector2f& pos);
    sf::Vector2f getPosition() const;
    void setSize(const sf::Vector2f& size);
    sf::Vector2f getSize() const;
    void setFillColor(sf::Color color);
    void setOutlineColor(sf::Color color);
    void setOutlineThickness(float t);
    void setNineSliceData(std::optional<NineSlicePanelData> data);
    const std::optional<NineSlicePanelData>& getNineSliceData() const;
    void setCloseHovered(bool hovered);
    std::optional<sf::FloatRect> getCloseButtonBounds() const;
    bool contains(const sf::Vector2f& point) const;
    sf::FloatRect getGlobalBounds() const;
    void draw(sf::RenderWindow& window) const;

private:
    sf::RectangleShape shape_;
    std::optional<NineSlicePanelData> nineSliceData_;
    bool closeHovered_ = false;
};

// ═══════════════════════════════════════════════════════════════════
//  ICON SPRITE  (single frame from a sprite sheet)
// ═══════════════════════════════════════════════════════════════════

class RENUI_API IconSprite {
public:
    IconSprite();
    void setTexture(const sf::Texture& texture, const sf::IntRect& rect);
    void setPosition(float x, float y);
    void setScale(float scaleX, float scaleY);
    void draw(sf::RenderWindow& window) const;
    sf::FloatRect getGlobalBounds() const;

private:
    std::optional<sf::Sprite> sprite_;
    bool hasTexture_ = false;
};

// ═══════════════════════════════════════════════════════════════════
//  BUTTON  (clickable rectangle with centered label)
// ═══════════════════════════════════════════════════════════════════

enum class UIButtonStyle {
    Default,
    HoverGlowBlue,
    PanelOutline,
    Primary,
    Danger
};

class RENUI_API UIButton {
public:
    UIButton();
    UIButton(const std::string& label, const sf::Vector2f& position, const sf::Vector2f& size,
             unsigned int fontSize = 12);

    void draw(sf::RenderWindow& window) const;
    bool isClicked(const sf::Vector2i& mousePosition) const;
    bool contains(const sf::Vector2f& point) const;
    void updateHover(const sf::Vector2i& mousePosition);
    bool isHovered() const;
    void setHovered(bool hovered);
    void setEnabled(bool enabled);
    bool isEnabled() const;

    void setFocused(bool f);
    bool isFocused() const;
    void setSelected(bool s);
    bool isSelected() const;
    void setStyle(UIButtonStyle style);
    UIButtonStyle getStyle() const;
    void setPrimary(bool primary);
    bool isPrimary() const;
    void setFrameVisible(bool visible);
    bool isFrameVisible() const;
    void setHoverGlowWidthScale(float scale);
    float getHoverGlowWidthScale() const;
    void setLabel(const std::string& label);
    void setPosition(const sf::Vector2f& pos);
    sf::Vector2f getPosition() const;
    void setSize(const sf::Vector2f& size);
    sf::Vector2f getSize() const;
    void setAutoExpandToFitLabel(bool enabled,
                                 const sf::Vector2f& padding = {16.0f, 8.0f});
    bool getAutoExpandToFitLabel() const;

    void setFillColor(sf::Color color);
    void setOutlineColor(sf::Color color);
    void setTextColor(sf::Color color);

private:
    mutable sf::RectangleShape shape_;
    mutable std::optional<sf::Text> text_;
    mutable std::uint64_t textInitializationRevision_{0};
    std::string label_;
    unsigned int fontSize_ = 12;
    sf::Vector2f minimumSize_{0.0f, 0.0f};
    sf::Vector2f labelPadding_{16.0f, 8.0f};
    bool autoExpandToFitLabel_ = false;
    sf::Color textColor_ = sf::Color::White;
    bool focused_ = false;
    bool selected_ = false;
    bool hovered_ = false;
    bool enabled_ = true;
    UIButtonStyle style_ = UIButtonStyle::Default;
    bool frameVisible_ = true;
    bool customFillColor_ = false;
    bool customOutlineColor_ = false;
    float hoverGlowWidthScale_ = 1.0f;
    mutable float hoverAnim_ = 0.0f;
    mutable bool hoverAnimInitialized_ = false;
    mutable float hoverAnimSampleTime_ = 0.0f;
    mutable bool textLayoutDirty_ = true;
    mutable std::uint64_t textLayoutMetricsRevision_ = 0;
    mutable float textLayoutUIScaleFactor_ = -1.0f;
    mutable float textLayoutRenderScaleFactor_ = -1.0f;
    bool ensureText_() const;
    void refreshTextLayout_() const;
    void invalidateTextLayout_() const;
};

// ═══════════════════════════════════════════════════════════════════
//  CHECKBOX  (toggleable square with a label to its right)
// ═══════════════════════════════════════════════════════════════════
// Lightweight boolean toggle used by tool panels (e.g. "override
// restrictions" flags). Click the box or the label to flip the state.

class RENUI_API Checkbox {
public:
    Checkbox();
    Checkbox(const std::string& label, const sf::Vector2f& position,
             float boxSize = 20.0f, unsigned int fontSize = 12);

    void draw(sf::RenderWindow& window) const;

    // Toggles the checked state if the point hits the box or label.
    // Returns true when the state changed.
    bool handleClick(const sf::Vector2i& mousePosition);
    bool contains(const sf::Vector2f& point) const;

    void setChecked(bool c);
    bool isChecked() const;
    void setFocused(bool f);
    void setLabel(const std::string& label);
    void setPosition(const sf::Vector2f& pos);
    sf::Vector2f getPosition() const;
    sf::FloatRect getBounds() const; // box + label combined

private:
    sf::RectangleShape box_;
    mutable std::optional<sf::Text> label_;
    mutable std::uint64_t labelInitializationRevision_{0};
    std::string labelText_;
    bool checked_ = false;
    bool focused_ = false;
    float boxSize_ = 20.0f;
    unsigned int fontSize_ = 12;
    bool ensureLabel_() const;
    void positionLabel() const;
};

// ════════════════════════════════════════════════════════════════════════════════
//  SLIDER  (continuous or stepped horizontal value selector)
// ═══════════════════════════════════════════════════════════════════════════════

class RENUI_API Slider {
public:
    Slider();
    Slider(const sf::Vector2f& position, const sf::Vector2f& size,
           float minimum, float maximum, float value, float step = 0.0f);

    void draw(sf::RenderWindow& window) const;

    // Pointer coordinates must already be mapped through the same UI view
    // used to draw the slider. A zero step means continuous; positive steps
    // snap relative to the minimum value.
    bool handlePointerPress(const sf::Vector2f& pointer);
    bool handlePointerMove(const sf::Vector2f& pointer);
    bool handlePointerRelease(const sf::Vector2f& pointer);

    void setPosition(const sf::Vector2f& position);
    sf::Vector2f getPosition() const;
    void setSize(const sf::Vector2f& size);
    sf::Vector2f getSize() const;
    sf::FloatRect getBounds() const;

    void setRange(float minimum, float maximum);
    float getMinimum() const;
    float getMaximum() const;
    void setStep(float step);
    float getStep() const;
    void setValue(float value);
    float getValue() const;
    float getNormalizedValue() const;
    // Small stepped ranges render one tick at every selectable stop. Dense
    // ranges and continuous sliders return zero to keep the track uncluttered.
    std::size_t getVisibleTickCount() const;

    bool isHovered() const;
    bool isDragging() const;

private:
    sf::Vector2f position_{0.0f, 0.0f};
    sf::Vector2f size_{180.0f, 24.0f};
    float minimum_ = 0.0f;
    float maximum_ = 1.0f;
    float value_ = 0.0f;
    float step_ = 0.0f;
    bool hovered_ = false;
    bool dragging_ = false;

    float snapAndClamp_(float value) const;
    void updateFromPointer_(const sf::Vector2f& pointer);
};

// ═══════════════════════════════════════════════════════════════════
//  TEXT INPUT  (single-line editable field)
// ═══════════════════════════════════════════════════════════════════

class RENUI_API UITextInput {
public:
    UITextInput();
    UITextInput(const sf::Vector2f& position, const sf::Vector2f& size, bool passwordMode = false);

    void draw(sf::RenderWindow& window) const;
    void handleEvent(const sf::Event& event);
    // Use when the host renders through a scaled/anchored view. Mouse press
    // and selection-drag coordinates use mappedPointer; keyboard/text event
    // behavior is identical to the compatibility one-argument overload.
    void handleEvent(const sf::Event& event, const sf::Vector2f& mappedPointer);
    void setActive(bool a);
    bool isActive() const;
    std::string getText() const;
    void setText(const std::string& text);
    // Inserts atomically at the caret, replacing the current selection.
    // Returns false when the complete value would exceed maxLength.
    bool insertText(std::string_view text);
    void clear();
    void setPlaceholder(const std::string& p);
    // A value of zero keeps input length unlimited. Lowering the limit trims
    // existing content and moves the caret to the new end.
    void setMaxLength(std::size_t maxLength);
    std::size_t getMaxLength() const;
    void setFontSize(unsigned int fontSize);
    unsigned int getFontSize() const;
    void setPasswordMode(bool passwordMode);
    bool isPasswordMode() const;
    void setPosition(const sf::Vector2f& pos);
    void setSize(const sf::Vector2f& size);
    sf::FloatRect getBounds() const;
    bool contains(const sf::Vector2f& point) const;
    void updateHover(const sf::Vector2i& mousePosition);

private:
    sf::RectangleShape box_;
    bool active_ = false;
    bool passwordMode_ = false;
    bool sensitive_ = false;
    std::string content_;
    std::string placeholder_;
    std::size_t maxLength_ = 0;
    unsigned int fontSize_ = 14;
    std::size_t cursorPos_ = 0;
    std::size_t selectionStart_ = 0;
    std::size_t selectionEnd_ = 0;
    std::optional<std::size_t> selectionAnchor_;
    bool mouseSelecting_ = false;
    bool hovered_ = false;
    mutable sf::Clock caretBlinkClock_;
    mutable bool caretVisible_ = true;
    mutable float horizontalScroll_ = 0.0f;

    bool hasSelection_() const;
    void clearSelection_();
    std::size_t selectionLeft_() const;
    std::size_t selectionRight_() const;
    void eraseSelection_();
    void resetCaretBlink_();
    sf::FloatRect getInnerBounds_() const;
    sf::Vector2f getTextPosition_(const std::string& displayText) const;
    void ensureCaretVisible_() const;
    float charPosX_(std::size_t index, const std::string& displayText) const;
    std::size_t indexFromMouseX_(float mouseX) const;
    void moveCursorHorizontal_(int delta, bool extendSelection);
    void updateTextPosition_();
};

// ═══════════════════════════════════════════════════════════════════
//  TEXT AREA  (multi-line editable field with word-wrap + auto-grow)
// ═══════════════════════════════════════════════════════════════════
// Used for multi-line prose fields such as descriptions and dialogue.
// Behaviour:
//   • Automatic word-wrap inside the current width (keeps whole words
//     together when possible; long individual tokens break at character
//     boundaries).
//   • Height auto-grows with content up to maxHeight_ (if set) after
//     which it switches to internal scrolling. Call getHeight() each
//     frame to get the current rendered height and feed that into the
//     parent layout so sibling widgets can be pushed down.
//   • Mouse + keyboard selection with the same light-blue highlight used
//     by every other editable text widget in the project.
//   • Explicit newlines via Enter. Clipboard (Ctrl+C/X/V), Ctrl+A,
//     Home/End (line + shift-select), Up/Down arrow navigation.

class RENUI_API UITextArea {
public:
    UITextArea();
    UITextArea(const sf::Vector2f& position, float width,
               float minHeight = 48.0f, float maxHeight = 0.0f,
               unsigned int fontSize = 12);

    // Rendering
    void draw(sf::RenderWindow& window) const;

    // Events — returns true if the event was handled (consumed by the area).
    bool handleEvent(const sf::Event& event);
    // Use when the host renders through a scaled/anchored view. Mouse press
    // and selection-drag coordinates use mappedPointer; keyboard/text event
    // behavior is identical to the compatibility one-argument overload.
    bool handleEvent(const sf::Event& event, const sf::Vector2f& mappedPointer);

    // Content
    std::string getText() const { return content_; }
    void setText(const std::string& text);
    void clear();
    void setPlaceholder(const std::string& p) { placeholder_ = p; }

    // Layout
    void setPosition(const sf::Vector2f& pos);
    sf::Vector2f getPosition() const { return position_; }
    void setWidth(float w);
    float getWidth() const { return width_; }
    void setMinHeight(float h);
    void setMaxHeight(float h);  // 0 = unlimited (pure auto-grow)
    void setFontSize(unsigned int size);

    // Current rendered height (auto-computed from wrapped line count,
    // clamped to [minHeight_, maxHeight_] — when maxHeight_ is 0 there
    // is no upper clamp). Parent layouts consult this every frame to
    // reflow siblings.
    float getHeight() const;
    sf::FloatRect getBounds() const;
    bool contains(const sf::Vector2f& point) const;

    // Focus
    void setActive(bool a);
    bool isActive() const { return active_; }

private:
    sf::Vector2f position_{0, 0};
    float width_ = 200.0f;
    float minHeight_ = 48.0f;
    float maxHeight_ = 0.0f;  // 0 = unlimited
    unsigned int fontSize_ = 12;

    std::string content_;
    std::string placeholder_;

    bool active_ = false;

    // Selection / caret
    std::size_t cursorPos_ = 0;
    std::size_t selectionStart_ = 0;
    std::size_t selectionEnd_ = 0;
    std::optional<std::size_t> selectionAnchor_;
    bool mouseSelecting_ = false;
    mutable sf::Clock caretBlinkClock_;
    mutable bool caretVisible_ = true;

    // Wrapped layout cache (rebuilt whenever content/width/font changes).
    struct WrappedLine {
        std::size_t startByte = 0;  // index into content_
        std::size_t endByte = 0;    // exclusive
        bool hardBreak = false;     // ends in '\n'
    };
    mutable std::vector<WrappedLine> lines_;
    mutable float lineHeight_ = 16.0f;
    mutable float currentHeight_ = 48.0f;
    mutable bool layoutDirty_ = true;
    mutable std::uint64_t textMetricsRevision_ = 0;

    // Scroll offset (in pixels, 0..maxScroll). Only used when
    // maxHeight_ > 0 and content overflows. `mutable` because
    // rebuildLayout_() runs inside draw() (const) and needs to clamp.
    mutable float scrollOffset_ = 0.0f;

    bool hasSelection_() const { return selectionStart_ != selectionEnd_; }
    std::size_t selectionLeft_() const { return std::min(selectionStart_, selectionEnd_); }
    std::size_t selectionRight_() const { return std::max(selectionStart_, selectionEnd_); }
    void clearSelection_();
    void eraseSelection_();
    void resetCaretBlink_();
    bool layoutNeedsRebuild_() const;
    void rebuildLayout_() const;
    void markDirty_() { layoutDirty_ = true; }
    std::size_t indexFromMouse_(const sf::Vector2f& mouse) const;
    sf::Vector2f caretPixel_(std::size_t index) const;  // relative to position_
    std::size_t lineIndexOf_(std::size_t byte) const;
    void moveCursorVertical_(int dy, bool extendSelection);
    void moveCursorHorizontal_(int dx, bool extendSelection);
    void ensureCaretVisible_();
    float maxScroll_() const;
};

// ═══════════════════════════════════════════════════════════════════
//  PROGRESS BAR  (generic fill bar with optional text overlay)
// ═══════════════════════════════════════════════════════════════════

class RENUI_API ProgressBar {
public:
    ProgressBar();
    ProgressBar(const sf::Vector2f& position, const sf::Vector2f& size,
                sf::Color bgColor, sf::Color fillColor,
                sf::Color textColor = getTheme().textPrimary,
                unsigned int fontSize = 10);

    void setValue(float current, float max);
    float getValue() const { return current_; }
    float getMaximum() const { return max_; }
    float getNormalizedValue() const;
    void setLabel(const std::string& label); // Override auto-text with custom label
    void setPosition(const sf::Vector2f& pos);
    sf::Vector2f getPosition() const;
    void draw(sf::RenderWindow& window) const;

private:
    sf::RectangleShape background_;
    sf::RectangleShape fill_;
    mutable std::optional<sf::Text> text_;
    mutable std::uint64_t textInitializationRevision_{0};
    unsigned int fontSize_ = 10;
    sf::Color textColor_{sf::Color::White};
    std::string label_;
    float current_ = 0.0f;
    float max_ = 1.0f;
    bool customLabel_ = false;
    bool ensureText_() const;
    void updateFill();
    void updateTextPosition() const;
};

// Concise aliases for common generic controls.
using Button = UIButton;
using TextInput = UITextInput;

class RENUI_API Dropdown {
public:
    Dropdown(const sf::Vector2f& position, const sf::Vector2f& size,
             const std::vector<std::string>& options, int selectedIndex = 0);

    void draw(sf::RenderWindow& window) const;
    bool handleClick(const sf::Vector2i& mousePosition);
    void updateHover(const sf::Vector2i& mousePosition);
    void close();
    bool isOpen() const { return open_; }
    int getSelectedIndex() const { return selectedIndex_; }
    const std::string& getSelectedText() const;
    void setSelectedIndex(int index);
    void setPosition(const sf::Vector2f& position) { position_ = position; }

private:
    sf::Vector2f position_;
    sf::Vector2f size_;
    std::vector<std::string> options_;
    int selectedIndex_{-1};
    bool open_{false};
    int hoveredOption_{-1};
    bool headerHovered_{false};
};

class RENUI_API ToggleSwitch {
public:
    ToggleSwitch(const std::string& label, const sf::Vector2f& position,
                 bool initialState = true);

    void draw(sf::RenderWindow& window) const;
    bool handleClick(const sf::Vector2i& mousePosition);
    void updateHover(const sf::Vector2i& mousePosition);
    bool isEnabled() const { return enabled_; }
    void setEnabled(bool enabled) { enabled_ = enabled; }
    void setPosition(const sf::Vector2f& position) { position_ = position; }
    sf::Vector2f getPosition() const { return position_; }

private:
    sf::Vector2f position_;
    std::string label_;
    bool enabled_{true};
    bool hovered_{false};
    static constexpr float SwitchWidth = 16.f;
    static constexpr float SwitchHeight = 16.f;
    static constexpr float LabelGap = 8.f;
    sf::FloatRect bounds_() const;
};

// ═══════════════════════════════════════════════════════════════════
//  TOOLTIP  (auto-sizing popup with multi-line colored text)
// ═══════════════════════════════════════════════════════════════════

struct TooltipLine {
    std::string text;
    sf::Color color;
    bool isTitle = false;   // Larger font
    bool isRainbow = false; // Animated per-character colour emphasis
};

class RENUI_API Tooltip {
public:
    Tooltip();

    void setLines(const std::vector<TooltipLine>& lines);
    void setPosition(const sf::Vector2f& mousePos); // Auto-clamps to the visible active view
    // Override the viewport the popup is clamped inside. Pass the host
    // window size when rendering in a host window smaller than the configured
    // logical canvas. A zero component falls back to the configured logical
    // dimension for that axis.
    void setClampSize(const sf::Vector2f& size);
    void draw(sf::RenderWindow& window) const;
    // Logical size of the auto-sized panel for the current lines. This
    // includes the tooltip padding and text-snap guard used by draw().
    sf::Vector2f getPanelSize() const;
    bool hasContent() const { return !lines_.empty(); }
    void clear();

private:
    std::vector<TooltipLine> lines_;
    sf::Vector2f position_;
    sf::Vector2f clampSize_{0.f, 0.f};
};

// ═══════════════════════════════════════════════════════════════════
//  DRAGGABLE WINDOW  (titled panel with close button, draggable header)
// ═══════════════════════════════════════════════════════════════════
// Subclass or compose this to build movable application windows.

class RENUI_API DraggableWindow {
public:
    DraggableWindow();
    DraggableWindow(const std::string& title, const sf::Vector2f& defaultPos,
                    const sf::Vector2f& size, float headerHeight = 0.0f);

    // Toggle open/close
    void toggle();
    void open();
    void close();
    bool isOpen() const { return open_; }

    // Event handling (drag, close button). Returns true if event was consumed.
    bool handleEvent(const sf::Event& event, const sf::RenderWindow& window);
    // Use when the host already mapped the pointer through the UI view used
    // for rendering. Pointer press, drag, and wheel routing use mappedPointer.
    bool handleEvent(const sf::Event& event, const sf::Vector2f& mappedPointer);

    // Draw the window chrome (panel + title + close button).
    // Call this first, then draw custom content inside the content area.
    void drawChrome(sf::RenderWindow& window);

    // Opt into atlas-backed panel chrome. Missing resources fall back to the
    // existing primitive panel and standard close control.
    void setNineSliceData(std::optional<NineSlicePanelData> data);
    const std::optional<NineSlicePanelData>& getNineSliceData() const;

    // Getters for content area (excluding header)
    sf::Vector2f getContentPosition() const;
    sf::Vector2f getContentSize() const;
    sf::FloatRect getPanelBounds() const;
    sf::Vector2f getPanelPosition() const;
    sf::Vector2f getPanelSize() const;
    // Resize around the panel's current center and keep the result inside the
    // authored logical screen. Before first open, the default position is
    // adjusted equivalently so responsive sizing does not shift the panel.
    void setSize(const sf::Vector2f& size);
    // Resize while preserving the current top-left corner. If the enlarged
    // panel would leave the visible UI region, only the minimum clamping
    // needed to keep it visible is applied. Useful for edge-origin rollouts.
    void setSizeKeepingTopLeft(const sf::Vector2f& size);

    // Layer priority is assigned when opened and raised whenever the panel
    // receives focus. Callers sort open panels by this value for rendering
    // (ascending) and event routing (descending).
    std::uint64_t getLayeringPriority() const { return layeringPriority_; }
    void bringToFront();

    // Scrolling support
    float getScrollOffset() const { return scrollOffset_; }
    void setScrollOffset(float offset) { scrollOffset_ = offset; }
    void setMaxScroll(float maxScroll) { maxScroll_ = maxScroll; }
    bool handleScroll(float delta, const sf::Vector2f& mousePos);

    void setTitle(const std::string& title);

    // Rendering and hit-testing use the same scaled UI view. Center is the
    // compatibility default; edge-docked panels should select the matching
    // anchor before drawing or routing events.
    void setUIAnchor(UIAnchor anchor);
    UIAnchor getUIAnchor() const { return uiAnchor_; }

    // Render-view-corrected logical pointer-position override.
    void setLogicalMousePos(sf::Vector2i pos) { logicalMouseOverride_ = pos; }

protected:
    sf::Vector2i logicalMouseOverride_{-1, -1};
    bool open_ = false;
    bool positionInitialized_ = false;
    float panelX_ = 0.0f;
    float panelY_ = 0.0f;
    float panelWidth_ = 0.0f;
    float panelHeight_ = 0.0f;
    float headerHeight_ = 0.0f;
    sf::Vector2f defaultPos_;
    std::string title_;
    UIAnchor uiAnchor_ = UIAnchor::Center;

    // Scroll
    float scrollOffset_ = 0.0f;
    float maxScroll_ = 0.0f;

    // Drag state
    bool dragging_ = false;
    sf::Vector2f dragOffset_;

    std::uint64_t layeringPriority_ = 0;
    std::optional<NineSlicePanelData> nineSliceData_;

    void clampToVisibleBounds_();
    sf::FloatRect getCloseButtonBounds_() const;
    bool handleEventImpl_(const sf::Event& event,
                          const sf::Vector2f& mappedPointer);

    // Close button size
    static constexpr float CLOSE_BTN_SIZE = 24.0f;
};

// ═══════════════════════════════════════════════════════════════════
//  SCROLLABLE LIST  (renders items with scroll clipping)
// ═══════════════════════════════════════════════════════════════════

class RENUI_API ScrollableList {
public:
    ScrollableList();
    ScrollableList(const sf::Vector2f& position, const sf::Vector2f& size, float rowHeight);

    void setItemCount(int count);
    int getItemCount() const { return itemCount_; }
    float getRowHeight() const { return rowHeight_; }
    void setPosition(const sf::Vector2f& pos);
    void setSize(const sf::Vector2f& size);
    sf::Vector2f getPosition() const;
    sf::Vector2f getSize() const;

    // Scroll handling. Returns true if consumed.
    bool handleScroll(float delta, const sf::Vector2f& mousePos);
    float getScrollOffset() const { return scrollOffset_; }
    void setScrollOffset(float offset) { scrollOffset_ = std::max(0.0f, std::min(offset, maxScroll_)); }

    // Returns index of first visible row and last visible row
    int getFirstVisibleRow() const;
    int getLastVisibleRow() const;

    // Get Y position for row index (accounting for scroll)
    float getRowY(int index) const;

    // Is a given row Y within visible bounds?
    bool isRowVisible(float rowY) const;

    // Draw scroll bar indicator
    void drawScrollBar(sf::RenderWindow& window) const;

    // Handle scroll bar click+drag events. Returns true if consumed.
    bool handleScrollBarEvent(const sf::Event& event, const sf::Vector2f& mousePos);

    // Check if a point is inside the list area
    bool contains(const sf::Vector2f& point) const;

    // Get hovered row index (-1 if none)
    int getHoveredRow(const sf::Vector2f& mousePos) const;

private:
    sf::Vector2f position_;
    sf::Vector2f size_;
    float rowHeight_ = 40.0f;
    int itemCount_ = 0;
    float scrollOffset_ = 0.0f;
    float maxScroll_ = 0.0f;
    mutable bool draggingScrollBar_ = false;
    float scrollBarGrabOffset_ = 0.0f;
    static constexpr float SCROLLBAR_WIDTH = 8.0f;
    void recalcMaxScroll();
};

// ═══════════════════════════════════════════════════════════════════
//  VALUE BREAKDOWN ROW  (label plus primary, secondary, modifier, and total)
// ═══════════════════════════════════════════════════════════════════

class RENUI_API ValueBreakdownRow {
public:
    ValueBreakdownRow();
    ValueBreakdownRow(const std::string& name, const sf::Vector2f& position);

    void setValues(int primary, int secondary = 0, int modifier = 0);
    void setValuesFloat(float primary, float secondary = 0.0f,
                        float modifier = 0.0f,
                        const std::string& suffix = "");
    void setNegativeModifierIsBad(bool isBad);
    // Configure value columns relative to the row origin. A positive
    // valueRowOffset places the breakdown on a second line so long semantic-
    // scaled names cannot collide with numeric columns.
    void setLayout(float primaryOffset, float secondaryOffset,
                   float modifierOffset,
                   float totalOffset, float valueRowOffset = 0.0f);
    void setPosition(float x, float y);
    sf::Vector2f getPosition() const { return {x_, y_}; }
    // Return the interactive row region supplied by the host layout. This
    // keeps hover behavior aligned with the entire table row, not only the
    // visible glyph bounds of one label.
    sf::FloatRect getRowBounds(float width, float height) const;
    void draw(sf::RenderWindow& window) const;

private:
    std::string name_;
    Label nameLabel_;
    Label primaryLabel_;
    Label secondaryLabel_;
    Label modifierLabel_;
    Label totalLabel_;
    float x_ = 0.0f;
    float y_ = 0.0f;
    float primaryOffset_ = 0.0f;
    float secondaryOffset_ = 0.0f;
    float modifierOffset_ = 0.0f;
    float totalOffset_ = 0.0f;
    float valueRowOffset_ = 0.0f;
    bool negativeModifierIsBad_ = true;
    enum class CachedValueKind : std::uint8_t {
        None,
        Integer,
        Floating
    };
    CachedValueKind cachedValueKind_ = CachedValueKind::None;
    int cachedPrimaryInt_ = 0;
    int cachedSecondaryInt_ = 0;
    int cachedModifierInt_ = 0;
    float cachedPrimaryFloat_ = 0.0f;
    float cachedSecondaryFloat_ = 0.0f;
    float cachedModifierFloat_ = 0.0f;
    std::string cachedSuffix_;
    float positionedUIScaleFactor_ = -1.0f;
    float positionedRenderScaleFactor_ = -1.0f;
    bool positionsMatchCurrentScale_() const;
    void positionLabels_();
};

// ═══════════════════════════════════════════════════════════════════
//  SEPARATOR  (horizontal line)
// ═══════════════════════════════════════════════════════════════════

class RENUI_API Separator {
public:
    Separator();
    Separator(const sf::Vector2f& position, float width,
              sf::Color color = getTheme().divider);

    void setPosition(float x, float y);
    void setWidth(float w);
    void draw(sf::RenderWindow& window) const;

private:
    sf::RectangleShape line_;
};

// ═══════════════════════════════════════════════════════════════════
//  APPLICATION-SPECIFIC EXTENSIONS BELONG OUTSIDE THE CORE LIBRARY
// ═══════════════════════════════════════════════════════════════════
} // namespace RenUI
