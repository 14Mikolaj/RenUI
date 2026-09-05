#include <RenUI/RenUI.hpp>

int main(int argc, char** argv) {
    RenUI::shutdown();

    // Controls created before initialization must retain state and geometry.
    RenUI::Button button("RenUI", {0.f, 0.f}, {120.f, 32.f});
    RenUI::Label label("late font", 14);
    RenUI::Checkbox checkbox("late checkbox", {0.f, 40.f});
    RenUI::ProgressBar progress({0.f, 70.f}, {120.f, 20.f},
                                sf::Color::Black, sf::Color::Green);
    progress.setValue(1.f, 2.f);
    button.setPrimary(true);

    const RenUI::FlowLayoutResult flow = RenUI::layoutFlowItems(
        {0.f, 0.f}, 100.f, {{60.f, 20.f}, {60.f, 24.f}}, 4.f, 5.f);
    RenUI::DraggableWindow rollout("Rollout", {20.f, 20.f}, {100.f, 100.f});
    rollout.setSizeKeepingTopLeft({180.f, 100.f});
    rollout.open();
    const bool responsiveGeometryWorks = flow.items.size() == 2 &&
        flow.items[1].position.y == 25.f && flow.height == 49.f &&
        rollout.getPanelPosition() == sf::Vector2f(20.f, 20.f) &&
        rollout.getPanelSize() == sf::Vector2f(180.f, 100.f);

    int diagnosticCount = 0;
    RenUI::Config missing;
    missing.regularFontPaths = {"missing/font.ttf"};
    missing.boldFontPaths = {"missing/font-bold.ttf"};
    missing.atlasTexturePath = "missing/atlas.png";
    missing.atlasMetadataPath = "missing/atlas.json";
    missing.hoverShaderPath = "missing/hover.frag";
    missing.enableShaders = false;
    missing.diagnosticSink = [&](const RenUI::Diagnostic&) { ++diagnosticCount; };
    const auto missingResult = RenUI::initialize(missing);
    label.setPosition({4.f, 4.f});
    checkbox.setChecked(true);

    RenUI::NineSlicePanelData panelSkin{
        "panel_tl", "panel_t", "panel_tr",
        "panel_l", "panel_center", "panel_r",
        "panel_bl", "panel_b", "panel_br",
        RenUI::NineSliceCloseButtonData{
            "panel_close", "panel_close_hover", "panel_close_inset",
            "panel_close_unavailable"}
    };
    RenUI::NineSlicePanelData unavailablePanelSkin = panelSkin;
    unavailablePanelSkin.closeButtonAvailable = false;
    RenUI::NineSlicePanelData textButtonSkin = panelSkin;
    textButtonSkin.closeButton.reset();
    RenUI::setUiAtlasSliceAlias("panel_tl", "style1_panel_tl");
    RenUI::setDefaultTextButtonNineSliceData(textButtonSkin);
    RenUI::Panel skinnedPanel({8.f, 8.f}, {160.f, 100.f}, panelSkin);
    rollout.setNineSliceData(panelSkin);

    const bool missingFallbackWorks = missingResult.initialized &&
        !missingResult.regularFontAvailable && !missingResult.atlasAvailable &&
        diagnosticCount > 0 && button.isPrimary() && checkbox.isChecked() &&
        skinnedPanel.getNineSliceData().has_value() &&
        !skinnedPanel.getCloseButtonBounds().has_value() &&
        unavailablePanelSkin.closeButton.has_value() &&
        !unavailablePanelSkin.closeButtonAvailable &&
        RenUI::getDefaultTextButtonNineSliceData() != nullptr &&
        rollout.getNineSliceData().has_value();

    RenUI::setTextScaleLevel(RenUI::TextScaleLevel::Large);
    RenUI::setLargeTextRefinementEnabled(false);
    const std::uint64_t directTextRevision = RenUI::getTextMetricsRevision();
    RenUI::setLargeTextRefinementEnabled(true);
    const bool textRefinementToggleWorks =
        RenUI::isLargeTextRefinementEnabled() &&
        RenUI::getTextMetricsRevision() > directTextRevision;

    using NamedMarkerFunction = void (*)(
        sf::RenderWindow&, const sf::Vector2f&, float, sf::Angle,
        const std::string&, sf::Color);
    const NamedMarkerFunction namedMarker = &RenUI::drawDirectionalMarker;
    const bool namedMarkerApiWorks = namedMarker != nullptr;

    using TintedNineSliceFunction = bool (*)(
        sf::RenderTarget&, const sf::FloatRect&,
        const RenUI::NineSlicePanelData&, bool, sf::Color);
    constexpr TintedNineSliceFunction tintedNineSlice =
        static_cast<TintedNineSliceFunction>(&RenUI::drawNineSlicePanel);
    static_assert(tintedNineSlice != nullptr);
    const bool tintedNineSliceApiWorks = tintedNineSlice != nullptr;
    RenUI::setNineSlicePanelTint(sf::Color(255, 255, 255, 128));
    const bool globalNineSliceTintApiWorks =
        RenUI::getNineSlicePanelTint() == sf::Color(255, 255, 255, 128);

    using MappedWindowEventFunction =
        bool (RenUI::DraggableWindow::*)(const sf::Event&, const sf::Vector2f&);
    constexpr MappedWindowEventFunction mappedWindowEvent =
        static_cast<MappedWindowEventFunction>(
            &RenUI::DraggableWindow::handleEvent);
    static_assert(mappedWindowEvent != nullptr);
    const bool mappedWindowEventApiWorks = mappedWindowEvent != nullptr;

    using CompatibilityTextAreaEventFunction =
        bool (RenUI::UITextArea::*)(const sf::Event&);
    using MappedTextAreaEventFunction =
        bool (RenUI::UITextArea::*)(const sf::Event&, const sf::Vector2f&);
    constexpr CompatibilityTextAreaEventFunction compatibilityTextAreaEvent =
        static_cast<CompatibilityTextAreaEventFunction>(
            &RenUI::UITextArea::handleEvent);
    constexpr MappedTextAreaEventFunction mappedTextAreaEvent =
        static_cast<MappedTextAreaEventFunction>(
            &RenUI::UITextArea::handleEvent);
    static_assert(compatibilityTextAreaEvent != nullptr);
    static_assert(mappedTextAreaEvent != nullptr);
    const bool textAreaEventApiWorks = compatibilityTextAreaEvent != nullptr &&
                                       mappedTextAreaEvent != nullptr;

    bool lateFontWorks = true;
    if (argc > 1) {
        RenUI::Config late;
        late.resources = std::make_shared<RenUI::FileSystemResourceProvider>(argv[1]);
        late.atlasTexturePath = "missing/atlas.png";
        late.atlasMetadataPath = "missing/atlas.json";
        late.enableShaders = false;
        late.diagnosticSink = [](const RenUI::Diagnostic&) {};
        const auto lateResult = RenUI::initialize(late);
        lateFontWorks = lateResult.regularFontAvailable &&
                        label.getBounds().size.x > 0.f &&
                        checkbox.getBounds().size.x > 20.f;
    }

    RenUI::shutdown();
    return missingFallbackWorks && lateFontWorks && responsiveGeometryWorks &&
                   textRefinementToggleWorks && namedMarkerApiWorks &&
                   tintedNineSliceApiWorks &&
                   globalNineSliceTintApiWorks &&
                   mappedWindowEventApiWorks &&
                   textAreaEventApiWorks
        ? 0
        : 1;
}
