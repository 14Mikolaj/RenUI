// TextInput.cpp
// Implementation of UITextInput — single-line editable text field with
// caret blink, mouse + keyboard selection, clipboard support, and
// password masking. Kept as a focused RenUI implementation unit because this
// component is large enough (~370 lines) to warrant its own translation
// unit.

#include <RenUI/RenUI.hpp>
#include <SFML/Window/Clipboard.hpp>
#include <algorithm>
#include <cstdint>

namespace RenUI {

namespace {

constexpr float INPUT_HORIZONTAL_PADDING = 6.0f;
constexpr float INPUT_VERTICAL_PADDING = 2.0f;
constexpr float INPUT_CARET_WIDTH = 1.5f;
constexpr float INPUT_CARET_MARGIN = 1.0f;

} // namespace

UITextInput::UITextInput() {
    const Theme theme = getTheme();
    box_.setFillColor(theme.inputBackground);
    box_.setOutlineColor(theme.inputBorder);
    box_.setOutlineThickness(1);
    updateTextPosition_();
}

UITextInput::UITextInput(const sf::Vector2f& position, const sf::Vector2f& size, bool passwordMode)
    : passwordMode_(passwordMode) {
    const Theme theme = getTheme();
    box_.setSize(size);
    box_.setPosition(position);
    box_.setFillColor(theme.inputBackground);
    box_.setOutlineColor(theme.inputBorder);
    box_.setOutlineThickness(1);
    updateTextPosition_();
}

bool UITextInput::hasSelection_() const {
    return selectionStart_ != selectionEnd_;
}

void UITextInput::clearSelection_() {
    selectionStart_ = cursorPos_;
    selectionEnd_ = cursorPos_;
    selectionAnchor_.reset();
}

std::size_t UITextInput::selectionLeft_() const {
    return std::min(selectionStart_, selectionEnd_);
}

std::size_t UITextInput::selectionRight_() const {
    return std::max(selectionStart_, selectionEnd_);
}

void UITextInput::eraseSelection_() {
    if (!hasSelection_()) return;
    std::size_t left = std::min(selectionLeft_(), content_.size());
    std::size_t right = std::min(selectionRight_(), content_.size());
    if (right > left) {
        content_.erase(left, right - left);
    }
    cursorPos_ = left;
    clearSelection_();
}

void UITextInput::resetCaretBlink_() {
    caretVisible_ = true;
    caretBlinkClock_.restart();
    ensureCaretVisible_();
}

sf::FloatRect UITextInput::getInnerBounds_() const {
    const sf::Vector2f position = box_.getPosition();
    const sf::Vector2f size = box_.getSize();
    const float insetX = std::min(INPUT_HORIZONTAL_PADDING,
                                  std::max(0.0f, size.x * 0.5f));
    const float insetY = std::min(INPUT_VERTICAL_PADDING,
                                  std::max(0.0f, size.y * 0.5f));
    return sf::FloatRect(
        {position.x + insetX, position.y + insetY},
        {std::max(0.001f, size.x - insetX * 2.0f),
         std::max(0.001f, size.y - insetY * 2.0f)});
}

sf::Vector2f UITextInput::getTextPosition_(const std::string& /*displayText*/) const {
    const sf::FloatRect inner = getInnerBounds_();
    float y = inner.position.y;
    if (const sf::Font* font = getRegularFont()) {
        sf::Text lineProbe(*font, "Mg", fontSize_);
        applyCrispText(lineProbe, fontSize_);
        const sf::FloatRect bounds = getScaledLocalBounds(lineProbe);
        y += (inner.size.y - bounds.size.y) * 0.5f - bounds.position.y;
    }
    return {inner.position.x - horizontalScroll_, y};
}

void UITextInput::ensureCaretVisible_() const {
    const std::string display = passwordMode_
        ? std::string(content_.size(), '*')
        : content_;
    const sf::FloatRect inner = getInnerBounds_();
    const sf::Font* font = getRegularFont();
    if (!font || display.empty() || inner.size.x <= INPUT_CARET_WIDTH) {
        horizontalScroll_ = 0.0f;
        return;
    }

    sf::Text measure(*font, display, fontSize_);
    applyCrispText(measure, fontSize_);
    measure.setPosition({inner.position.x, 0.0f});

    const std::size_t cursor = std::min(cursorPos_, display.size());
    const float textStart = measure.findCharacterPos(0).x;
    const float textEnd = measure.findCharacterPos(display.size()).x;
    const float caretUnscrolled = measure.findCharacterPos(cursor).x;
    const float maximumScroll = std::max(
        0.0f, textEnd - textStart - inner.size.x
            + INPUT_CARET_WIDTH + INPUT_CARET_MARGIN);
    horizontalScroll_ = std::clamp(horizontalScroll_, 0.0f, maximumScroll);

    const float visibleLeft = inner.position.x + INPUT_CARET_MARGIN;
    const float visibleRight = inner.position.x + inner.size.x
                             - INPUT_CARET_WIDTH - INPUT_CARET_MARGIN;
    const float caretX = caretUnscrolled - horizontalScroll_;
    if (caretX < visibleLeft) {
        horizontalScroll_ -= visibleLeft - caretX;
    } else if (caretX > visibleRight) {
        horizontalScroll_ += caretX - visibleRight;
    }
    horizontalScroll_ = std::clamp(horizontalScroll_, 0.0f, maximumScroll);
}

float UITextInput::charPosX_(std::size_t index, const std::string& displayText) const {
    const sf::FloatRect inner = getInnerBounds_();
    if (displayText.empty()) return inner.position.x - horizontalScroll_;

    const sf::Font* font = getRegularFont();
    if (!font) return inner.position.x - horizontalScroll_;

    sf::Text measure(*font, displayText, fontSize_);
    applyCrispText(measure, fontSize_);
    measure.setPosition(getTextPosition_(displayText));
    std::size_t clamped = std::min(index, displayText.size());
    return measure.findCharacterPos(clamped).x;
}

std::size_t UITextInput::indexFromMouseX_(float mouseX) const {
    ensureCaretVisible_();
    std::string displayText = passwordMode_ ? std::string(content_.size(), '*') : content_;
    if (displayText.empty()) {
        return 0;
    }

    const sf::Font* font = getRegularFont();
    if (!font) return 0;

    sf::Text measure(*font, displayText, fontSize_);
    applyCrispText(measure, fontSize_);
    measure.setPosition(getTextPosition_(displayText));

    float prevX = measure.findCharacterPos(0).x;
    if (mouseX <= prevX) {
        return 0;
    }

    for (std::size_t i = 1; i <= displayText.size(); ++i) {
        float x = measure.findCharacterPos(i).x;
        if (mouseX < x) {
            return ((mouseX - prevX) <= (x - mouseX)) ? (i - 1) : i;
        }
        prevX = x;
    }
    return displayText.size();
}

void UITextInput::moveCursorHorizontal_(int delta, bool extendSelection) {
    std::size_t nextPos = cursorPos_;
    if (delta < 0) {
        if (nextPos > 0) nextPos--;
    } else if (delta > 0) {
        if (nextPos < content_.size()) nextPos++;
    }

    if (extendSelection) {
        if (!selectionAnchor_) selectionAnchor_ = cursorPos_;
        cursorPos_ = nextPos;
        std::size_t anchor = *selectionAnchor_;
        selectionStart_ = std::min(anchor, cursorPos_);
        selectionEnd_ = std::max(anchor, cursorPos_);
        if (!hasSelection_()) selectionAnchor_.reset();
        return;
    }

    if (hasSelection_()) {
        cursorPos_ = (delta < 0) ? selectionLeft_() : selectionRight_();
    } else {
        cursorPos_ = nextPos;
    }
    clearSelection_();
}

void UITextInput::draw(sf::RenderWindow& window) const {
    const Theme theme = getTheme();
    sf::RectangleShape box = box_;
    box.setFillColor(active_ ? theme.inputActiveBackground : theme.inputBackground);
    box.setOutlineColor(active_ ? theme.inputActiveBorder
                                : hovered_ ? theme.buttonHoverBorder : theme.inputBorder);
    window.draw(box);
    const sf::Font* font = getRegularFont();
    if (!font) return;

    ensureCaretVisible_();
    const sf::FloatRect inner = getInnerBounds_();
    const sf::View callerView = window.getView();
    window.setView(makeClippedUIView(inner, callerView));

    if (content_.empty() && !active_ && !placeholder_.empty()) {
        sf::Text ph(*font, placeholder_, fontSize_);
        applyCrispText(ph, fontSize_);
        ph.setFillColor(theme.textMuted);
        ph.setPosition(getTextPosition_(placeholder_));
        window.draw(ph);
    } else {
        std::string display = passwordMode_ ? std::string(content_.size(), '*') : content_;
        const sf::Vector2f textPos = getTextPosition_(display);

        if (hasSelection_() && !display.empty()) {
            float selX1 = charPosX_(selectionLeft_(), display);
            float selX2 = charPosX_(selectionRight_(), display);
            sf::RectangleShape selRect(
                sf::Vector2f(std::max(1.0f, selX2 - selX1), inner.size.y));
            selRect.setPosition({selX1, inner.position.y});
            selRect.setFillColor(getTextSelectionFillColor());
            window.draw(selRect);
        }

        sf::Text t(*font, display, fontSize_);
        applyCrispText(t, fontSize_);
        t.setFillColor(theme.textPrimary);
        t.setPosition(textPos);
        window.draw(t);

        if (active_) {
            if (caretBlinkClock_.getElapsedTime().asSeconds() >= 0.5f) {
                caretVisible_ = !caretVisible_;
                caretBlinkClock_.restart();
            }
            if (caretVisible_) {
                float caretX = charPosX_(cursorPos_, display);
                sf::Text lineProbe(*font, "Mg", fontSize_);
                applyCrispText(lineProbe, fontSize_);
                const float caretHeight = std::min(
                    inner.size.y, std::max(1.0f, getScaledLocalBounds(lineProbe).size.y));
                sf::RectangleShape caret({INPUT_CARET_WIDTH, caretHeight});
                caret.setFillColor(theme.textPrimary);
                caret.setPosition({caretX, inner.position.y
                    + (inner.size.y - caretHeight) * 0.5f});
                window.draw(caret);
            }
        }
    }

    window.setView(callerView);
}

void UITextInput::handleEvent(const sf::Event& event) {
    sf::Vector2f rawPointer{0.0f, 0.0f};
    if (const auto* pressed = event.getIf<sf::Event::MouseButtonPressed>()) {
        rawPointer = static_cast<sf::Vector2f>(pressed->position);
    } else if (const auto* moved = event.getIf<sf::Event::MouseMoved>()) {
        rawPointer = static_cast<sf::Vector2f>(moved->position);
    } else if (const auto* released = event.getIf<sf::Event::MouseButtonReleased>()) {
        rawPointer = static_cast<sf::Vector2f>(released->position);
    }
    handleEvent(event, rawPointer);
}

void UITextInput::handleEvent(const sf::Event& event,
                              const sf::Vector2f& mappedPointer) {
    if (auto* mb = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (mb->button == sf::Mouse::Button::Left) {
            if (contains(mappedPointer)) {
                setActive(true);
                cursorPos_ = indexFromMouseX_(mappedPointer.x);
                selectionStart_ = cursorPos_;
                selectionEnd_ = cursorPos_;
                selectionAnchor_ = cursorPos_;
                mouseSelecting_ = true;
                resetCaretBlink_();
            } else {
                setActive(false);
                mouseSelecting_ = false;
                selectionAnchor_.reset();
            }
        }
        return;
    }

    if (auto* mm = event.getIf<sf::Event::MouseMoved>()) {
        if (active_ && mouseSelecting_) {
            cursorPos_ = indexFromMouseX_(mappedPointer.x);
            if (selectionAnchor_) {
                selectionStart_ = std::min(*selectionAnchor_, cursorPos_);
                selectionEnd_ = std::max(*selectionAnchor_, cursorPos_);
            }
            resetCaretBlink_();
        }
        return;
    }

    if (auto* mr = event.getIf<sf::Event::MouseButtonReleased>()) {
        if (mr->button == sf::Mouse::Button::Left) {
            mouseSelecting_ = false;
            if (!hasSelection_()) {
                selectionAnchor_.reset();
            }
        }
        return;
    }

    if (!active_) return;

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
            return;
        }

        if (ctrl && kp->code == sf::Keyboard::Key::C) {
            if (hasSelection_()) {
                sf::Clipboard::setString(content_.substr(selectionLeft_(), selectionRight_() - selectionLeft_()));
            }
            return;
        }

        if (ctrl && kp->code == sf::Keyboard::Key::X) {
            if (hasSelection_()) {
                sf::Clipboard::setString(content_.substr(selectionLeft_(), selectionRight_() - selectionLeft_()));
                eraseSelection_();
                resetCaretBlink_();
            }
            return;
        }

        if (ctrl && kp->code == sf::Keyboard::Key::V) {
            std::string clip = sf::Clipboard::getString().toAnsiString();
            std::string cleaned;
            cleaned.reserve(clip.size());
            for (char c : clip) {
                if (c >= 32 && c < 127) cleaned.push_back(c);
            }
            if (!cleaned.empty()) {
                const std::size_t selectedLength = hasSelection_()
                    ? selectionRight_() - selectionLeft_()
                    : 0;
                const std::size_t retainedLength = content_.size() - selectedLength;
                if (maxLength_ > 0) {
                    const std::size_t available = retainedLength < maxLength_
                        ? maxLength_ - retainedLength
                        : 0;
                    if (cleaned.size() > available) cleaned.resize(available);
                }
                insertText(cleaned);
            }
            return;
        }

        if (kp->code == sf::Keyboard::Key::Left) {
            moveCursorHorizontal_(-1, shift);
            resetCaretBlink_();
            return;
        }

        if (kp->code == sf::Keyboard::Key::Right) {
            moveCursorHorizontal_(1, shift);
            resetCaretBlink_();
            return;
        }

        if (kp->code == sf::Keyboard::Key::Home) {
            if (shift) {
                if (!selectionAnchor_) selectionAnchor_ = cursorPos_;
                cursorPos_ = 0;
                selectionStart_ = std::min(*selectionAnchor_, cursorPos_);
                selectionEnd_ = std::max(*selectionAnchor_, cursorPos_);
                if (!hasSelection_()) selectionAnchor_.reset();
            } else {
                cursorPos_ = 0;
                clearSelection_();
            }
            resetCaretBlink_();
            return;
        }

        if (kp->code == sf::Keyboard::Key::End) {
            if (shift) {
                if (!selectionAnchor_) selectionAnchor_ = cursorPos_;
                cursorPos_ = content_.size();
                selectionStart_ = std::min(*selectionAnchor_, cursorPos_);
                selectionEnd_ = std::max(*selectionAnchor_, cursorPos_);
                if (!hasSelection_()) selectionAnchor_.reset();
            } else {
                cursorPos_ = content_.size();
                clearSelection_();
            }
            resetCaretBlink_();
            return;
        }

        if (kp->code == sf::Keyboard::Key::Backspace) {
            if (hasSelection_()) {
                eraseSelection_();
            } else if (cursorPos_ > 0) {
                content_.erase(cursorPos_ - 1, 1);
                cursorPos_--;
                clearSelection_();
            }
            resetCaretBlink_();
            return;
        }

        if (kp->code == sf::Keyboard::Key::Delete) {
            if (hasSelection_()) {
                eraseSelection_();
            } else if (cursorPos_ < content_.size()) {
                content_.erase(cursorPos_, 1);
                clearSelection_();
            }
            resetCaretBlink_();
            return;
        }
    }

    if (auto* te = event.getIf<sf::Event::TextEntered>()) {
        std::uint32_t code = te->unicode;
        if (code >= 32 && code < 127) {
            const char character = static_cast<char>(code);
            insertText(std::string_view(&character, 1));
        }
    }
}

void UITextInput::setActive(bool a) {
    active_ = a;
    if (active_) {
        box_.setOutlineColor(getTheme().inputActiveBorder);
        cursorPos_ = std::min(cursorPos_, content_.size());
        clearSelection_();
        resetCaretBlink_();
    } else {
        box_.setOutlineColor(getTheme().inputBorder);
        mouseSelecting_ = false;
        selectionAnchor_.reset();
    }
}

bool UITextInput::isActive() const { return active_; }
std::string UITextInput::getText() const { return content_; }
void UITextInput::setText(const std::string& text) {
    content_ = maxLength_ > 0 && text.size() > maxLength_
        ? text.substr(0, maxLength_)
        : text;
    cursorPos_ = content_.size();
    clearSelection_();
    resetCaretBlink_();
}

bool UITextInput::insertText(std::string_view text) {
    if (text.empty()) return false;

    const std::size_t selectedLength = hasSelection_()
        ? selectionRight_() - selectionLeft_()
        : 0;
    const std::size_t retainedLength = content_.size() - selectedLength;
    if (maxLength_ > 0 && text.size() > maxLength_ - std::min(retainedLength, maxLength_)) {
        return false;
    }

    if (hasSelection_()) eraseSelection_();
    content_.insert(cursorPos_, text);
    cursorPos_ += text.size();
    clearSelection_();
    resetCaretBlink_();
    return true;
}

void UITextInput::clear() {
    content_.clear();
    cursorPos_ = 0;
    clearSelection_();
    resetCaretBlink_();
}
void UITextInput::setPlaceholder(const std::string& p) { placeholder_ = p; }

void UITextInput::setMaxLength(std::size_t maxLength) {
    maxLength_ = maxLength;
    if (maxLength_ > 0 && content_.size() > maxLength_) {
        content_.resize(maxLength_);
        cursorPos_ = content_.size();
        clearSelection_();
    }
    resetCaretBlink_();
}

std::size_t UITextInput::getMaxLength() const { return maxLength_; }

void UITextInput::setFontSize(unsigned int fontSize) {
    fontSize_ = std::max(1u, fontSize);
    updateTextPosition_();
    resetCaretBlink_();
}

unsigned int UITextInput::getFontSize() const { return fontSize_; }

void UITextInput::setPasswordMode(bool passwordMode) {
    passwordMode_ = passwordMode;
    resetCaretBlink_();
}

bool UITextInput::isPasswordMode() const { return passwordMode_; }
void UITextInput::setPosition(const sf::Vector2f& pos) {
    box_.setPosition(pos);
    updateTextPosition_();
}
void UITextInput::setSize(const sf::Vector2f& size) {
    box_.setSize(size);
    updateTextPosition_();
}
sf::FloatRect UITextInput::getBounds() const { return box_.getGlobalBounds(); }
bool UITextInput::contains(const sf::Vector2f& point) const {
    return box_.getGlobalBounds().contains(point);
}

void UITextInput::updateHover(const sf::Vector2i& mousePosition) {
    hovered_ = contains(static_cast<sf::Vector2f>(mousePosition));
}

void UITextInput::updateTextPosition_() {
    ensureCaretVisible_();
}

} // namespace RenUI
