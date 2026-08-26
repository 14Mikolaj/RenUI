// Window.cpp
// Container & overlay widgets:
//   Tooltip            — auto-sized hover popup with multi-line/coloured text
//   DraggableWindow    — titled panel with drag-by-header + close button
//   ScrollableList     — virtual scroll viewport with scroll-bar drag

#include <RenUI/RenUI.hpp>
#include <algorithm>
#include <cmath>

namespace RenUI {

namespace {
std::uint64_t gNextWindowLayeringPriority = 0;

sf::Vector2f clampWindowSize(const sf::Vector2f& size, float headerHeight,
                             UIAnchor anchor) {
    const sf::FloatRect visible = getVisibleUILogicalRect(anchor);
    const float minimumHeight = std::clamp(
        headerHeight, 0.0f, visible.size.y);
    return {
        std::clamp(size.x, 0.0f, visible.size.x),
        std::clamp(size.y, minimumHeight, visible.size.y)
    };
}

struct TooltipLayout {
    float contentLeft = 0.0f;
    float contentRight = 0.0f;
    float panelWidth = 0.0f;
    float panelHeight = 0.0f;
};

float tooltipLineAdvance(const TooltipLine& line) {
    const Theme theme = getTheme();
    const unsigned int authoredSize = static_cast<unsigned int>(
        line.isTitle ? theme.tooltipTitleFontSize : theme.tooltipBodyFontSize);
    return static_cast<float>(
        std::max(authoredSize, getScaledTextLogicalSize(authoredSize)) + 4u);
}

TooltipLayout measureTooltipLayout(const std::vector<TooltipLine>& lines,
                                   const sf::Font& regularFont,
                                   const sf::Font& boldFont) {
    TooltipLayout layout;
    const Theme theme = getTheme();
    layout.panelHeight = theme.tooltipPadding * 2.0f;

    const int titleFontSize = static_cast<int>(theme.tooltipTitleFontSize);
    const int bodyFontSize = static_cast<int>(theme.tooltipBodyFontSize);

    for (const TooltipLine& line : lines) {
        if (line.text.empty()) {
            layout.panelHeight += 6.0f;
            continue;
        }

        const int fontSize = line.isTitle ? titleFontSize : bodyFontSize;
        // drawRainbowText always renders bold, even for a non-title line.
        const bool useBold = line.isTitle || line.isRainbow;
        const sf::Font& lineFont = useBold ? boldFont : regularFont;
        sf::Text measure(lineFont, line.text, fontSize);
        if (useBold) {
            measure.setStyle(sf::Text::Bold);
        }
        applyCrispText(measure, fontSize);

        const sf::FloatRect bounds = getScaledLocalBounds(measure);
        layout.contentLeft = std::min(layout.contentLeft, bounds.position.x);
        layout.contentRight = std::max(layout.contentRight,
                                       bounds.position.x + bounds.size.x);
        layout.panelHeight += tooltipLineAdvance(line);
    }

    // Preserve padding around the actual text extents, including glyphs
    // whose local bounds begin before the text origin. Round outward and
    // leave one pixel for text-position snapping at fractional UI scales.
    layout.panelWidth = std::ceil(
        (layout.contentRight - layout.contentLeft) + theme.tooltipPadding * 2.0f + 1.0f);
    return layout;
}
}

// ═══════════════════════════════════════════════════════════════════
//  TOOLTIP
// ═══════════════════════════════════════════════════════════════════

Tooltip::Tooltip() = default;

void Tooltip::setLines(const std::vector<TooltipLine>& lines) { lines_ = lines; }

void Tooltip::setPosition(const sf::Vector2f& mousePos) {
    position_ = mousePos;
}

void Tooltip::setClampSize(const sf::Vector2f& size) { clampSize_ = size; }

void Tooltip::clear() { lines_.clear(); }

sf::Vector2f Tooltip::getPanelSize() const {
    if (lines_.empty()) return {};

    const sf::Font* regularFont = getRegularFont();
    const sf::Font* boldFont = getBoldFont();
    if (!regularFont) return {};
    if (!boldFont) boldFont = regularFont;

    const TooltipLayout layout = measureTooltipLayout(lines_, *regularFont, *boldFont);
    return {layout.panelWidth, layout.panelHeight};
}

void Tooltip::draw(sf::RenderWindow& window) const {
    if (lines_.empty()) return;
    const sf::Font* regularFont = getRegularFont();
    const sf::Font* boldFont = getBoldFont();
    if (!regularFont) return;
    if (!boldFont) boldFont = regularFont;

    const TooltipLayout layout = measureTooltipLayout(lines_, *regularFont, *boldFont);
    const Theme theme = getTheme();
    const sf::Vector2f logicalSize = getConfig().logicalSize;
    const float maxWidth = layout.panelWidth;
    const float totalHeight = layout.panelHeight;
    const int titleFontSize = static_cast<int>(theme.tooltipTitleFontSize);
    const int bodyFontSize = static_cast<int>(theme.tooltipBodyFontSize);

    // Position inside the logical portion of the active view that is actually
    // visible on screen. This matters for anchored UI views above 100%, whose
    // authored 1920x1080 canvas is clipped at one or more edges. Tool windows
    // may retain the explicit zero-origin clamp-size override.
    sf::FloatRect clampRect(
        {0.0f, 0.0f},
        {
            clampSize_.x > 0.f ? clampSize_.x : logicalSize.x,
            clampSize_.y > 0.f ? clampSize_.y : logicalSize.y
        });
    if (clampSize_.x <= 0.f && clampSize_.y <= 0.f) {
        const sf::Vector2u windowSize = window.getSize();
        if (windowSize.x > 0 && windowSize.y > 0) {
            const sf::View& view = window.getView();
            const sf::Vector2f topLeft = window.mapPixelToCoords({0, 0}, view);
            const sf::Vector2f bottomRight = window.mapPixelToCoords(
                {static_cast<int>(windowSize.x), static_cast<int>(windowSize.y)}, view);
            const float visibleLeft = std::max(0.0f, std::min(topLeft.x, bottomRight.x));
            const float visibleTop = std::max(0.0f, std::min(topLeft.y, bottomRight.y));
            const float visibleRight = std::min(
                logicalSize.x, std::max(topLeft.x, bottomRight.x));
            const float visibleBottom = std::min(
                logicalSize.y, std::max(topLeft.y, bottomRight.y));
            clampRect = sf::FloatRect(
                {visibleLeft, visibleTop},
                {std::max(0.0f, visibleRight - visibleLeft),
                 std::max(0.0f, visibleBottom - visibleTop)});
        }
    }
    const float clampLeft = clampRect.position.x;
    const float clampTop = clampRect.position.y;
    const float clampRight = clampLeft + clampRect.size.x;
    const float clampBottom = clampTop + clampRect.size.y;
    float tx = position_.x + 16.0f;
    float ty = position_.y + 16.0f;
    if (tx + maxWidth > clampRight) tx = position_.x - maxWidth - 8.0f;
    if (ty + totalHeight > clampBottom) ty = clampBottom - totalHeight - 4.0f;
    if (tx < clampLeft + 4.0f) tx = clampLeft + 4.0f;
    if (ty < clampTop + 4.0f) ty = clampTop + 4.0f;

    // Background
    sf::RectangleShape bg(sf::Vector2f(maxWidth, totalHeight));
    bg.setPosition({tx, ty});
    bg.setFillColor(theme.panelBackground);
    bg.setOutlineColor(theme.panelBorder);
    bg.setOutlineThickness(1.0f);
    window.draw(bg);

    // Draw lines
    const float contentX = tx + theme.tooltipPadding - layout.contentLeft;
    float curY = ty + theme.tooltipPadding;
    for (size_t i = 0; i < lines_.size(); ++i) {
        if (lines_[i].text.empty()) {
            curY += 6.0f;
            continue;
        }
        int fontSize = lines_[i].isTitle ? titleFontSize : bodyFontSize;
        if (lines_[i].isRainbow) {
            drawRainbowText(window, lines_[i].text,
                            contentX, curY,
                            static_cast<unsigned int>(fontSize));
        } else {
            const bool useBold = lines_[i].isTitle || lines_[i].isRainbow;
            const sf::Font* lineFont = useBold ? boldFont : regularFont;
            sf::Text lineText(*lineFont, lines_[i].text, fontSize);
            if (useBold) {
                lineText.setStyle(sf::Text::Bold);
            }
            applyCrispText(lineText, fontSize);
            lineText.setFillColor(lines_[i].color);
            lineText.setPosition(snapToTextPixelGrid({contentX, curY}));
            window.draw(lineText);
        }
        curY += tooltipLineAdvance(lines_[i]);
    }
}

// ═══════════════════════════════════════════════════════════════════
//  DRAGGABLE WINDOW
// ═══════════════════════════════════════════════════════════════════

DraggableWindow::DraggableWindow() = default;

DraggableWindow::DraggableWindow(const std::string& title, const sf::Vector2f& defaultPos,
                                 const sf::Vector2f& size, float headerHeight)
    : defaultPos_(defaultPos), title_(title),
      panelWidth_(size.x), panelHeight_(size.y),
      headerHeight_(headerHeight > 0.0f ? headerHeight : getTheme().windowHeaderHeight) {}

void DraggableWindow::toggle() {
    open_ = !open_;
    if (open_) {
        scrollOffset_ = 0.0f;
        dragging_ = false;
        if (!positionInitialized_) {
            panelX_ = defaultPos_.x;
            panelY_ = defaultPos_.y;
            positionInitialized_ = true;
        }
        clampToVisibleBounds_();
        bringToFront();
    }
}

void DraggableWindow::open() {
    if (!open_) toggle();
}

void DraggableWindow::close() {
    open_ = false;
    dragging_ = false;
}

void DraggableWindow::setTitle(const std::string& title) { title_ = title; }

void DraggableWindow::setUIAnchor(UIAnchor anchor) {
    uiAnchor_ = anchor;
    // Preserve the authored default center until a responsive caller has had
    // the opportunity to resize the unopened panel. open() performs the
    // final clamp, while already-initialized windows clamp immediately.
    if (positionInitialized_) clampToVisibleBounds_();
}

void DraggableWindow::clampToVisibleBounds_() {
    const sf::FloatRect visible = getVisibleUILogicalRect(uiAnchor_);
    const float maximumX = std::max(visible.position.x,
        visible.position.x + visible.size.x - panelWidth_);
    const float maximumY = std::max(visible.position.y,
        visible.position.y + visible.size.y - panelHeight_);

    if (positionInitialized_) {
        panelX_ = std::clamp(panelX_, visible.position.x, maximumX);
        panelY_ = std::clamp(panelY_, visible.position.y, maximumY);
    } else {
        defaultPos_.x = std::clamp(defaultPos_.x, visible.position.x, maximumX);
        defaultPos_.y = std::clamp(defaultPos_.y, visible.position.y, maximumY);
    }
}

void DraggableWindow::setSize(const sf::Vector2f& size) {
    const sf::Vector2f previousSize{panelWidth_, panelHeight_};
    const sf::Vector2f nextSize = clampWindowSize(
        size, headerHeight_, uiAnchor_);
    const sf::Vector2f centerOffset = (previousSize - nextSize) * 0.5f;

    if (positionInitialized_) {
        panelX_ += centerOffset.x;
        panelY_ += centerOffset.y;
    } else {
        defaultPos_ += centerOffset;
    }

    panelWidth_ = nextSize.x;
    panelHeight_ = nextSize.y;
    clampToVisibleBounds_();
}

void DraggableWindow::setSizeKeepingTopLeft(const sf::Vector2f& size) {
    const sf::Vector2f nextSize = clampWindowSize(
        size, headerHeight_, uiAnchor_);
    panelWidth_ = nextSize.x;
    panelHeight_ = nextSize.y;
    clampToVisibleBounds_();
}

void DraggableWindow::bringToFront() {
    layeringPriority_ = ++gNextWindowLayeringPriority;
}

bool DraggableWindow::handleEvent(const sf::Event& event, const sf::RenderWindow& window) {
    if (!open_) return false;

    clampToVisibleBounds_();

    sf::View uiView = makeUIView(uiAnchor_);
    sf::Vector2f mousePos = window.mapPixelToCoords(
        (logicalMouseOverride_.x >= 0) ? logicalMouseOverride_ : sf::Mouse::getPosition(window), uiView);

    float padding = getTheme().windowPadding;
    sf::FloatRect closeBtnRect({panelX_ + panelWidth_ - padding - CLOSE_BTN_SIZE,
                                panelY_ + padding}, {CLOSE_BTN_SIZE, CLOSE_BTN_SIZE});
    sf::FloatRect titleBarRect({panelX_, panelY_}, {panelWidth_, headerHeight_});
    sf::FloatRect panelRect({panelX_, panelY_}, {panelWidth_, panelHeight_});

    if (auto* mb = event.getIf<sf::Event::MouseButtonPressed>(); mb && mb->button == sf::Mouse::Button::Left) {
        if (panelRect.contains(mousePos)) {
            bringToFront();
        }
        if (closeBtnRect.contains(mousePos)) {
            close();
            return true;
        }
        if (titleBarRect.contains(mousePos)) {
            dragging_ = true;
            dragOffset_ = sf::Vector2f(mousePos.x - panelX_, mousePos.y - panelY_);
            return true;
        }
        if (panelRect.contains(mousePos)) {
            return true;
        }
    }

    if (auto* mb = event.getIf<sf::Event::MouseButtonReleased>(); mb && mb->button == sf::Mouse::Button::Left) {
        if (dragging_) {
            dragging_ = false;
            return true;
        }
    }

    if (event.getIf<sf::Event::MouseMoved>() && dragging_) {
        panelX_ = mousePos.x - dragOffset_.x;
        panelY_ = mousePos.y - dragOffset_.y;
        clampToVisibleBounds_();
        return true;
    }

    if (auto* mws = event.getIf<sf::Event::MouseWheelScrolled>()) {
        if (panelRect.contains(mousePos)) {
            return handleScroll(mws->delta, mousePos);
        }
    }

    return false;
}

bool DraggableWindow::handleScroll(float delta, const sf::Vector2f& /*mousePos*/) {
    scrollOffset_ -= delta * getTheme().windowScrollSpeed;
    scrollOffset_ = std::max(0.0f, std::min(scrollOffset_, maxScroll_));
    return true;
}

void DraggableWindow::drawChrome(sf::RenderWindow& window) {
    if (!open_) return;

    clampToVisibleBounds_();

    sf::View uiView = makeUIView(uiAnchor_);
    window.setView(uiView);

    sf::Vector2f mousePos = window.mapPixelToCoords(
        (logicalMouseOverride_.x >= 0) ? logicalMouseOverride_ : sf::Mouse::getPosition(window), uiView);

    const Theme theme = getTheme();
    float padding = theme.windowPadding;

    // Panel background
    Panel panel(sf::Vector2f(panelX_, panelY_), sf::Vector2f(panelWidth_, panelHeight_),
                theme.windowBackground, theme.windowBorder);
    panel.draw(window);

    // Title
    if (!title_.empty()) {
        Label title(title_, theme.windowTitleFontSize, theme.textPrimary, true);
        title.setPosition(panelX_ + padding, panelY_ + padding);
        title.draw(window);
    }

    // Close button
    {
        sf::FloatRect closeBtnRect({panelX_ + panelWidth_ - padding - CLOSE_BTN_SIZE,
                                    panelY_ + padding},
                                   {CLOSE_BTN_SIZE, CLOSE_BTN_SIZE});
        drawCloseWindowButton(window, closeBtnRect, closeBtnRect.contains(mousePos));
    }
}

sf::Vector2f DraggableWindow::getContentPosition() const {
    return sf::Vector2f(panelX_ + getTheme().windowPadding, panelY_ + headerHeight_);
}

sf::Vector2f DraggableWindow::getContentSize() const {
    const float padding = getTheme().windowPadding;
    return sf::Vector2f(panelWidth_ - padding * 2.0f,
                        panelHeight_ - headerHeight_ - padding);
}

sf::FloatRect DraggableWindow::getPanelBounds() const {
    return sf::FloatRect({panelX_, panelY_}, {panelWidth_, panelHeight_});
}

sf::Vector2f DraggableWindow::getPanelPosition() const {
    return sf::Vector2f(panelX_, panelY_);
}

sf::Vector2f DraggableWindow::getPanelSize() const {
    return sf::Vector2f(panelWidth_, panelHeight_);
}

// ═══════════════════════════════════════════════════════════════════
//  SCROLLABLE LIST
// ═══════════════════════════════════════════════════════════════════

ScrollableList::ScrollableList() = default;

ScrollableList::ScrollableList(const sf::Vector2f& position, const sf::Vector2f& size, float rowHeight)
    : position_(position), size_(size), rowHeight_(rowHeight) {}

void ScrollableList::setItemCount(int count) {
    itemCount_ = count;
    recalcMaxScroll();
}

void ScrollableList::setPosition(const sf::Vector2f& pos) { position_ = pos; }
void ScrollableList::setSize(const sf::Vector2f& size) {
    size_ = size;
    recalcMaxScroll();
}
sf::Vector2f ScrollableList::getPosition() const { return position_; }
sf::Vector2f ScrollableList::getSize() const { return size_; }

bool ScrollableList::handleScroll(float delta, const sf::Vector2f& mousePos) {
    if (!contains(mousePos)) return false;
    scrollOffset_ -= delta * getTheme().windowScrollSpeed;
    scrollOffset_ = std::max(0.0f, std::min(scrollOffset_, maxScroll_));
    return true;
}

int ScrollableList::getFirstVisibleRow() const {
    return static_cast<int>(scrollOffset_ / rowHeight_);
}

int ScrollableList::getLastVisibleRow() const {
    return std::min(itemCount_ - 1, static_cast<int>((scrollOffset_ + size_.y) / rowHeight_));
}

float ScrollableList::getRowY(int index) const {
    return position_.y + static_cast<float>(index) * rowHeight_ - scrollOffset_;
}

bool ScrollableList::isRowVisible(float rowY) const {
    return (rowY + rowHeight_ >= position_.y) && (rowY < position_.y + size_.y);
}

void ScrollableList::drawScrollBar(sf::RenderWindow& window) const {
    if (maxScroll_ <= 0.0f) return;
    const Theme theme = getTheme();
    float barH = size_.y * (size_.y / (size_.y + maxScroll_));
    barH = std::max(barH, 20.0f);
    float barY = position_.y + (scrollOffset_ / maxScroll_) * (size_.y - barH);

    // Track background
    sf::RectangleShape track(sf::Vector2f(SCROLLBAR_WIDTH, size_.y));
    track.setPosition({position_.x + size_.x + 2.0f, position_.y});
    track.setFillColor(theme.panelBackground);
    window.draw(track);

    // Thumb
    sf::RectangleShape scrollBar(sf::Vector2f(SCROLLBAR_WIDTH, barH));
    scrollBar.setPosition({position_.x + size_.x + 2.0f, barY});
    scrollBar.setFillColor(draggingScrollBar_ ? theme.buttonHover : theme.textMuted);
    window.draw(scrollBar);
}

bool ScrollableList::handleScrollBarEvent(const sf::Event& event, const sf::Vector2f& mousePos) {
    if (maxScroll_ <= 0.0f) return false;

    float barH = size_.y * (size_.y / (size_.y + maxScroll_));
    barH = std::max(barH, 20.0f);
    float barY = position_.y + (scrollOffset_ / maxScroll_) * (size_.y - barH);
    float barX = position_.x + size_.x + 2.0f;

    if (auto* mb = event.getIf<sf::Event::MouseButtonPressed>(); mb && mb->button == sf::Mouse::Button::Left) {
        // Click on thumb → start dragging
        sf::FloatRect thumbRect({barX, barY}, {SCROLLBAR_WIDTH, barH});
        if (thumbRect.contains(mousePos)) {
            draggingScrollBar_ = true;
            scrollBarGrabOffset_ = mousePos.y - barY;
            return true;
        }
        // Click on track → jump to position
        sf::FloatRect trackRect({barX, position_.y}, {SCROLLBAR_WIDTH, size_.y});
        if (trackRect.contains(mousePos)) {
            float ratio = (mousePos.y - position_.y - barH * 0.5f) / (size_.y - barH);
            ratio = std::max(0.0f, std::min(1.0f, ratio));
            scrollOffset_ = ratio * maxScroll_;
            draggingScrollBar_ = true;
            scrollBarGrabOffset_ = barH * 0.5f;
            return true;
        }
    }

    if (auto* mb = event.getIf<sf::Event::MouseButtonReleased>(); mb && mb->button == sf::Mouse::Button::Left) {
        if (draggingScrollBar_) {
            draggingScrollBar_ = false;
            return true;
        }
    }

    if (event.getIf<sf::Event::MouseMoved>() && draggingScrollBar_) {
        float barHCur = size_.y * (size_.y / (size_.y + maxScroll_));
        barHCur = std::max(barHCur, 20.0f);
        float newBarY = mousePos.y - scrollBarGrabOffset_;
        float ratio = (newBarY - position_.y) / (size_.y - barHCur);
        ratio = std::max(0.0f, std::min(1.0f, ratio));
        scrollOffset_ = ratio * maxScroll_;
        return true;
    }

    return false;
}

bool ScrollableList::contains(const sf::Vector2f& point) const {
    sf::FloatRect rect({position_.x, position_.y}, {size_.x, size_.y});
    return rect.contains(point);
}

int ScrollableList::getHoveredRow(const sf::Vector2f& mousePos) const {
    if (!contains(mousePos)) return -1;
    float relY = mousePos.y - position_.y + scrollOffset_;
    int row = static_cast<int>(relY / rowHeight_);
    if (row < 0 || row >= itemCount_) return -1;
    return row;
}

void ScrollableList::recalcMaxScroll() {
    float contentH = static_cast<float>(itemCount_) * rowHeight_;
    maxScroll_ = std::max(0.0f, contentH - size_.y);
    scrollOffset_ = std::min(scrollOffset_, maxScroll_);
}

// ═══════════════════════════════════════════════════════════════════
//  END OF GENERIC WINDOW CONTROLS
// ═══════════════════════════════════════════════════════════════════

} // namespace RenUI
