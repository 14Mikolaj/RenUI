#pragma once

#include <RenUI/Export.hpp>

#include <functional>
#include <string>
#include <vector>

namespace RenUI {

enum class DiagnosticSeverity {
    Info,
    Warning,
    Error
};

enum class DiagnosticCode {
    MissingResource,
    InvalidResource,
    ResourceProviderFailure,
    ShaderUnavailable,
    InitializationFailed
};

struct Diagnostic {
    DiagnosticSeverity severity{DiagnosticSeverity::Info};
    DiagnosticCode code{DiagnosticCode::InitializationFailed};
    std::string message;
    std::string resource;
};

using DiagnosticSink = std::function<void(const Diagnostic&)>;

RENUI_API std::vector<Diagnostic> getDiagnostics();
RENUI_API void clearDiagnostics();

} // namespace RenUI
