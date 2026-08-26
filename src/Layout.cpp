#include <RenUI/RenUI.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>

namespace RenUI {

std::vector<std::string> wrapTextLines(const std::string& text,
                                       float maxWidth,
                                       unsigned int fontSize,
                                       bool bold,
                                       std::size_t maxLines) {
    const sf::Font* font = bold ? getBoldFont() : getRegularFont();
    if (!font) font = getRegularFont();
    if (!font || !std::isfinite(maxWidth) || maxWidth <= 0.0f) return {};

    auto measuredWidth = [&](const std::string& value) {
        if (value.empty()) return 0.0f;
        sf::Text probe(*font, value, fontSize);
        if (bold) probe.setStyle(sf::Text::Bold);
        applyCrispText(probe, fontSize);
        return getScaledLocalBounds(probe).size.x;
    };
    auto fits = [&](const std::string& value) {
        return measuredWidth(value) <= maxWidth + 0.001f;
    };

    std::vector<std::string> lines;
    auto wrapParagraph = [&](const std::string& paragraph) {
        const std::size_t lineCountBefore = lines.size();
        std::string line;

        auto placeWord = [&](const std::string& word) {
            if (!line.empty()) {
                const std::string candidate = line + " " + word;
                if (fits(candidate)) {
                    line = candidate;
                    return;
                }
                lines.push_back(line);
                line.clear();
            }

            if (fits(word)) {
                line = word;
                return;
            }

            // A token that cannot fit intact is split bytewise, matching the
            // std::string/ANSI text convention used by existing RenUI fields.
            std::string fragment;
            for (char character : word) {
                const std::string candidate = fragment + character;
                if (fragment.empty() || fits(candidate)) {
                    fragment = candidate;
                } else {
                    lines.push_back(fragment);
                    fragment.assign(1, character);
                }
            }
            line = fragment;
        };

        std::size_t cursor = 0;
        while (cursor < paragraph.size()) {
            while (cursor < paragraph.size()
                   && std::isspace(static_cast<unsigned char>(paragraph[cursor]))) {
                ++cursor;
            }
            if (cursor >= paragraph.size()) break;

            const std::size_t wordStart = cursor;
            while (cursor < paragraph.size()
                   && !std::isspace(static_cast<unsigned char>(paragraph[cursor]))) {
                ++cursor;
            }
            placeWord(paragraph.substr(wordStart, cursor - wordStart));
        }

        if (!line.empty()) lines.push_back(line);
        if (lines.size() == lineCountBefore) lines.emplace_back();
    };

    std::size_t paragraphStart = 0;
    while (true) {
        const std::size_t hardBreak = text.find('\n', paragraphStart);
        wrapParagraph(text.substr(
            paragraphStart,
            hardBreak == std::string::npos ? std::string::npos
                                           : hardBreak - paragraphStart));
        if (hardBreak == std::string::npos) break;
        paragraphStart = hardBreak + 1;
    }

    if (maxLines > 0 && lines.size() > maxLines) {
        lines.resize(maxLines);
        std::string ellipsis = "...";
        while (!ellipsis.empty() && !fits(ellipsis)) ellipsis.pop_back();

        std::string& last = lines.back();
        while (!last.empty() && !fits(last + ellipsis)) last.pop_back();
        last += ellipsis;
    }
    return lines;
}

FormLayout::FormLayout(const sf::FloatRect& bounds, float gap)
    : bounds_(bounds), cursorY_(bounds.position.y), gap_(std::max(0.0f, gap)) {}

sf::FloatRect FormLayout::place(float height, float gapAfter) {
    const float safeHeight = std::max(0.0f, height);
    const float safeGap = gapAfter < 0.0f ? gap_ : std::max(0.0f, gapAfter);
    sf::FloatRect placed({bounds_.position.x, cursorY_},
                         {bounds_.size.x, safeHeight});
    cursorY_ += safeHeight + safeGap;
    return placed;
}

FormFieldLayout FormLayout::placeField(float labelHeight,
                                       float inputHeight,
                                       float labelToInputGap,
                                       float gapAfter) {
    const float safeLabelHeight = std::max(0.0f, labelHeight);
    const float safeInputHeight = std::max(0.0f, inputHeight);
    const float safeInnerGap = std::max(0.0f, labelToInputGap);
    const float safeOuterGap = gapAfter < 0.0f ? gap_ : std::max(0.0f, gapAfter);

    FormFieldLayout field;
    field.label = sf::FloatRect({bounds_.position.x, cursorY_},
                                {bounds_.size.x, safeLabelHeight});
    field.input = sf::FloatRect({bounds_.position.x,
                                 cursorY_ + safeLabelHeight + safeInnerGap},
                                {bounds_.size.x, safeInputHeight});
    cursorY_ = field.input.position.y + safeInputHeight + safeOuterGap;
    return field;
}

std::vector<sf::FloatRect> FormLayout::placeRow(const std::vector<float>& widths,
                                                float height,
                                                float itemGap,
                                                float gapAfter) {
    std::vector<sf::FloatRect> row;
    row.reserve(widths.size());
    const float safeHeight = std::max(0.0f, height);
    float safeItemGap = std::max(0.0f, itemGap);
    float requestedWidth = 0.0f;
    for (float width : widths) requestedWidth += std::max(0.0f, width);
    if (widths.size() > 1) {
        const float maxGap = std::max(
            0.0f,
            (bounds_.size.x - requestedWidth) /
                static_cast<float>(widths.size() - 1));
        safeItemGap = std::min(safeItemGap, maxGap);
    }
    const float availableForItems = std::max(
        0.0f,
        bounds_.size.x - safeItemGap *
            static_cast<float>(widths.empty() ? 0 : widths.size() - 1));
    const float widthScale = requestedWidth > availableForItems && requestedWidth > 0.0f
        ? availableForItems / requestedWidth
        : 1.0f;

    float x = bounds_.position.x;
    for (float width : widths) {
        const float safeWidth = std::max(0.0f, width) * widthScale;
        row.emplace_back(sf::Vector2f{x, cursorY_},
                         sf::Vector2f{safeWidth, safeHeight});
        x += safeWidth + safeItemGap;
    }
    const float safeOuterGap = gapAfter < 0.0f ? gap_ : std::max(0.0f, gapAfter);
    cursorY_ += safeHeight + safeOuterGap;
    return row;
}

void FormLayout::addSpacing(float extra) {
    cursorY_ += std::max(0.0f, extra);
}

float FormLayout::currentY() const { return cursorY_; }

float FormLayout::remainingHeight() const {
    return std::max(0.0f, bounds_.position.y + bounds_.size.y - cursorY_);
}

const sf::FloatRect& FormLayout::bounds() const { return bounds_; }

FlowLayoutResult layoutFlowItems(const sf::Vector2f& start,
                                 float availableWidth,
                                 const std::vector<sf::Vector2f>& itemSizes,
                                 float itemGap,
                                 float rowGap) {
    FlowLayoutResult result;
    result.items.reserve(itemSizes.size());
    if (itemSizes.empty()) return result;

    const float safeWidth = std::isfinite(availableWidth)
        ? std::max(0.0f, availableWidth)
        : 0.0f;
    const float safeItemGap = std::isfinite(itemGap)
        ? std::max(0.0f, itemGap)
        : 0.0f;
    const float safeRowGap = std::isfinite(rowGap)
        ? std::max(0.0f, rowGap)
        : 0.0f;
    const float rightEdge = start.x + safeWidth;
    float cursorX = start.x;
    float cursorY = start.y;
    float rowHeight = 0.0f;
    float bottom = start.y;

    for (const sf::Vector2f& requestedSize : itemSizes) {
        const sf::Vector2f size{
            std::isfinite(requestedSize.x) ? std::max(0.0f, requestedSize.x) : 0.0f,
            std::isfinite(requestedSize.y) ? std::max(0.0f, requestedSize.y) : 0.0f
        };
        if (cursorX > start.x && cursorX + size.x > rightEdge + 0.01f) {
            cursorX = start.x;
            cursorY += rowHeight + safeRowGap;
            rowHeight = 0.0f;
        }

        result.items.emplace_back(sf::Vector2f{cursorX, cursorY}, size);
        cursorX += size.x + safeItemGap;
        rowHeight = std::max(rowHeight, size.y);
        bottom = std::max(bottom, cursorY + size.y);
    }

    result.height = std::max(0.0f, bottom - start.y);
    return result;
}

} // namespace RenUI
