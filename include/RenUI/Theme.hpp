#pragma once

#include <RenUI/Export.hpp>

#include <SFML/Graphics/Color.hpp>

namespace RenUI {

struct RENUI_API Theme {
    // Semantic surface and text palette used by default controls.
    sf::Color panelBackground{40, 40, 50, 230};
    sf::Color panelBorder{100, 100, 120, 255};
    sf::Color inputBackground{64, 64, 64, 160};
    sf::Color inputActiveBackground{56, 56, 72, 220};
    sf::Color inputBorder{255, 255, 255, 255};
    sf::Color inputActiveBorder{140, 180, 220, 255};
    sf::Color buttonNormal{128, 128, 128, 128};
    sf::Color buttonHover{150, 150, 164, 180};
    sf::Color buttonActive{60, 120, 200, 220};
    sf::Color buttonPrimary{38, 112, 70, 225};
    sf::Color buttonPrimaryHover{54, 142, 88, 235};
    sf::Color buttonPrimaryBorder{100, 190, 132, 255};
    sf::Color buttonPrimaryHoverBorder{164, 238, 190, 255};
    sf::Color buttonDanger{132, 48, 54, 225};
    sf::Color buttonDangerHover{174, 62, 70, 235};
    sf::Color buttonDangerBorder{200, 82, 88, 255};
    sf::Color buttonDisabled{70, 70, 78, 150};
    sf::Color buttonBorder{0, 0, 0, 0};
    sf::Color buttonHoverBorder{200, 200, 220, 255};
    sf::Color buttonGlowBase{28, 36, 56, 220};
    sf::Color buttonGlowHover{40, 58, 86, 234};
    sf::Color buttonGlowBorder{108, 148, 196, 225};
    sf::Color buttonGlowHoverBorder{176, 222, 255, 245};
    sf::Color buttonGlowText{226, 186, 84, 242};
    sf::Color buttonGlowHoverText{255, 232, 152, 255};
    sf::Color textPrimary{255, 255, 255, 255};
    sf::Color textSecondary{206, 212, 236, 255};
    sf::Color textMuted{120, 120, 140, 255};
    sf::Color textDisabled{100, 100, 110, 255};
    sf::Color focusOutline{255, 255, 0, 255};
    sf::Color selectionFill{72, 124, 216, 165};
    sf::Color rowHighlight{50, 60, 85, 255};
    sf::Color divider{80, 80, 100, 255};

    // Specialized surfaces retain the same semantic defaults by default.
    sf::Color windowBackground{40, 40, 50, 230};
    sf::Color windowBorder{100, 100, 120, 255};
    float windowPadding{16.f};
    float windowHeaderHeight{60.f};
    float windowScrollSpeed{40.f};
    float tooltipPadding{8.f};

    unsigned int windowTitleFontSize{18};
    unsigned int tooltipBodyFontSize{11};
    unsigned int tooltipTitleFontSize{13};
    unsigned int valueBreakdownFontSize{13};

    float valueBreakdownPrimaryOffset{110.f};
    float valueBreakdownSecondaryOffset{150.f};
    float valueBreakdownModifierOffset{210.f};
    float valueBreakdownTotalOffset{270.f};

    static Theme defaults();
};

} // namespace RenUI
