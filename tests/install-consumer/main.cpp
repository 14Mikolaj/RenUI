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

    const bool missingFallbackWorks = missingResult.initialized &&
        !missingResult.regularFontAvailable && !missingResult.atlasAvailable &&
        diagnosticCount > 0 && button.isPrimary() && checkbox.isChecked();

    RenUI::setTextScaleLevel(RenUI::TextScaleLevel::Large);
    RenUI::setLargeTextRefinementEnabled(false);
    const std::uint64_t directTextRevision = RenUI::getTextMetricsRevision();
    RenUI::setLargeTextRefinementEnabled(true);
    const bool textRefinementToggleWorks =
        RenUI::isLargeTextRefinementEnabled() &&
        RenUI::getTextMetricsRevision() > directTextRevision;

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
                   textRefinementToggleWorks
        ? 0
        : 1;
}
