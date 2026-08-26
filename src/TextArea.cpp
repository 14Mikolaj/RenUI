// TextArea.cpp
// Multi-line editable text area — word-wrapping, auto-grow, mouse +
// keyboard selection (light-blue highlight), clipboard. Built from the
// same primitives as UITextInput; shares getRegularFont() and
// getTextSelectionFillColor() for cross-widget consistency.

#include <RenUI/RenUI.hpp>
#include <SFML/Window/Clipboard.hpp>
#include <algorithm>
#include <cstdint>

namespace RenUI {

namespace {

// ASCII-only for now — matches UITextInput. Word-breakable whitespace.
constexpr bool isBreakable(char c) {
    return c == ' ' || c == '\t';
}

}  // namespace

// ── Construction ──────────────────────────────────────────────────

UITextArea::UITextArea() {
    layoutDirty_ = true;
}

UITextArea::UITextArea(const sf::Vector2f& position, float width,
                       float minHeight, float maxHeight, unsigned int fontSize)
    : position_(position),
      width_(width),
      minHeight_(minHeight),
      maxHeight_(maxHeight),
      fontSize_(fontSize) {
    currentHeight_ = minHeight_;
    layoutDirty_ = true;
}

// ── Layout ────────────────────────────────────────────────────────

bool UITextArea::layoutNeedsRebuild_() const {
    return layoutDirty_ || textMetricsRevision_ != getTextMetricsRevision();
}

void UITextArea::rebuildLayout_() const {
    textMetricsRevision_ = getTextMetricsRevision();
    lines_.clear();
    const sf::Font* font = getRegularFont();
    if (!font) {
        lines_.push_back({0, content_.size(), false});
        lineHeight_ = static_cast<float>(fontSize_) + 4.0f;
        currentHeight_ = std::max(minHeight_, lineHeight_ + 8.0f);
        layoutDirty_ = false;
        return;
    }

    // Pad the content width a bit to account for caret + padding.
    const float innerW = std::max(20.0f, width_ - 12.0f);

    // Measure a single 'M' at current font size as a rough line-height baseline.
    {
        sf::Text probe(*font, "Mg", fontSize_);
        applyCrispText(probe, fontSize_);
        lineHeight_ = std::max(12.0f, getScaledLocalBounds(probe).size.y + 4.0f);
    }

    auto measureWidth = [&](std::size_t a, std::size_t b) -> float {
        if (b <= a) return 0.0f;
        std::string s = content_.substr(a, b - a);
        sf::Text probe(*font, s, fontSize_);
        applyCrispText(probe, fontSize_);
        return getScaledLocalBounds(probe).size.x;
    };

    std::size_t i = 0;
    const std::size_t n = content_.size();
    while (i <= n) {
        // Consume a "hard" line ending at a newline or EOF.
        std::size_t lineStart = i;
        std::size_t hardEnd = i;
        while (hardEnd < n && content_[hardEnd] != '\n') ++hardEnd;

        // Wrap within [lineStart, hardEnd).
        std::size_t cursor = lineStart;
        while (cursor < hardEnd) {
            // Try to fit as many chars as possible starting at `cursor`.
            // Binary-ish forward scan: extend until we overflow, then
            // back off to the last break-point.
            std::size_t fit = cursor;
            std::size_t lastBreak = std::string::npos;
            while (fit < hardEnd) {
                // Move `fit` forward one char at a time; track last breakable.
                char c = content_[fit];
                if (isBreakable(c)) lastBreak = fit;  // break AFTER space
                ++fit;
                float w = measureWidth(cursor, fit);
                if (w > innerW && fit - cursor > 1) {
                    // Overflow. Prefer break at lastBreak+1 (so space stays on
                    // prev line); else hard-break one char back.
                    std::size_t breakAt;
                    if (lastBreak != std::string::npos && lastBreak + 1 > cursor) {
                        breakAt = lastBreak + 1;  // keep space at end of cur
                    } else {
                        breakAt = fit - 1;        // hard break mid-word
                        if (breakAt == cursor) breakAt = cursor + 1;
                    }
                    lines_.push_back({cursor, breakAt, false});
                    cursor = breakAt;
                    // Skip a single space at the start of the new line
                    // (we keep the breaking space on the previous line).
                    goto next_wrap_iter;
                }
            }
            // Everything from cursor..hardEnd fits.
            lines_.push_back({cursor, hardEnd, false});
            cursor = hardEnd;
        next_wrap_iter:;
        }

        // If the line was empty (two consecutive newlines) still emit a blank line.
        if (lineStart == hardEnd) {
            lines_.push_back({lineStart, lineStart, hardEnd < n /*hardBreak*/});
        } else if (!lines_.empty()) {
            // Mark last line as hardBreak if it ends at newline.
            lines_.back().hardBreak = (hardEnd < n);
        }

        if (hardEnd >= n) break;
        i = hardEnd + 1;  // skip the '\n'
    }

    if (lines_.empty()) {
        lines_.push_back({0, 0, false});
    }

    const float contentH = lines_.size() * lineHeight_ + 8.0f;
    float h = std::max(minHeight_, contentH);
    if (maxHeight_ > 0.0f) h = std::min(h, maxHeight_);
    currentHeight_ = h;
    // Clamp scroll.
    scrollOffset_ = std::max(0.0f, std::min(scrollOffset_, maxScroll_()));
    layoutDirty_ = false;
}

float UITextArea::maxScroll_() const {
    const float contentH = lines_.size() * lineHeight_ + 8.0f;
    const float visibleH = (maxHeight_ > 0.0f) ? std::min(currentHeight_, maxHeight_) : currentHeight_;
    return std::max(0.0f, contentH - visibleH);
}

std::size_t UITextArea::lineIndexOf_(std::size_t byte) const {
    // Find the wrapped line that `byte` belongs to. If byte sits at a
    // wrap boundary we prefer the START of the next line (so typing at
    // column 0 looks natural).
    if (lines_.empty()) return 0;
    for (std::size_t i = 0; i < lines_.size(); ++i) {
        const auto& L = lines_[i];
        if (byte >= L.startByte && byte <= L.endByte) {
            // If byte == endByte and this isn't the last line AND the next
            // line begins exactly here (soft-wrap boundary), prefer next.
            if (i + 1 < lines_.size() && byte == L.endByte &&
                !L.hardBreak && lines_[i + 1].startByte == byte) {
                return i + 1;
            }
            return i;
        }
    }
    return lines_.size() - 1;
}

sf::Vector2f UITextArea::caretPixel_(std::size_t index) const {
    const sf::Font* font = getRegularFont();
    if (!font || lines_.empty()) return position_;
    std::size_t li = lineIndexOf_(std::min(index, content_.size()));
    const auto& L = lines_[li];
    std::string prefix = content_.substr(L.startByte, std::min(index, L.endByte) - L.startByte);
    sf::Text probe(*font, prefix, fontSize_);
    applyCrispText(probe, fontSize_);
    float x = position_.x + 6.0f + getScaledLocalBounds(probe).size.x;
    float y = position_.y + 4.0f + li * lineHeight_ - scrollOffset_;
    return {x, y};
}

std::size_t UITextArea::indexFromMouse_(const sf::Vector2f& mouse) const {
    if (layoutNeedsRebuild_()) rebuildLayout_();
    if (lines_.empty()) return 0;

    const sf::Font* font = getRegularFont();
    if (!font) return 0;

    // Which line?
    float relY = mouse.y - (position_.y + 4.0f) + scrollOffset_;
    int li = static_cast<int>(std::floor(relY / lineHeight_));
    li = std::max(0, std::min(li, static_cast<int>(lines_.size()) - 1));
    const auto& L = lines_[li];

    std::string s = content_.substr(L.startByte, L.endByte - L.startByte);
    if (s.empty()) return L.startByte;

    sf::Text probe(*font, s, fontSize_);
    applyCrispText(probe, fontSize_);
    float baseX = position_.x + 6.0f;

    float prevX = baseX + probe.findCharacterPos(0).x;
    if (mouse.x <= prevX) return L.startByte;
    for (std::size_t i = 1; i <= s.size(); ++i) {
        float x = baseX + probe.findCharacterPos(i).x;
        if (mouse.x < x) {
            return L.startByte + (((mouse.x - prevX) <= (x - mouse.x)) ? (i - 1) : i);
        }
        prevX = x;
    }
    return L.endByte;
}

void UITextArea::ensureCaretVisible_() {
    if (layoutNeedsRebuild_()) rebuildLayout_();
    if (maxHeight_ <= 0.0f) return;  // no scroll
    const std::size_t li = lineIndexOf_(cursorPos_);
    float caretTop = li * lineHeight_;
    float caretBot = caretTop + lineHeight_;
    float viewTop = scrollOffset_;
    float viewBot = viewTop + maxHeight_ - 8.0f;
    if (caretTop < viewTop) scrollOffset_ = caretTop;
    else if (caretBot > viewBot) scrollOffset_ = caretBot - (maxHeight_ - 8.0f);
    scrollOffset_ = std::max(0.0f, std::min(scrollOffset_, maxScroll_()));
}

// ── Cursor motion ─────────────────────────────────────────────────

void UITextArea::moveCursorHorizontal_(int dx, bool extendSelection) {
    std::size_t next = cursorPos_;
    if (dx < 0 && next > 0) --next;
    else if (dx > 0 && next < content_.size()) ++next;

    if (extendSelection) {
        if (!selectionAnchor_) selectionAnchor_ = cursorPos_;
        cursorPos_ = next;
        selectionStart_ = std::min(*selectionAnchor_, cursorPos_);
        selectionEnd_ = std::max(*selectionAnchor_, cursorPos_);
        if (!hasSelection_()) selectionAnchor_.reset();
    } else {
        if (hasSelection_()) {
            cursorPos_ = (dx < 0) ? selectionLeft_() : selectionRight_();
        } else {
            cursorPos_ = next;
        }
        clearSelection_();
    }
    ensureCaretVisible_();
}

void UITextArea::moveCursorVertical_(int dy, bool extendSelection) {
    if (layoutNeedsRebuild_()) rebuildLayout_();
    if (lines_.empty()) return;
    std::size_t li = lineIndexOf_(cursorPos_);
    int target = static_cast<int>(li) + dy;
    target = std::max(0, std::min(target, static_cast<int>(lines_.size()) - 1));
    if (static_cast<std::size_t>(target) == li) return;

    // Preserve X column: measure current caret X relative to line start.
    const sf::Font* font = getRegularFont();
    float targetX = 0.0f;
    if (font) {
        const auto& cur = lines_[li];
        sf::Text probe(*font, content_.substr(cur.startByte, cursorPos_ - cur.startByte),
                       fontSize_);
        applyCrispText(probe, fontSize_);
        targetX = getScaledLocalBounds(probe).size.x;
    }

    const auto& L = lines_[target];
    std::string s = content_.substr(L.startByte, L.endByte - L.startByte);
    std::size_t best = L.startByte;
    if (font && !s.empty()) {
        sf::Text probe(*font, s, fontSize_);
        applyCrispText(probe, fontSize_);
        float prevX = 0.0f;
        best = L.endByte;
        for (std::size_t i = 1; i <= s.size(); ++i) {
            float x = probe.findCharacterPos(i).x;
            if (targetX < x) {
                best = L.startByte + (((targetX - prevX) <= (x - targetX)) ? (i - 1) : i);
                break;
            }
            prevX = x;
        }
    }

    std::size_t next = best;
    if (extendSelection) {
        if (!selectionAnchor_) selectionAnchor_ = cursorPos_;
        cursorPos_ = next;
        selectionStart_ = std::min(*selectionAnchor_, cursorPos_);
        selectionEnd_ = std::max(*selectionAnchor_, cursorPos_);
        if (!hasSelection_()) selectionAnchor_.reset();
    } else {
        cursorPos_ = next;
        clearSelection_();
    }
    ensureCaretVisible_();
}

void UITextArea::clearSelection_() {
    selectionStart_ = cursorPos_;
    selectionEnd_ = cursorPos_;
    selectionAnchor_.reset();
}

void UITextArea::eraseSelection_() {
    if (!hasSelection_()) return;
    std::size_t left = std::min(selectionLeft_(), content_.size());
    std::size_t right = std::min(selectionRight_(), content_.size());
    if (right > left) {
        content_.erase(left, right - left);
    }
    cursorPos_ = left;
    clearSelection_();
    markDirty_();
}

void UITextArea::resetCaretBlink_() {
    caretVisible_ = true;
    caretBlinkClock_.restart();
}

// ── Public API ────────────────────────────────────────────────────

void UITextArea::setText(const std::string& text) {
    content_ = text;
    cursorPos_ = std::min(cursorPos_, content_.size());
    clearSelection_();
    markDirty_();
    resetCaretBlink_();
}

void UITextArea::clear() {
    content_.clear();
    cursorPos_ = 0;
    clearSelection_();
    markDirty_();
    resetCaretBlink_();
}

void UITextArea::setPosition(const sf::Vector2f& pos) { position_ = pos; }

void UITextArea::setWidth(float w) {
    if (std::abs(w - width_) > 0.1f) {
        width_ = w;
        markDirty_();
    }
}
void UITextArea::setMinHeight(float h) { minHeight_ = h; markDirty_(); }
void UITextArea::setMaxHeight(float h) { maxHeight_ = h; markDirty_(); }
void UITextArea::setFontSize(unsigned int s) {
    if (s != fontSize_) { fontSize_ = s; markDirty_(); }
}

sf::FloatRect UITextArea::getBounds() const {
    if (layoutNeedsRebuild_()) rebuildLayout_();
    return sf::FloatRect(position_, {width_, currentHeight_});
}

float UITextArea::getHeight() const {
    if (layoutNeedsRebuild_()) rebuildLayout_();
    return currentHeight_;
}

bool UITextArea::contains(const sf::Vector2f& p) const {
    return getBounds().contains(p);
}

void UITextArea::setActive(bool a) {
    active_ = a;
    if (active_) {
        cursorPos_ = std::min(cursorPos_, content_.size());
        resetCaretBlink_();
    } else {
        mouseSelecting_ = false;
        selectionAnchor_.reset();
    }
}

// ── Rendering ─────────────────────────────────────────────────────

void UITextArea::draw(sf::RenderWindow& window) const {
    if (layoutNeedsRebuild_()) rebuildLayout_();
    const Theme theme = getTheme();

    // Background box
    sf::RectangleShape box(sf::Vector2f(width_, currentHeight_));
    box.setPosition(position_);
    box.setFillColor(active_ ? theme.inputActiveBackground : theme.inputBackground);
    box.setOutlineColor(active_ ? theme.inputActiveBorder : theme.inputBorder);
    box.setOutlineThickness(1.0f);
    window.draw(box);

    const sf::Font* font = getRegularFont();
    if (!font) return;

    // Placeholder
    if (content_.empty() && !active_ && !placeholder_.empty()) {
        sf::Text ph(*font, placeholder_, fontSize_);
        applyCrispText(ph, fontSize_);
        ph.setFillColor(theme.textMuted);
        ph.setPosition({position_.x + 6.0f, position_.y + 4.0f});
        window.draw(ph);
        return;
    }

    // Clip when we have a maxHeight and content overflows.
    const bool needsClip = (maxHeight_ > 0.0f) && (maxScroll_() > 0.1f);
    sf::View prevView;
    if (needsClip) {
        prevView = window.getView();
        sf::FloatRect clipRect(position_, {width_, currentHeight_});
        window.setView(makeClippedUIView(clipRect, prevView));
    }

    // Selection highlight (drawn first so text is on top).
    if (active_ && hasSelection_()) {
        const std::size_t selL = selectionLeft_();
        const std::size_t selR = selectionRight_();
        const std::size_t liL = lineIndexOf_(selL);
        const std::size_t liR = lineIndexOf_(selR);
        for (std::size_t i = liL; i <= liR && i < lines_.size(); ++i) {
            const auto& L = lines_[i];
            std::size_t a = std::max(selL, L.startByte);
            std::size_t b = std::min(selR, L.endByte);
            if (b <= a) continue;
            std::string pre = content_.substr(L.startByte, a - L.startByte);
            std::string mid = content_.substr(a, b - a);
            sf::Text preT(*font, pre, fontSize_); applyCrispText(preT, fontSize_);
            sf::Text midT(*font, mid, fontSize_); applyCrispText(midT, fontSize_);
            float x0 = position_.x + 6.0f + getScaledLocalBounds(preT).size.x;
            float w = getScaledLocalBounds(midT).size.x;
            if (w < 1.0f) w = 4.0f;  // at-end-of-line visual hint
            float yLine = position_.y + 4.0f + i * lineHeight_ - scrollOffset_;
            sf::RectangleShape sel(sf::Vector2f(w, lineHeight_));
            sel.setPosition({x0, yLine});
            sel.setFillColor(getTextSelectionFillColor());
            window.draw(sel);
        }
    }

    // Text
    for (std::size_t i = 0; i < lines_.size(); ++i) {
        const auto& L = lines_[i];
        if (L.endByte <= L.startByte) continue;
        float yLine = position_.y + 4.0f + i * lineHeight_ - scrollOffset_;
        if (needsClip) {
            // Quick vertical cull relative to view
            if (yLine + lineHeight_ < position_.y || yLine > position_.y + currentHeight_) continue;
        }
        sf::Text t(*font, content_.substr(L.startByte, L.endByte - L.startByte), fontSize_);
        applyCrispText(t, fontSize_);
        t.setFillColor(theme.textPrimary);
        t.setPosition({position_.x + 6.0f, yLine});
        window.draw(t);
    }

    // Caret
    if (active_) {
        if (caretBlinkClock_.getElapsedTime().asSeconds() >= 0.5f) {
            caretVisible_ = !caretVisible_;
            caretBlinkClock_.restart();
        }
        if (caretVisible_) {
            sf::Vector2f c = caretPixel_(cursorPos_);
            sf::RectangleShape caret(sf::Vector2f(1.5f, lineHeight_ - 2.0f));
            caret.setPosition({c.x, c.y});
            caret.setFillColor(theme.textPrimary);
            window.draw(caret);
        }
    }

    if (needsClip) window.setView(prevView);
}

// ── Events ────────────────────────────────────────────────────────

bool UITextArea::handleEvent(const sf::Event& event) {
    if (auto* mb = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (mb->button == sf::Mouse::Button::Left) {
            sf::Vector2f p(static_cast<float>(mb->position.x),
                            static_cast<float>(mb->position.y));
            if (contains(p)) {
                setActive(true);
                cursorPos_ = indexFromMouse_(p);
                selectionStart_ = cursorPos_;
                selectionEnd_ = cursorPos_;
                selectionAnchor_ = cursorPos_;
                mouseSelecting_ = true;
                resetCaretBlink_();
                return true;
            } else {
                if (active_) setActive(false);
                mouseSelecting_ = false;
                selectionAnchor_.reset();
            }
        }
        return false;
    }

    if (auto* mm = event.getIf<sf::Event::MouseMoved>()) {
        if (active_ && mouseSelecting_) {
            sf::Vector2f p(static_cast<float>(mm->position.x),
                            static_cast<float>(mm->position.y));
            cursorPos_ = indexFromMouse_(p);
            if (selectionAnchor_) {
                selectionStart_ = std::min(*selectionAnchor_, cursorPos_);
                selectionEnd_ = std::max(*selectionAnchor_, cursorPos_);
            }
            resetCaretBlink_();
            return true;
        }
        return false;
    }

    if (auto* mr = event.getIf<sf::Event::MouseButtonReleased>()) {
        if (mr->button == sf::Mouse::Button::Left) {
            bool was = mouseSelecting_;
            mouseSelecting_ = false;
            if (!hasSelection_()) selectionAnchor_.reset();
            return was;
        }
        return false;
    }

    if (auto* mw = event.getIf<sf::Event::MouseWheelScrolled>()) {
        if (active_ && maxHeight_ > 0.0f && maxScroll_() > 0.1f) {
            scrollOffset_ = std::max(0.0f,
                std::min(scrollOffset_ - mw->delta * lineHeight_ * 2.0f, maxScroll_()));
            return true;
        }
        return false;
    }

    if (!active_) return false;

    if (auto* kp = event.getIf<sf::Event::KeyPressed>()) {
        bool ctrl = kp->control;
        bool shift = kp->shift;

        if (ctrl && kp->code == sf::Keyboard::Key::A) {
            if (!content_.empty()) {
                selectionAnchor_ = 0;
                selectionStart_ = 0;
                selectionEnd_ = content_.size();
                cursorPos_ = content_.size();
                resetCaretBlink_();
            }
            return true;
        }
        if (ctrl && kp->code == sf::Keyboard::Key::C) {
            if (hasSelection_())
                sf::Clipboard::setString(
                    content_.substr(selectionLeft_(), selectionRight_() - selectionLeft_()));
            return true;
        }
        if (ctrl && kp->code == sf::Keyboard::Key::X) {
            if (hasSelection_()) {
                sf::Clipboard::setString(
                    content_.substr(selectionLeft_(), selectionRight_() - selectionLeft_()));
                eraseSelection_();
                resetCaretBlink_();
            }
            return true;
        }
        if (ctrl && kp->code == sf::Keyboard::Key::V) {
            std::string clip = sf::Clipboard::getString().toAnsiString();
            std::string cleaned;
            cleaned.reserve(clip.size());
            for (char c : clip) {
                if (c == '\n' || (c >= 32 && c < 127)) cleaned.push_back(c);
            }
            if (!cleaned.empty()) {
                if (hasSelection_()) eraseSelection_();
                content_.insert(cursorPos_, cleaned);
                cursorPos_ += cleaned.size();
                clearSelection_();
                markDirty_();
                resetCaretBlink_();
            }
            return true;
        }

        if (kp->code == sf::Keyboard::Key::Left)  { moveCursorHorizontal_(-1, shift); resetCaretBlink_(); return true; }
        if (kp->code == sf::Keyboard::Key::Right) { moveCursorHorizontal_(+1, shift); resetCaretBlink_(); return true; }
        if (kp->code == sf::Keyboard::Key::Up)    { moveCursorVertical_(-1, shift);   resetCaretBlink_(); return true; }
        if (kp->code == sf::Keyboard::Key::Down)  { moveCursorVertical_(+1, shift);   resetCaretBlink_(); return true; }

        if (kp->code == sf::Keyboard::Key::Home) {
            if (layoutNeedsRebuild_()) rebuildLayout_();
            std::size_t li = lineIndexOf_(cursorPos_);
            std::size_t target = lines_[li].startByte;
            if (shift) {
                if (!selectionAnchor_) selectionAnchor_ = cursorPos_;
                cursorPos_ = target;
                selectionStart_ = std::min(*selectionAnchor_, cursorPos_);
                selectionEnd_ = std::max(*selectionAnchor_, cursorPos_);
                if (!hasSelection_()) selectionAnchor_.reset();
            } else {
                cursorPos_ = target;
                clearSelection_();
            }
            ensureCaretVisible_();
            resetCaretBlink_();
            return true;
        }
        if (kp->code == sf::Keyboard::Key::End) {
            if (layoutNeedsRebuild_()) rebuildLayout_();
            std::size_t li = lineIndexOf_(cursorPos_);
            std::size_t target = lines_[li].endByte;
            if (shift) {
                if (!selectionAnchor_) selectionAnchor_ = cursorPos_;
                cursorPos_ = target;
                selectionStart_ = std::min(*selectionAnchor_, cursorPos_);
                selectionEnd_ = std::max(*selectionAnchor_, cursorPos_);
                if (!hasSelection_()) selectionAnchor_.reset();
            } else {
                cursorPos_ = target;
                clearSelection_();
            }
            ensureCaretVisible_();
            resetCaretBlink_();
            return true;
        }

        if (kp->code == sf::Keyboard::Key::Backspace) {
            if (hasSelection_()) { eraseSelection_(); }
            else if (cursorPos_ > 0) {
                content_.erase(cursorPos_ - 1, 1);
                --cursorPos_;
                clearSelection_();
                markDirty_();
            }
            ensureCaretVisible_();
            resetCaretBlink_();
            return true;
        }
        if (kp->code == sf::Keyboard::Key::Delete) {
            if (hasSelection_()) { eraseSelection_(); }
            else if (cursorPos_ < content_.size()) {
                content_.erase(cursorPos_, 1);
                clearSelection_();
                markDirty_();
            }
            resetCaretBlink_();
            return true;
        }
        if (kp->code == sf::Keyboard::Key::Enter) {
            if (hasSelection_()) eraseSelection_();
            content_.insert(content_.begin() + static_cast<std::ptrdiff_t>(cursorPos_), '\n');
            ++cursorPos_;
            clearSelection_();
            markDirty_();
            ensureCaretVisible_();
            resetCaretBlink_();
            return true;
        }
    }

    if (auto* te = event.getIf<sf::Event::TextEntered>()) {
        std::uint32_t code = te->unicode;
        // Exclude \b, \r, \n (handled as KeyPressed above) and control chars.
        if (code >= 32 && code < 127) {
            if (hasSelection_()) eraseSelection_();
            content_.insert(content_.begin() + static_cast<std::ptrdiff_t>(cursorPos_),
                            static_cast<char>(code));
            ++cursorPos_;
            clearSelection_();
            markDirty_();
            ensureCaretVisible_();
            resetCaretBlink_();
            return true;
        }
    }

    return false;
}

}  // namespace RenUI
