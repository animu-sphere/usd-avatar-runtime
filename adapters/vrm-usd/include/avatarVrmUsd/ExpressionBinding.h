#pragma once
#include "avatarVrmUsd/HumanoidBinding.h"
#include "avatarVrm/ExpressionAdapter.h"

namespace avatarVrmUsd {
struct ExpressionBindingConfig {
    HumanoidBindingConfig humanoid;
    // Empty: all applied VrmExpressionAPI prims below avatarRoot.
    pxr::SdfPath expressionsRoot;
};

// Owned default-time expression definitions and canonical output baseline.
// No stage is retained. Input identity/semantic selection belongs to the host.
class ExpressionBinding {
public:
    ExpressionBinding(const pxr::UsdStagePtr& stage, ExpressionBindingConfig config);
    const HumanoidBinding& Humanoid() const;
    const ArStateView& Baseline() const;
    const vrmRig::ExpressionRig& Rig() const;
    avatarVrm::ExpressionAdapterConfig AdapterConfig(
        std::string evaluatorId, std::vector<avatarVrm::ExpressionInputBinding> inputs,
        std::vector<std::string> after = {}) const;
    // Enrich a LookAt configuration for the exact same skeleton/layout.
    void ApplyTo(avatarVrm::ExpressionAdapterConfig& config) const;
private:
    struct Impl;
    std::shared_ptr<const Impl> impl_;
};
} // namespace avatarVrmUsd
