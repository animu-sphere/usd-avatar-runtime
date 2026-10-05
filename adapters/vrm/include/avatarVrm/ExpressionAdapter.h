#pragma once
#include "avatarRuntime/api.h"
#include "vrmRig/ExpressionResolver.h"
#include "vrmRig/LookAtEvaluator.h"
#include <array>
#include <memory>

namespace avatarVrm {
// Adapter-only C++ configuration. No owner types enter the runtime C ABI.
struct InputIdentity {
    std::string source, actor, channel;
};
struct ExpressionInputBinding {
    InputIdentity input;
    std::string expression; // verbatim owner name; explicit semantic mapping
};
struct MorphBinding {
    std::string ownerTarget, mesh, target;
};
struct EyeBinding {
    std::string ownerJoint, skeleton, joint;
    // Authored parent-local rest quaternion (x,y,z,w), not the animated value.
    std::array<double, 4> restRotation{0, 0, 0, 1};
};
struct ExpressionAdapterConfig {
    std::string evaluatorId;
    std::string layoutId;
    uint64_t layoutVersion = 0;
    vrmRig::ExpressionRig expressions;
    std::vector<ExpressionInputBinding> inputs;
    std::vector<MorphBinding> morphs;
    // Optional expression- or bone-driven LookAt. Points/directions in world
    // or bound joint-local space; head/reference ancestry must have unit scale.
    // Directions use orientation only, with no positional eye parallax.
    std::optional<vrmRig::LookAtRig> lookAt;
    InputIdentity gaze;
    std::string headSkeleton, headJoint;
    std::vector<std::string> after;
    // Bone rigs: exactly one mapping for each named owner eye. Runtime eyes
    // must be direct children of the head in the same skeleton.
    std::vector<EyeBinding> eyes;
};

// Immutable configuration, stateless per-instance evaluation. Keep this object
// alive until the runtime is destroyed (registration borrows user_data).
// Throws invalid_argument for ambiguous/incomplete configuration.
class ExpressionAdapter {
public:
    explicit ExpressionAdapter(ExpressionAdapterConfig config);
    ~ExpressionAdapter();
    ExpressionAdapter(const ExpressionAdapter&) = delete;
    ExpressionAdapter& operator=(const ExpressionAdapter&) = delete;
    ArEvaluatorDesc Descriptor() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace avatarVrm
