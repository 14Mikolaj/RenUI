// Widgets.cpp
// Implementation of the simple presentational widgets:
//   Label, Panel, IconSprite, UIButton, ProgressBar, Separator,
//   ValueBreakdownRow.
// (UITextInput lives in TextInput.cpp; container widgets such as Tooltip,
// DraggableWindow, and ScrollableList live in Window.cpp.)

#include <RenUI/RenUI.hpp>
#include <cstdio>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace RenUI {

namespace {

sf::Shader* getHoverGlowShader() {
    static sf::Shader shader;
    static std::uint64_t attemptedRevision = 0;
    static bool loaded = false;

    const auto revision = getInitializationRevision();
    if (attemptedRevision != revision) {
        attemptedRevision = revision;
        loaded = false;
        const Config config = getConfig();
        if (config.enableShaders && !config.hoverShaderPath.empty() &&
            sf::Shader::isAvailable()) {
            auto source = readResourceText(config.hoverShaderPath);
            loaded = source && shader.loadFromMemory(*source, sf::Shader::Type::Fragment);
            if (source && !loaded) {
                reportDiagnostic({DiagnosticSeverity::Warning, DiagnosticCode::InvalidResource,
                                  "Hover shader could not be compiled; using a solid fallback",
                                  config.hoverShaderPath});
            }
        }
    }

    return loaded ? &shader : nullptr;
}

float getHoverGlowTimeSeconds() {
    static sf::Clock clock;
    return clock.getElapsedTime().asSeconds();
}

void drawHoverGlowBackdrop(sf::RenderWindow& window,
                           const sf::Vector2f& position,
                           const sf::Vector2f& size,
                           float glowStrength,
                           float glowProgress,
                           float widthScale,
                           float minimumVisualWidth) {
    if (glowStrength <= 0.0f || glowProgress <= 0.0f) return;

    sf::Shader* shader = getHoverGlowShader();
    if (!shader) return;

    widthScale = std::max(0.0f, widthScale);
    const float visualWidth = std::max(size.x * widthScale,
                                       std::max(0.0f, minimumVisualWidth));
    const float effectiveWidthScale = visualWidth / std::max(1.0f, size.x);
    const sf::Vector2f visualPosition(
        position.x + (size.x - visualWidth) * 0.5f,
        position.y);
    const sf::Vector2f visualSize(visualWidth, size.y);
    const sf::Vector2f pad(80.0f * effectiveWidthScale, 46.0f);
    sf::RectangleShape glowRect;
    glowRect.setPosition({visualPosition.x - pad.x, visualPosition.y - pad.y});
    glowRect.setSize({visualSize.x + pad.x * 2.0f, visualSize.y + pad.y * 2.0f});
    glowRect.setFillColor(sf::Color::White);

    const sf::Vector2u windowSize = window.getSize();
    const sf::Vector2f logicalTL = glowRect.getPosition();
    const sf::Vector2f logicalBR = logicalTL + glowRect.getSize();
    const sf::Vector2i pixTL = window.mapCoordsToPixel(logicalTL);
    const sf::Vector2i pixBR = window.mapCoordsToPixel(logicalBR);
    const float left = static_cast<float>(std::min(pixTL.x, pixBR.x));
    const float right = static_cast<float>(std::max(pixTL.x, pixBR.x));
    const float top = static_cast<float>(std::min(pixTL.y, pixBR.y));
    const float bottom = static_cast<float>(std::max(pixTL.y, pixBR.y));
    const sf::Vector2f glowPos(left, top);
    const sf::Vector2f glowSize(
        std::max(1.0f, right - left),
        std::max(1.0f, bottom - top));

    shader->setUniform("u_resolution",
        sf::Glsl::Vec2(static_cast<float>(windowSize.x), static_cast<float>(windowSize.y)));
    shader->setUniform("u_rectPos", sf::Glsl::Vec2(glowPos.x, glowPos.y));
    shader->setUniform("u_rectSize", sf::Glsl::Vec2(glowSize.x, glowSize.y));
    shader->setUniform("u_intensity", std::clamp(glowStrength, 0.0f, 1.0f));
    shader->setUniform("u_progress", std::clamp(glowProgress, 0.0f, 1.0f));
    shader->setUniform("u_time", getHoverGlowTimeSeconds());

    sf::RenderStates glowStates;
    glowStates.shader = shader;
    glowStates.blendMode = sf::BlendAdd;
    window.draw(glowRect, glowStates);
}

sf::Color lerpColor(const sf::Color& from, const sf::Color& to, float t) {
    const float clamped = std::clamp(t, 0.0f, 1.0f);
    auto lerpChannel = [clamped](std::uint8_t a, std::uint8_t b) {
        return static_cast<std::uint8_t>(
            static_cast<float>(a) +
            (static_cast<float>(b) - static_cast<float>(a)) * clamped);
    };
    return sf::Color(
        lerpChannel(from.r, to.r),
        lerpChannel(from.g, to.g),
        lerpChannel(from.b, to.b),
        lerpChannel(from.a, to.a));
}

bool sameCachedFloatValue(float lhs, float rhs) {
    if (lhs != rhs) return false;
    // Preserve the formatted distinction between 0.0 and -0.0. NaNs compare
    // unequal and therefore remain on the conservative refresh path.
    return lhs != 0.0f || std::signbit(lhs) == std::signbit(rhs);
}

} // namespace

VerticalReveal::VerticalReveal(float value) {
    snapTo(value);
}

void VerticalReveal::snapTo(float value) {
    start_ = std::max(0.0f, value);
    target_ = start_;
    animating_ = false;
}

void VerticalReveal::setTarget(float target, float durationSeconds) {
    target = std::max(0.0f, target);
    const float current = value();
    if (std::abs(target - current) <= 0.01f) {
        snapTo(target);
        return;
    }

    start_ = current;
    target_ = target;
    durationSeconds_ = std::max(0.001f, durationSeconds);
    startedAt_ = std::chrono::steady_clock::now();
    animating_ = true;
}

float VerticalReveal::value() const {
    if (!animating_) return target_;

    const auto elapsed = std::chrono::steady_clock::now() - startedAt_;
    const float progress = std::clamp(
        std::chrono::duration<float>(elapsed).count() / durationSeconds_, 0.0f, 1.0f);
    const float eased = 1.0f - std::pow(1.0f - progress, 3.0f);
    return start_ + (target_ - start_) * eased;
}

bool VerticalReveal::isAnimating() const {
    if (!animating_) return false;
    const auto elapsed = std::chrono::steady_clock::now() - startedAt_;
    return std::chrono::duration<float>(elapsed).count() < durationSeconds_;
}

// ═══════════════════════════════════════════════════════════════════
//  LABEL
// ═══════════════════════════════════════════════════════════════════

Label::Label() {
    color_ = getTheme().textPrimary;
    ensureText_();
}

Label::Label(const std::string& str, unsigned int fontSize, sf::Color color, bool bold)
    : value_(str), fontSize_(fontSize), color_(color), bold_(bold) {
    ensureText_();
}

bool Label::ensureText_() const {
    const std::uint64_t revision = getInitializationRevision();
    if (textInitializationRevision_ != revision) {
        text_.reset();
        textInitializationRevision_ = revision;
    }
    if (text_) return true;
    const sf::Font* font = bold_ ? getBoldFont() : getRegularFont();
    if (!font) return false;
    text_.emplace(*font);
    text_->setString(value_);
    std::uint32_t style = sf::Text::Regular;
    if (bold_) style |= sf::Text::Bold;
    if (underlined_) style |= sf::Text::Underlined;
    text_->setStyle(style);
    applyCrispText(*text_, fontSize_);
    text_->setFillColor(color_);
    text_->setPosition(snapToTextPixelGrid(position_));
    text_->setOutlineColor(outlineColor_);
    text_->setOutlineThickness(outlineThickness_);
    return true;
}

void Label::setText(const std::string& str) {
    value_ = str;
    if (ensureText_()) text_->setString(str);
}
void Label::setFontSize(unsigned int size) {
    fontSize_ = size;
    if (ensureText_()) applyCrispText(*text_, fontSize_);
}
void Label::setColor(sf::Color color) {
    color_ = color;
    if (ensureText_()) text_->setFillColor(color);
}
void Label::setPosition(float x, float y) { setPosition(sf::Vector2f{x, y}); }
void Label::setPosition(const sf::Vector2f& pos) {
    position_ = pos;
    if (ensureText_()) text_->setPosition(snapToTextPixelGrid(pos));
}
sf::Vector2f Label::getPosition() const { return position_; }
sf::FloatRect Label::getBounds() const {
    if (!ensureText_()) return sf::FloatRect(position_, {0.f, 0.f});
    applyCrispText(*text_, fontSize_);
    return getScaledLocalBounds(*text_);
}
void Label::setOutline(sf::Color color, float thickness) {
    outlineColor_ = color;
    outlineThickness_ = thickness;
    if (ensureText_()) {
        text_->setOutlineColor(color);
        text_->setOutlineThickness(thickness);
    }
}
void Label::setUnderlined(bool underlined) {
    underlined_ = underlined;
    if (ensureText_()) {
        std::uint32_t style = sf::Text::Regular;
        if (bold_) style |= sf::Text::Bold;
        if (underlined_) style |= sf::Text::Underlined;
        text_->setStyle(style);
    }
}
bool Label::isUnderlined() const { return underlined_; }
void Label::draw(sf::RenderWindow& window) const {
    if (!ensureText_()) return;
    applyCrispText(*text_, fontSize_);
    sf::Text t = *text_;
    snapTextToPixelGrid(t);
    window.draw(t);
}

const sf::Text& Label::getText() const {
    if (!ensureText_()) {
        throw std::logic_error("RenUI::Label has no text object because no font is available");
    }
    applyCrispText(*text_, fontSize_);
    return *text_;
}

// ═══════════════════════════════════════════════════════════════════
//  PANEL
// ═══════════════════════════════════════════════════════════════════

Panel::Panel() {
    const Theme theme = getTheme();
    shape_.setFillColor(theme.panelBackground);
    shape_.setOutlineColor(theme.panelBorder);
    shape_.setOutlineThickness(2.0f);
}

Panel::Panel(const sf::Vector2f& position, const sf::Vector2f& size,
             sf::Color fillColor, sf::Color outlineColor, float outlineThickness) {
    shape_.setPosition(position);
    shape_.setSize(size);
    shape_.setFillColor(fillColor);
    shape_.setOutlineColor(outlineColor);
    shape_.setOutlineThickness(outlineThickness);
}

void Panel::setPosition(float x, float y) { shape_.setPosition({x, y}); }
void Panel::setPosition(const sf::Vector2f& pos) { shape_.setPosition(pos); }
sf::Vector2f Panel::getPosition() const { return shape_.getPosition(); }
void Panel::setSize(const sf::Vector2f& size) { shape_.setSize(size); }
sf::Vector2f Panel::getSize() const { return shape_.getSize(); }
void Panel::setFillColor(sf::Color color) { shape_.setFillColor(color); }
void Panel::setOutlineColor(sf::Color color) { shape_.setOutlineColor(color); }
void Panel::setOutlineThickness(float t) { shape_.setOutlineThickness(t); }
bool Panel::contains(const sf::Vector2f& point) const { return shape_.getGlobalBounds().contains(point); }
sf::FloatRect Panel::getGlobalBounds() const { return shape_.getGlobalBounds(); }
void Panel::draw(sf::RenderWindow& window) const { window.draw(shape_); }

// ═══════════════════════════════════════════════════════════════════
//  ICON SPRITE
// ═══════════════════════════════════════════════════════════════════

IconSprite::IconSprite() = default;

void IconSprite::setTexture(const sf::Texture& texture, const sf::IntRect& rect) {
    sprite_.emplace(texture, rect);
    hasTexture_ = true;
}

void IconSprite::setPosition(float x, float y) { if (sprite_) sprite_->setPosition({x, y}); }
void IconSprite::setScale(float scaleX, float scaleY) { if (sprite_) sprite_->setScale({scaleX, scaleY}); }
void IconSprite::draw(sf::RenderWindow& window) const {
    if (hasTexture_ && sprite_) window.draw(*sprite_);
}
sf::FloatRect IconSprite::getGlobalBounds() const { return sprite_ ? sprite_->getGlobalBounds() : sf::FloatRect(); }

// ═══════════════════════════════════════════════════════════════════
//  BUTTON
// ═══════════════════════════════════════════════════════════════════

UIButton::UIButton() {
    const Theme theme = getTheme();
    textColor_ = theme.textPrimary;
    ensureText_();
    shape_.setFillColor(theme.buttonNormal);
    shape_.setOutlineColor(theme.buttonBorder);
    shape_.setOutlineThickness(2);
}

UIButton::UIButton(const std::string& label, const sf::Vector2f& position, const sf::Vector2f& size,
                   unsigned int fontSize)
    : label_(label), fontSize_(fontSize), minimumSize_(size),
      autoExpandToFitLabel_(label.size() > 1) {
    const Theme theme = getTheme();
    textColor_ = theme.textPrimary;
    ensureText_();

    shape_.setSize(size);
    shape_.setPosition(position);
    shape_.setFillColor(theme.buttonNormal);
    shape_.setOutlineColor(theme.buttonBorder);
    shape_.setOutlineThickness(2);

    invalidateTextLayout_();
}

void UIButton::draw(sf::RenderWindow& window) const {
    // Application-owned UI objects can be constructed before the shared font cache
    // is initialized. Materialize and position their labels on first use.
    refreshTextLayout_();

    float hoverBlend = 0.0f;
    if (style_ == UIButtonStyle::HoverGlowBlue) {
        const bool glowActive = enabled_ && (selected_ || hovered_ || focused_);
        const float now = getHoverGlowTimeSeconds();
        if (!hoverAnimInitialized_) {
            hoverAnimInitialized_ = true;
            hoverAnimSampleTime_ = now;
            hoverAnim_ = glowActive ? 1.0f : 0.0f;
        }

        float dt = now - hoverAnimSampleTime_;
        hoverAnimSampleTime_ = now;
        if (!std::isfinite(dt) || dt < 0.0f || dt > 0.25f) {
            dt = 1.0f / 60.0f;
        }

        constexpr float HOVER_ANIM_DURATION_SEC = 0.30f;
        constexpr float HOVER_IN_SPEED = 1.0f / HOVER_ANIM_DURATION_SEC;
        constexpr float HOVER_OUT_SPEED = 1.0f / HOVER_ANIM_DURATION_SEC;
        if (selected_) {
            hoverAnim_ = 1.0f;
        } else if (glowActive) {
            hoverAnim_ = std::min(1.0f, hoverAnim_ + dt * HOVER_IN_SPEED);
        } else {
            hoverAnim_ = std::max(0.0f, hoverAnim_ - dt * HOVER_OUT_SPEED);
        }

        hoverBlend = hoverAnim_ * hoverAnim_ * (3.0f - 2.0f * hoverAnim_);
        const float glowStrength = 0.95f * hoverBlend;
        constexpr float HOVER_GLOW_LABEL_SIDE_MARGIN = 24.0f;
        const float labelWidth = text_
            ? getScaledLocalBounds(*text_).size.x
            : 0.0f;
        const float minimumGlowWidth = labelWidth > 0.0f
            ? labelWidth + HOVER_GLOW_LABEL_SIDE_MARGIN * 2.0f
            : 0.0f;
        drawHoverGlowBackdrop(window, shape_.getPosition(), shape_.getSize(), glowStrength,
                              hoverBlend, hoverGlowWidthScale_, minimumGlowWidth);
    }

    sf::RectangleShape drawShape = shape_;
    const Theme theme = getTheme();
    if (!enabled_) {
        drawShape.setFillColor(theme.buttonDisabled);
        drawShape.setOutlineColor(theme.panelBorder);
        drawShape.setOutlineThickness(1.f);
    } else if (style_ == UIButtonStyle::PanelOutline) {
        const bool highlighted = selected_ || focused_;
        drawShape.setFillColor(highlighted ? theme.buttonActive : theme.panelBackground);
        drawShape.setOutlineColor(
            highlighted ? theme.focusOutline
                        : hovered_ ? theme.buttonHoverBorder : theme.panelBorder);
        drawShape.setOutlineThickness((highlighted || hovered_) ? 2.0f : 1.0f);
    } else if (selected_) {
        drawShape.setFillColor(theme.buttonActive);
        drawShape.setOutlineColor(theme.textPrimary);
        drawShape.setOutlineThickness(2);
    } else if (style_ == UIButtonStyle::HoverGlowBlue) {
        drawShape.setFillColor(lerpColor(theme.buttonGlowBase,
                                         theme.buttonGlowHover, hoverBlend));
        drawShape.setOutlineColor(lerpColor(theme.buttonGlowBorder,
                                            theme.buttonGlowHoverBorder, hoverBlend));
        drawShape.setOutlineThickness(1.5f + 0.6f * hoverBlend);
    } else if (style_ == UIButtonStyle::Primary) {
        drawShape.setFillColor(hovered_ ? theme.buttonPrimaryHover : theme.buttonPrimary);
        drawShape.setOutlineColor(hovered_ ? theme.buttonPrimaryHoverBorder
                                           : theme.buttonPrimaryBorder);
        drawShape.setOutlineThickness(hovered_ ? 2.0f : 1.5f);
    } else if (style_ == UIButtonStyle::Danger) {
        drawShape.setFillColor(hovered_ ? theme.buttonDangerHover : theme.buttonDanger);
        drawShape.setOutlineColor(hovered_ ? theme.buttonHoverBorder : theme.buttonDangerBorder);
        drawShape.setOutlineThickness(hovered_ ? 2.f : 1.5f);
    } else if (focused_) {
        drawShape.setOutlineColor(theme.focusOutline);
        drawShape.setOutlineThickness(2);
    } else if (hovered_) {
        if (!customFillColor_) drawShape.setFillColor(theme.buttonHover);
        if (!customOutlineColor_) drawShape.setOutlineColor(theme.buttonHoverBorder);
    }
    if (frameVisible_) {
        window.draw(drawShape);
    }
    if (text_) {
        sf::Text t = *text_;
        if (!enabled_) t.setFillColor(theme.textDisabled);
        if (style_ == UIButtonStyle::HoverGlowBlue && hoverBlend > 0.001f) {
            t.setFillColor(lerpColor(theme.buttonGlowText, theme.buttonGlowHoverText,
                                     std::min(1.0f, hoverBlend * 1.15f)));
            t.setOutlineColor(sf::Color(0, 0, 0, 225));
            t.setOutlineThickness(0.65f + 0.45f * hoverBlend);

            sf::Text shadow = t;
            shadow.setOutlineThickness(0.0f);
            const float shadowAlphaF = std::clamp(80.0f + 95.0f * hoverBlend, 0.0f, 255.0f);
            shadow.setFillColor(sf::Color(0, 0, 0, static_cast<std::uint8_t>(shadowAlphaF)));
            shadow.setPosition(snapToTextPixelGrid(t.getPosition() + sf::Vector2f(0.9f, 0.9f)));
            snapTextToPixelGrid(shadow);
            window.draw(shadow);
        }
        snapTextToPixelGrid(t);
        window.draw(t);
    }
}

bool UIButton::isClicked(const sf::Vector2i& mousePosition) const {
    if (!enabled_) return false;
    refreshTextLayout_();
    return shape_.getGlobalBounds().contains(static_cast<sf::Vector2f>(mousePosition));
}

bool UIButton::contains(const sf::Vector2f& point) const {
    refreshTextLayout_();
    return shape_.getGlobalBounds().contains(point);
}

void UIButton::updateHover(const sf::Vector2i& mousePosition) {
    refreshTextLayout_();
    hovered_ = enabled_ &&
        shape_.getGlobalBounds().contains(static_cast<sf::Vector2f>(mousePosition));
}

bool UIButton::isHovered() const { return hovered_; }
void UIButton::setHovered(bool hovered) { hovered_ = hovered; }
void UIButton::setEnabled(bool enabled) {
    enabled_ = enabled;
    if (!enabled_) hovered_ = false;
}
bool UIButton::isEnabled() const { return enabled_; }

void UIButton::setFocused(bool f) { focused_ = f; }
bool UIButton::isFocused() const { return focused_; }
void UIButton::setSelected(bool s) { selected_ = s; }
bool UIButton::isSelected() const { return selected_; }
void UIButton::setStyle(UIButtonStyle style) {
    if (style_ == style) return;
    style_ = style;
    if (ensureText_()) {
        text_->setStyle((style_ == UIButtonStyle::HoverGlowBlue ||
                         style_ == UIButtonStyle::PanelOutline ||
                         style_ == UIButtonStyle::Primary ||
                         style_ == UIButtonStyle::Danger)
                            ? sf::Text::Bold
                            : sf::Text::Regular);
    }
    invalidateTextLayout_();
}
UIButtonStyle UIButton::getStyle() const { return style_; }
void UIButton::setPrimary(bool primary) {
    setStyle(primary ? UIButtonStyle::Primary : UIButtonStyle::Default);
}
bool UIButton::isPrimary() const { return style_ == UIButtonStyle::Primary; }
void UIButton::setFrameVisible(bool visible) { frameVisible_ = visible; }
bool UIButton::isFrameVisible() const { return frameVisible_; }
void UIButton::setHoverGlowWidthScale(float scale) {
    hoverGlowWidthScale_ = std::max(0.0f, scale);
}
float UIButton::getHoverGlowWidthScale() const { return hoverGlowWidthScale_; }

void UIButton::setLabel(const std::string& label) {
    if (label_ == label) return;
    label_ = label;
    if (ensureText_()) text_->setString(label_);
    invalidateTextLayout_();
}

void UIButton::setPosition(const sf::Vector2f& pos) {
    if (shape_.getPosition() == pos) return;
    shape_.setPosition(pos);
    invalidateTextLayout_();
}

sf::Vector2f UIButton::getPosition() const { return shape_.getPosition(); }

void UIButton::setSize(const sf::Vector2f& size) {
    const sf::Vector2f normalizedSize{
        std::max(0.0f, size.x),
        std::max(0.0f, size.y)
    };
    if (minimumSize_ == normalizedSize) return;
    minimumSize_ = normalizedSize;
    invalidateTextLayout_();
}

sf::Vector2f UIButton::getSize() const {
    refreshTextLayout_();
    return shape_.getSize();
}

void UIButton::setAutoExpandToFitLabel(bool enabled, const sf::Vector2f& padding) {
    const sf::Vector2f normalizedPadding{
        std::max(0.0f, padding.x),
        std::max(0.0f, padding.y)
    };
    if (autoExpandToFitLabel_ == enabled && labelPadding_ == normalizedPadding) return;
    autoExpandToFitLabel_ = enabled;
    labelPadding_ = normalizedPadding;
    invalidateTextLayout_();
}

bool UIButton::getAutoExpandToFitLabel() const { return autoExpandToFitLabel_; }

void UIButton::setFillColor(sf::Color color) {
    customFillColor_ = true;
    shape_.setFillColor(color);
}
void UIButton::setOutlineColor(sf::Color color) {
    customOutlineColor_ = true;
    shape_.setOutlineColor(color);
}
void UIButton::setTextColor(sf::Color color) {
    if (textColor_ == color) return;
    textColor_ = color;
    if (ensureText_()) text_->setFillColor(textColor_);
}

bool UIButton::ensureText_() const {
    const std::uint64_t revision = getInitializationRevision();
    if (textInitializationRevision_ != revision) {
        text_.reset();
        textInitializationRevision_ = revision;
        invalidateTextLayout_();
    }
    if (text_) return true;
    const sf::Font* font = getRegularFont();
    if (!font) return false;

    text_.emplace(*font);
    text_->setString(label_);
    text_->setStyle((style_ == UIButtonStyle::HoverGlowBlue ||
                     style_ == UIButtonStyle::PanelOutline ||
                     style_ == UIButtonStyle::Primary ||
                     style_ == UIButtonStyle::Danger)
                        ? sf::Text::Bold
                        : sf::Text::Regular);
    applyCrispText(*text_, fontSize_);
    text_->setFillColor(textColor_);
    invalidateTextLayout_();
    return true;
}

void UIButton::invalidateTextLayout_() const {
    textLayoutDirty_ = true;
}

void UIButton::refreshTextLayout_() const {
    if (!ensureText_()) return;

    const std::uint64_t metricsRevision = getTextMetricsRevision();
    const float uiScaleFactor = getUIScaleFactor();
    const float renderScaleFactor = getTextScaleFactor();
    if (!textLayoutDirty_ &&
        textLayoutMetricsRevision_ == metricsRevision &&
        textLayoutUIScaleFactor_ == uiScaleFactor &&
        textLayoutRenderScaleFactor_ == renderScaleFactor) {
        return;
    }

    applyCrispText(*text_, fontSize_);

    sf::FloatRect textBounds = getScaledLocalBounds(*text_);
    sf::Vector2f fittedSize = minimumSize_;
    if (autoExpandToFitLabel_ && !label_.empty()) {
        fittedSize.x = std::max(fittedSize.x, textBounds.size.x + labelPadding_.x);
        fittedSize.y = std::max(fittedSize.y, textBounds.size.y + labelPadding_.y);
    }
    shape_.setSize(fittedSize);

    auto pos = shape_.getPosition();
    auto size = shape_.getSize();
    text_->setOrigin({0.f, 0.f});
    text_->setPosition(snapToTextPixelGrid({
        pos.x + (size.x - textBounds.size.x) / 2.0f - textBounds.position.x,
        pos.y + (size.y - textBounds.size.y) / 2.0f - textBounds.position.y
    }));

    textLayoutMetricsRevision_ = metricsRevision;
    textLayoutUIScaleFactor_ = uiScaleFactor;
    textLayoutRenderScaleFactor_ = renderScaleFactor;
    textLayoutDirty_ = false;
}

// ═══════════════════════════════════════════════════════════════════
//  CHECKBOX
// ═══════════════════════════════════════════════════════════════════

Checkbox::Checkbox() {
    const Theme theme = getTheme();
    box_.setSize({boxSize_, boxSize_});
    box_.setFillColor(theme.inputBackground);
    box_.setOutlineColor(theme.inputBorder);
    box_.setOutlineThickness(2.f);
}

Checkbox::Checkbox(const std::string& label, const sf::Vector2f& position,
                   float boxSize, unsigned int fontSize)
    : labelText_(label), boxSize_(boxSize), fontSize_(fontSize) {
    const Theme theme = getTheme();
    box_.setSize({boxSize_, boxSize_});
    box_.setPosition(position);
    box_.setFillColor(theme.inputBackground);
    box_.setOutlineColor(theme.inputBorder);
    box_.setOutlineThickness(2.f);

    ensureLabel_();
    positionLabel();
}

bool Checkbox::ensureLabel_() const {
    const std::uint64_t revision = getInitializationRevision();
    if (labelInitializationRevision_ != revision) {
        label_.reset();
        labelInitializationRevision_ = revision;
    }
    if (label_) return true;
    const sf::Font* font = getRegularFont();
    if (!font) return false;
    label_.emplace(*font);
    label_->setString(labelText_);
    applyCrispText(*label_, fontSize_);
    label_->setFillColor(getTheme().textSecondary);
    return true;
}

void Checkbox::positionLabel() const {
    if (!ensureLabel_()) return;
    applyCrispText(*label_, fontSize_);
    const auto pos = box_.getPosition();
    sf::FloatRect bounds = getScaledLocalBounds(*label_);
    label_->setOrigin({0.f, 0.f});
    label_->setPosition(snapToTextPixelGrid({
        pos.x + boxSize_ + 8.f,
        pos.y + (boxSize_ - bounds.size.y) / 2.0f - bounds.position.y
    }));
}

void Checkbox::draw(sf::RenderWindow& window) const {
    positionLabel();
    sf::RectangleShape drawBox = box_;
    const Theme theme = getTheme();
    drawBox.setOutlineColor(focused_ ? theme.focusOutline : theme.inputBorder);
    window.draw(drawBox);

    if (checked_) {
        const auto pos = box_.getPosition();
        sf::RectangleShape mark({boxSize_ - 8.f, boxSize_ - 8.f});
        mark.setPosition({pos.x + 4.f, pos.y + 4.f});
        mark.setFillColor(theme.buttonPrimaryHover);
        window.draw(mark);
    }

    if (label_) {
        sf::Text l = *label_;
        snapTextToPixelGrid(l);
        window.draw(l);
    }
}

bool Checkbox::handleClick(const sf::Vector2i& mousePosition) {
    if (contains(static_cast<sf::Vector2f>(mousePosition))) {
        checked_ = !checked_;
        return true;
    }
    return false;
}

bool Checkbox::contains(const sf::Vector2f& point) const {
    return getBounds().contains(point);
}

void Checkbox::setChecked(bool c) { checked_ = c; }
bool Checkbox::isChecked() const { return checked_; }
void Checkbox::setFocused(bool f) { focused_ = f; }

void Checkbox::setLabel(const std::string& label) {
    labelText_ = label;
    if (ensureLabel_()) {
        label_->setString(label);
        positionLabel();
    }
}

void Checkbox::setPosition(const sf::Vector2f& pos) {
    box_.setPosition(pos);
    positionLabel();
}

sf::Vector2f Checkbox::getPosition() const { return box_.getPosition(); }

sf::FloatRect Checkbox::getBounds() const {
    positionLabel();
    sf::FloatRect bounds = box_.getGlobalBounds();
    if (!label_) return bounds;

    const sf::FloatRect labelBounds = label_->getGlobalBounds();
    const float left = std::min(bounds.position.x, labelBounds.position.x);
    const float top = std::min(bounds.position.y, labelBounds.position.y);
    const float right = std::max(bounds.position.x + bounds.size.x,
                                 labelBounds.position.x + labelBounds.size.x);
    const float bottom = std::max(bounds.position.y + bounds.size.y,
                                  labelBounds.position.y + labelBounds.size.y);
    return sf::FloatRect({left, top}, {right - left, bottom - top});
}

// ════════════════════════════════════════════════════════════════════════════════
//  SLIDER
// ═══════════════════════════════════════════════════════════════════════════════

Slider::Slider() = default;

Slider::Slider(const sf::Vector2f& position, const sf::Vector2f& size,
               float minimum, float maximum, float value, float step)
    : position_(position) {
    setSize(size);
    setRange(minimum, maximum);
    setStep(step);
    setValue(value);
}

void Slider::draw(sf::RenderWindow& window) const {
    const Theme theme = getTheme();
    const float diameter = std::clamp(size_.y * 0.70f, 10.0f, 18.0f);
    const float radius = diameter * 0.5f;
    const float trackLeft = position_.x + radius;
    const float trackWidth = std::max(1.0f, size_.x - diameter);
    const float trackHeight = std::clamp(size_.y * 0.20f, 3.0f, 6.0f);
    const float trackY = position_.y + (size_.y - trackHeight) * 0.5f;
    const float filledWidth = trackWidth * getNormalizedValue();

    sf::RectangleShape track({trackWidth, trackHeight});
    track.setPosition({trackLeft, trackY});
    track.setFillColor(theme.panelBackground);
    track.setOutlineColor(hovered_ || dragging_ ? theme.focusOutline
                                                : theme.panelBorder);
    track.setOutlineThickness(1.0f);
    window.draw(track);

    if (filledWidth > 0.0f) {
        sf::RectangleShape fill({filledWidth, trackHeight});
        fill.setPosition({trackLeft, trackY});
        fill.setFillColor(dragging_ ? theme.buttonHover : theme.selectionFill);
        window.draw(fill);
    }

    const std::size_t tickCount = getVisibleTickCount();
    if (tickCount > 1) {
        sf::RectangleShape tick({1.0f, trackHeight + 4.0f});
        tick.setFillColor(theme.divider);
        for (std::size_t i = 0; i < tickCount; ++i) {
            const float ratio = static_cast<float>(i)
                              / static_cast<float>(tickCount - 1);
            tick.setPosition({trackLeft + trackWidth * ratio - 0.5f,
                              trackY - 2.0f});
            window.draw(tick);
        }
    }

    sf::CircleShape thumb(radius);
    thumb.setOrigin({radius, radius});
    thumb.setPosition({trackLeft + filledWidth, position_.y + size_.y * 0.5f});
    thumb.setFillColor(dragging_ ? theme.textPrimary
                                 : hovered_ ? theme.textSecondary : theme.textMuted);
    thumb.setOutlineColor(theme.panelBorder);
    thumb.setOutlineThickness(1.5f);
    window.draw(thumb);
}

bool Slider::handlePointerPress(const sf::Vector2f& pointer) {
    hovered_ = getBounds().contains(pointer);
    if (!hovered_) return false;
    dragging_ = true;
    updateFromPointer_(pointer);
    return true;
}

bool Slider::handlePointerMove(const sf::Vector2f& pointer) {
    hovered_ = getBounds().contains(pointer);
    if (!dragging_) return false;
    updateFromPointer_(pointer);
    return true;
}

bool Slider::handlePointerRelease(const sf::Vector2f& pointer) {
    hovered_ = getBounds().contains(pointer);
    const bool wasDragging = dragging_;
    if (dragging_) updateFromPointer_(pointer);
    dragging_ = false;
    return wasDragging;
}

void Slider::setPosition(const sf::Vector2f& position) { position_ = position; }
sf::Vector2f Slider::getPosition() const { return position_; }

void Slider::setSize(const sf::Vector2f& size) {
    size_ = {std::max(1.0f, size.x), std::max(1.0f, size.y)};
}

sf::Vector2f Slider::getSize() const { return size_; }
sf::FloatRect Slider::getBounds() const { return sf::FloatRect(position_, size_); }

void Slider::setRange(float minimum, float maximum) {
    if (!std::isfinite(minimum)) minimum = 0.0f;
    if (!std::isfinite(maximum)) maximum = minimum;
    if (maximum < minimum) std::swap(minimum, maximum);
    minimum_ = minimum;
    maximum_ = maximum;
    value_ = snapAndClamp_(value_);
}

float Slider::getMinimum() const { return minimum_; }
float Slider::getMaximum() const { return maximum_; }

void Slider::setStep(float step) {
    step_ = std::isfinite(step) ? std::abs(step) : 0.0f;
    value_ = snapAndClamp_(value_);
}

float Slider::getStep() const { return step_; }
void Slider::setValue(float value) { value_ = snapAndClamp_(value); }
float Slider::getValue() const { return value_; }

float Slider::getNormalizedValue() const {
    const float span = maximum_ - minimum_;
    return span > 0.0f ? std::clamp((value_ - minimum_) / span, 0.0f, 1.0f) : 0.0f;
}

std::size_t Slider::getVisibleTickCount() const {
    const float span = maximum_ - minimum_;
    if (step_ <= 0.0f || span <= 0.0f) return 0;

    const float intervalEstimate = span / step_;
    if (!std::isfinite(intervalEstimate) || intervalEstimate > 10.0001f) return 0;
    const auto intervals = static_cast<std::size_t>(
        std::max(1.0f, std::ceil(intervalEstimate - 0.0001f)));
    return intervals + 1;
}

bool Slider::isHovered() const { return hovered_; }
bool Slider::isDragging() const { return dragging_; }

float Slider::snapAndClamp_(float value) const {
    if (!std::isfinite(value)) value = minimum_;
    value = std::clamp(value, minimum_, maximum_);
    if (step_ > 0.0f && maximum_ > minimum_) {
        value = minimum_ + std::round((value - minimum_) / step_) * step_;
        value = std::clamp(value, minimum_, maximum_);
    }
    return value;
}

void Slider::updateFromPointer_(const sf::Vector2f& pointer) {
    const float diameter = std::clamp(size_.y * 0.70f, 10.0f, 18.0f);
    const float trackLeft = position_.x + diameter * 0.5f;
    const float trackWidth = std::max(1.0f, size_.x - diameter);
    const float ratio = std::clamp((pointer.x - trackLeft) / trackWidth, 0.0f, 1.0f);
    setValue(minimum_ + ratio * (maximum_ - minimum_));
}

// ═══════════════════════════════════════════════════════════════════
//  PROGRESS BAR
// ═══════════════════════════════════════════════════════════════════

ProgressBar::ProgressBar() {
    const Theme theme = getTheme();
    textColor_ = theme.textPrimary;
    background_.setFillColor(theme.inputBackground);
    fill_.setFillColor(theme.buttonActive);
    ensureText_();
}

ProgressBar::ProgressBar(const sf::Vector2f& position, const sf::Vector2f& size,
                         sf::Color bgColor, sf::Color fillColor, sf::Color textColor,
                         unsigned int fontSize)
    : fontSize_(fontSize), textColor_(textColor) {
    background_.setPosition(position);
    background_.setSize(size);
    background_.setFillColor(bgColor);

    fill_.setPosition(position);
    fill_.setSize(size);
    fill_.setFillColor(fillColor);

    ensureText_();
    updateTextPosition();
}

bool ProgressBar::ensureText_() const {
    const std::uint64_t revision = getInitializationRevision();
    if (textInitializationRevision_ != revision) {
        text_.reset();
        textInitializationRevision_ = revision;
    }
    if (text_) return true;
    const sf::Font* font = getRegularFont();
    if (!font) return false;
    text_.emplace(*font);
    applyCrispText(*text_, fontSize_);
    text_->setFillColor(textColor_);
    text_->setString(label_);
    updateTextPosition();
    return true;
}

void ProgressBar::setValue(float current, float max) {
    max_ = std::isfinite(max) ? std::max(1.0f, max) : 1.0f;
    current_ = std::isfinite(current)
        ? std::clamp(current, 0.0f, max_)
        : 0.0f;
    updateFill();
    if (!customLabel_) {
        label_ = std::to_string(static_cast<int>(current_)) + "/" +
                 std::to_string(static_cast<int>(max_));
        if (ensureText_()) text_->setString(label_);
        updateTextPosition();
    }
}

float ProgressBar::getNormalizedValue() const {
    return std::clamp(current_ / max_, 0.0f, 1.0f);
}

void ProgressBar::setLabel(const std::string& label) {
    customLabel_ = true;
    label_ = label;
    if (ensureText_()) text_->setString(label);
    updateTextPosition();
}

void ProgressBar::setPosition(const sf::Vector2f& pos) {
    background_.setPosition(pos);
    fill_.setPosition(pos);
    updateTextPosition();
}

sf::Vector2f ProgressBar::getPosition() const { return background_.getPosition(); }

void ProgressBar::draw(sf::RenderWindow& window) const {
    window.draw(background_);
    window.draw(fill_);
    if (ensureText_()) {
        sf::Text t = *text_;
        applyCrispText(t, fontSize_);
        const sf::FloatRect textBounds = getScaledLocalBounds(t);
        const auto pos = background_.getPosition();
        const auto size = background_.getSize();
        t.setOrigin({0.0f, 0.0f});
        t.setPosition(snapToTextPixelGrid({
            pos.x + (size.x - textBounds.size.x) * 0.5f - textBounds.position.x,
            pos.y + (size.y - textBounds.size.y) * 0.5f - textBounds.position.y
        }));
        snapTextToPixelGrid(t);
        window.draw(t);
    }
}

void ProgressBar::updateFill() {
    auto size = background_.getSize();
    fill_.setSize(sf::Vector2f(size.x * getNormalizedValue(), size.y));
}

void ProgressBar::updateTextPosition() const {
    if (!ensureText_()) return;
    sf::FloatRect textBounds = getScaledLocalBounds(*text_);
    auto pos = background_.getPosition();
    auto size = background_.getSize();
    text_->setOrigin({0.f, 0.f});
    text_->setPosition(snapToTextPixelGrid({
        pos.x + (size.x - textBounds.size.x) / 2.0f - textBounds.position.x,
        pos.y + (size.y - textBounds.size.y) / 2.0f - textBounds.position.y
    }));
}

Dropdown::Dropdown(const sf::Vector2f& position, const sf::Vector2f& size,
                   const std::vector<std::string>& options, int selectedIndex)
    : position_(position),
      size_({std::max(0.f, size.x), std::max(0.f, size.y)}),
      options_(options) {
    setSelectedIndex(selectedIndex);
    if (selectedIndex_ < 0 && !options_.empty()) selectedIndex_ = 0;
}

const std::string& Dropdown::getSelectedText() const {
    static const std::string empty;
    return selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(options_.size())
        ? options_[static_cast<std::size_t>(selectedIndex_)] : empty;
}

void Dropdown::setSelectedIndex(int index) {
    if (index >= 0 && index < static_cast<int>(options_.size())) selectedIndex_ = index;
}

void Dropdown::draw(sf::RenderWindow& window) const {
    const Theme theme = getTheme();
    sf::RectangleShape header(size_);
    header.setPosition(position_);
    header.setFillColor(headerHovered_ && !open_ ? theme.buttonHover
                                                 : theme.inputBackground);
    header.setOutlineColor(headerHovered_ ? theme.buttonHoverBorder : theme.inputBorder);
    header.setOutlineThickness(1.f);
    window.draw(header);

    const sf::Font* font = getRegularFont();
    auto drawText = [&](const std::string& value, const sf::Vector2f& position,
                        unsigned int size, sf::Color color) {
        if (!font || value.empty()) return;
        sf::Text text(*font, value, size);
        applyCrispText(text, size);
        text.setFillColor(color);
        text.setPosition(snapToTextPixelGrid(position));
        window.draw(text);
    };

    drawText(getSelectedText(), {position_.x + 8.f, position_.y + 5.f}, 13,
             theme.textPrimary);
    drawText(open_ ? "^" : "v", {position_.x + size_.x - 18.f, position_.y + 5.f},
             12, theme.textMuted);
    if (!open_) return;

    for (int index = 0; index < static_cast<int>(options_.size()); ++index) {
        const float itemY = position_.y + size_.y * static_cast<float>(index + 1);
        sf::RectangleShape item(size_);
        item.setPosition({position_.x, itemY});
        item.setFillColor(index == hoveredOption_ ? theme.buttonHover
                          : index == selectedIndex_ ? theme.buttonActive
                                                    : theme.panelBackground);
        item.setOutlineColor(theme.panelBorder);
        item.setOutlineThickness(1.f);
        window.draw(item);
        drawText(options_[static_cast<std::size_t>(index)],
                 {position_.x + 8.f, itemY + 5.f}, 13, theme.textPrimary);
    }
}

bool Dropdown::handleClick(const sf::Vector2i& mousePosition) {
    const sf::Vector2f mouse = static_cast<sf::Vector2f>(mousePosition);
    if (sf::FloatRect(position_, size_).contains(mouse)) {
        if (!options_.empty()) open_ = !open_;
        return true;
    }
    if (!open_) return false;

    for (int index = 0; index < static_cast<int>(options_.size()); ++index) {
        const sf::Vector2f itemPosition{
            position_.x, position_.y + size_.y * static_cast<float>(index + 1)};
        if (sf::FloatRect(itemPosition, size_).contains(mouse)) {
            selectedIndex_ = index;
            open_ = false;
            return true;
        }
    }
    open_ = false;
    return true;
}

void Dropdown::updateHover(const sf::Vector2i& mousePosition) {
    const sf::Vector2f mouse = static_cast<sf::Vector2f>(mousePosition);
    headerHovered_ = sf::FloatRect(position_, size_).contains(mouse);
    hoveredOption_ = -1;
    if (!open_) return;
    for (int index = 0; index < static_cast<int>(options_.size()); ++index) {
        const sf::Vector2f itemPosition{
            position_.x, position_.y + size_.y * static_cast<float>(index + 1)};
        if (sf::FloatRect(itemPosition, size_).contains(mouse)) {
            hoveredOption_ = index;
            return;
        }
    }
}

void Dropdown::close() {
    open_ = false;
    hoveredOption_ = -1;
}

ToggleSwitch::ToggleSwitch(const std::string& label, const sf::Vector2f& position,
                           bool initialState)
    : position_(position), label_(label), enabled_(initialState) {}

sf::FloatRect ToggleSwitch::bounds_() const {
    float labelWidth = 0.f;
    float labelHeight = 14.f;
    if (const sf::Font* font = getRegularFont()) {
        sf::Text text(*font, label_, 14);
        applyCrispText(text, 14);
        const auto bounds = getScaledLocalBounds(text);
        labelWidth = bounds.size.x;
        labelHeight = bounds.size.y;
    }
    const float switchX = position_.x + labelWidth + LabelGap;
    return sf::FloatRect(position_,
                         {switchX + SwitchWidth - position_.x,
                          std::max(SwitchHeight + 2.f, labelHeight + 4.f)});
}

void ToggleSwitch::draw(sf::RenderWindow& window) const {
    const Theme theme = getTheme();
    float labelWidth = 0.f;
    if (const sf::Font* font = getRegularFont()) {
        sf::Text text(*font, label_, 14);
        applyCrispText(text, 14);
        text.setFillColor(theme.textSecondary);
        text.setPosition(snapToTextPixelGrid({position_.x, position_.y + 1.f}));
        labelWidth = getScaledLocalBounds(text).size.x;
        window.draw(text);
    }

    const float switchX = position_.x + labelWidth + LabelGap;
    const float switchY = position_.y + 1.f;
    sf::RectangleShape box({SwitchWidth, SwitchHeight});
    box.setPosition({switchX, switchY});
    box.setFillColor(theme.inputBackground);
    box.setOutlineThickness(1.f);
    box.setOutlineColor(hovered_ ? theme.buttonHoverBorder : theme.inputBorder);
    window.draw(box);

    if (enabled_) {
        sf::RectangleShape mark({SwitchWidth - 6.f, SwitchHeight - 6.f});
        mark.setPosition({switchX + 3.f, switchY + 3.f});
        mark.setFillColor(theme.buttonActive);
        window.draw(mark);
    }
}

bool ToggleSwitch::handleClick(const sf::Vector2i& mousePosition) {
    if (!bounds_().contains(static_cast<sf::Vector2f>(mousePosition))) return false;
    enabled_ = !enabled_;
    return true;
}

void ToggleSwitch::updateHover(const sf::Vector2i& mousePosition) {
    hovered_ = bounds_().contains(static_cast<sf::Vector2f>(mousePosition));
}

// ═══════════════════════════════════════════════════════════════════
//  SEPARATOR
// ═══════════════════════════════════════════════════════════════════

Separator::Separator() {
    line_.setSize(sf::Vector2f(100.0f, 1.0f));
    line_.setFillColor(getTheme().divider);
}

Separator::Separator(const sf::Vector2f& position, float width, sf::Color color) {
    line_.setSize(sf::Vector2f(width, 1.0f));
    line_.setPosition(position);
    line_.setFillColor(color);
}

void Separator::setPosition(float x, float y) { line_.setPosition({x, y}); }
void Separator::setWidth(float w) { line_.setSize(sf::Vector2f(w, 1.0f)); }
void Separator::draw(sf::RenderWindow& window) const { window.draw(line_); }

// ═══════════════════════════════════════════════════════════════════
//  VALUE BREAKDOWN ROW
// ═══════════════════════════════════════════════════════════════════

ValueBreakdownRow::ValueBreakdownRow()
    : nameLabel_("", getTheme().valueBreakdownFontSize, sf::Color(180, 180, 200)),
      primaryLabel_("", getTheme().valueBreakdownFontSize, sf::Color::White),
      secondaryLabel_("", getTheme().valueBreakdownFontSize, sf::Color(100, 160, 255)),
      modifierLabel_("", getTheme().valueBreakdownFontSize, sf::Color(80, 220, 80)),
      totalLabel_("", getTheme().valueBreakdownFontSize, sf::Color(255, 220, 50)),
      primaryOffset_(getTheme().valueBreakdownPrimaryOffset),
      secondaryOffset_(getTheme().valueBreakdownSecondaryOffset),
      modifierOffset_(getTheme().valueBreakdownModifierOffset),
      totalOffset_(getTheme().valueBreakdownTotalOffset) {}

ValueBreakdownRow::ValueBreakdownRow(const std::string& name,
                                     const sf::Vector2f& position)
    : name_(name), x_(position.x), y_(position.y),
      nameLabel_(name + ":", getTheme().valueBreakdownFontSize, sf::Color(180, 180, 200)),
      primaryLabel_("0", getTheme().valueBreakdownFontSize, sf::Color::White),
      secondaryLabel_("", getTheme().valueBreakdownFontSize, sf::Color(100, 160, 255)),
      modifierLabel_("", getTheme().valueBreakdownFontSize, sf::Color(80, 220, 80)),
      totalLabel_("0", getTheme().valueBreakdownFontSize, sf::Color(255, 220, 50)),
      primaryOffset_(getTheme().valueBreakdownPrimaryOffset),
      secondaryOffset_(getTheme().valueBreakdownSecondaryOffset),
      modifierOffset_(getTheme().valueBreakdownModifierOffset),
      totalOffset_(getTheme().valueBreakdownTotalOffset) {
    positionLabels_();
}

void ValueBreakdownRow::setValues(int primary, int secondary, int modifier) {
    const bool valuesUnchanged =
        cachedValueKind_ == CachedValueKind::Integer &&
        cachedPrimaryInt_ == primary &&
        cachedSecondaryInt_ == secondary &&
        cachedModifierInt_ == modifier;
    if (!positionsMatchCurrentScale_()) {
        positionLabels_();
    }
    if (valuesUnchanged) return;

    cachedValueKind_ = CachedValueKind::Integer;
    cachedPrimaryInt_ = primary;
    cachedSecondaryInt_ = secondary;
    cachedModifierInt_ = modifier;
    cachedSuffix_.clear();

    primaryLabel_.setText(std::to_string(primary));
    primaryLabel_.setPosition(x_ + primaryOffset_, y_ + valueRowOffset_);

    if (secondary > 0) {
        secondaryLabel_.setText("+(" + std::to_string(secondary) + ")");
    } else if (secondary < 0) {
        secondaryLabel_.setText("(" + std::to_string(secondary) + ")");
    } else {
        secondaryLabel_.setText("");
    }
    secondaryLabel_.setPosition(x_ + secondaryOffset_, y_ + valueRowOffset_);

    if (modifier > 0) {
        modifierLabel_.setText("+(" + std::to_string(modifier) + ")");
        modifierLabel_.setColor(sf::Color(80, 220, 80));
    } else if (modifier < 0) {
        modifierLabel_.setText("(" + std::to_string(modifier) + ")");
        modifierLabel_.setColor(negativeModifierIsBad_ ? sf::Color(230, 80, 80)
                                                       : sf::Color(80, 220, 80));
    } else {
        modifierLabel_.setText("");
        modifierLabel_.setColor(sf::Color(80, 220, 80));
    }
    modifierLabel_.setPosition(x_ + modifierOffset_, y_ + valueRowOffset_);
    totalLabel_.setText(std::to_string(primary + secondary + modifier));
    totalLabel_.setPosition(x_ + totalOffset_, y_ + valueRowOffset_);
}

void ValueBreakdownRow::setValuesFloat(float primary, float secondary,
                                       float modifier,
                                       const std::string& suffix) {
    const bool valuesUnchanged =
        cachedValueKind_ == CachedValueKind::Floating &&
        sameCachedFloatValue(cachedPrimaryFloat_, primary) &&
        sameCachedFloatValue(cachedSecondaryFloat_, secondary) &&
        sameCachedFloatValue(cachedModifierFloat_, modifier) &&
        cachedSuffix_ == suffix;
    if (!positionsMatchCurrentScale_()) {
        positionLabels_();
    }
    if (valuesUnchanged) return;

    cachedValueKind_ = CachedValueKind::Floating;
    cachedPrimaryFloat_ = primary;
    cachedSecondaryFloat_ = secondary;
    cachedModifierFloat_ = modifier;
    cachedSuffix_ = suffix;

    // Format with 1 decimal place
    auto fmt = [&](float v) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.1f", v);
        return std::string(buf) + suffix;
    };
    primaryLabel_.setText(fmt(primary));
    primaryLabel_.setPosition(x_ + primaryOffset_, y_ + valueRowOffset_);

    if (secondary > 0.001f) {
        secondaryLabel_.setText("+" + fmt(secondary));
    } else if (secondary < -0.001f) {
        secondaryLabel_.setText(fmt(secondary));
    } else {
        secondaryLabel_.setText("");
    }
    secondaryLabel_.setPosition(x_ + secondaryOffset_, y_ + valueRowOffset_);

    if (modifier > 0.001f) {
        modifierLabel_.setText("+" + fmt(modifier));
        modifierLabel_.setColor(sf::Color(80, 220, 80));
    } else if (modifier < -0.001f) {
        modifierLabel_.setText(fmt(modifier));
        modifierLabel_.setColor(negativeModifierIsBad_ ? sf::Color(230, 80, 80)
                                                       : sf::Color(80, 220, 80));
    } else {
        modifierLabel_.setText("");
        modifierLabel_.setColor(sf::Color(80, 220, 80));
    }
    modifierLabel_.setPosition(x_ + modifierOffset_, y_ + valueRowOffset_);

    totalLabel_.setText(fmt(primary + secondary + modifier));
    totalLabel_.setPosition(x_ + totalOffset_, y_ + valueRowOffset_);
}

void ValueBreakdownRow::setNegativeModifierIsBad(bool isBad) {
    if (negativeModifierIsBad_ == isBad) return;
    negativeModifierIsBad_ = isBad;
    // The next value update must refresh the cached negative modifier colour.
    cachedValueKind_ = CachedValueKind::None;
}

void ValueBreakdownRow::setLayout(float primaryOffset,
                                  float secondaryOffset,
                                  float modifierOffset,
                                  float totalOffset,
                                  float valueRowOffset) {
    const float normalizedValueRowOffset = std::max(0.0f, valueRowOffset);
    if (primaryOffset_ == primaryOffset &&
        secondaryOffset_ == secondaryOffset &&
        modifierOffset_ == modifierOffset &&
        totalOffset_ == totalOffset &&
        valueRowOffset_ == normalizedValueRowOffset &&
        positionsMatchCurrentScale_()) {
        return;
    }

    primaryOffset_ = primaryOffset;
    secondaryOffset_ = secondaryOffset;
    modifierOffset_ = modifierOffset;
    totalOffset_ = totalOffset;
    valueRowOffset_ = normalizedValueRowOffset;
    positionLabels_();
}

void ValueBreakdownRow::setPosition(float x, float y) {
    if (x_ == x && y_ == y && positionsMatchCurrentScale_()) return;
    x_ = x;
    y_ = y;
    positionLabels_();
}

bool ValueBreakdownRow::positionsMatchCurrentScale_() const {
    return positionedUIScaleFactor_ == getUIScaleFactor() &&
           positionedRenderScaleFactor_ == getTextScaleFactor();
}

void ValueBreakdownRow::positionLabels_() {
    nameLabel_.setPosition(x_, y_);
    primaryLabel_.setPosition(x_ + primaryOffset_, y_ + valueRowOffset_);
    secondaryLabel_.setPosition(x_ + secondaryOffset_, y_ + valueRowOffset_);
    modifierLabel_.setPosition(x_ + modifierOffset_, y_ + valueRowOffset_);
    totalLabel_.setPosition(x_ + totalOffset_, y_ + valueRowOffset_);
    positionedUIScaleFactor_ = getUIScaleFactor();
    positionedRenderScaleFactor_ = getTextScaleFactor();
}

sf::FloatRect ValueBreakdownRow::getRowBounds(float width, float height) const {
    return sf::FloatRect({x_, y_}, {std::max(0.0f, width), std::max(0.0f, height)});
}

void ValueBreakdownRow::draw(sf::RenderWindow& window) const {
    nameLabel_.draw(window);
    primaryLabel_.draw(window);
    secondaryLabel_.draw(window);
    modifierLabel_.draw(window);
    totalLabel_.draw(window);
}

} // namespace RenUI
