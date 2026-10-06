#pragma once
#include "avatarRuntime/diagnostics.h"
#include "motionCore/Validation.h"
#include <stdexcept>
#include <utility>

namespace avatarMotion {
// Owned construction/assembly failure. Keeps owner reports and installed
// package identity outside the core C ABI. Catching invalid_argument remains
// supported. Call Emit to map the report into a host's synchronous C sink.
class MotionValidationError : public std::invalid_argument {
public:
    MotionValidationError(std::string origin, std::string ownerVersion,
                          openstrata::motion::ValidationReport report)
        : std::invalid_argument(Summary(origin, report)), origin_(std::move(origin)),
          ownerVersion_(std::move(ownerVersion)), report_(std::move(report)) {}
    const std::string& Origin() const { return origin_; }
    const std::string& OwnerVersion() const { return ownerVersion_; }
    const openstrata::motion::ValidationReport& Report() const { return report_; }
    // Borrowed callback strings; sink must copy retained records and not throw.
    // Construction errors have no runtime instance/frame/evaluator identity.
    void Emit(const ArDiagnosticSink& sink) const {
        if (!sink.emit) return;
        for (const auto& issue : report_.reported) {
            const std::string code(openstrata::motion::ValidationCodeString(issue.code));
            ArDiagnostic d{AR_HEADER(ArDiagnostic)};
            d.status = AR_INVALID_ARGUMENT; d.severity = AR_SEVERITY_ERROR;
            d.origin = origin_.c_str(); d.code = code.c_str();
            d.subject = issue.subject.c_str(); d.message = issue.detail.c_str();
            sink.emit(sink.user_data, &d);
        }
    }
private:
    static std::string Summary(const std::string& origin,
                               const openstrata::motion::ValidationReport& report) {
        if (report.reported.empty()) return origin + ": invalid motion input";
        const auto& issue = report.reported.front();
        return origin + ": " + std::string(openstrata::motion::ValidationCodeString(issue.code)) +
            " [" + issue.subject + "]: " + issue.detail;
    }
    std::string origin_, ownerVersion_;
    openstrata::motion::ValidationReport report_;
};
} // namespace avatarMotion
