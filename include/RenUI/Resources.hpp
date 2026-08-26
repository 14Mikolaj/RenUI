#pragma once

#include <RenUI/Export.hpp>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace RenUI {

class RENUI_API ResourceProvider {
public:
    virtual ~ResourceProvider() = default;

    virtual std::optional<std::vector<std::uint8_t>> readBinary(
        std::string_view resource) = 0;
    virtual std::optional<std::string> readText(std::string_view resource);
};

class RENUI_API FileSystemResourceProvider final : public ResourceProvider {
public:
    explicit FileSystemResourceProvider(std::filesystem::path root = {});

    std::optional<std::vector<std::uint8_t>> readBinary(
        std::string_view resource) override;
    std::optional<std::string> readText(std::string_view resource) override;

    const std::filesystem::path& root() const noexcept;
    void setRoot(std::filesystem::path root);

private:
    std::filesystem::path root_;
};

} // namespace RenUI
