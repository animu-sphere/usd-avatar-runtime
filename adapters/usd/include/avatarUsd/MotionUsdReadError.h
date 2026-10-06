#pragma once
#include "motionUsd/SkeletonReader.h"
#include <stdexcept>
#include <utility>

namespace avatarUsd {
// A pre-instance owner refusal, retained independently of stage/binding life.
// Existing invalid_argument handlers remain usable; hosts can inspect the
// unmodified owner record and the installed package version separately.
class MotionUsdReadError : public std::invalid_argument {
public:
    MotionUsdReadError(openstrata::motion::SkeletonReadDiagnostic diagnostic, std::string version)
        : std::invalid_argument(diagnostic.code + ": " + diagnostic.subject + ": " + diagnostic.detail),
          diagnostic_(std::move(diagnostic)), version_(std::move(version)) {}
    const openstrata::motion::SkeletonReadDiagnostic& Diagnostic() const noexcept { return diagnostic_; }
    const std::string& OwnerVersion() const noexcept { return version_; }
    const char* Owner() const noexcept { return "motionUsd"; }
private:
    openstrata::motion::SkeletonReadDiagnostic diagnostic_;
    std::string version_;
};
} // namespace avatarUsd
