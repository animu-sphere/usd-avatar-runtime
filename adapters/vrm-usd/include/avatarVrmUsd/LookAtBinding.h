#pragma once
#include "avatarVrmUsd/HumanoidBinding.h"
#include "avatarVrm/ExpressionAdapter.h"

namespace avatarVrmUsd {
struct LookAtBindingConfig {
    HumanoidBindingConfig humanoid;
    // Empty: discover exactly one applied VrmLookAtAPI under avatarRoot.
    pxr::SdfPath lookAt;
};

// Owned default-time schema snapshot. Range maps/raw offsets are parsed by
// vrmRig; normalized typed vrm:type takes precedence. No stage is retained.
// Expression-type rigs need host-supplied expression/output bindings before
// they can produce morph/material effects. Input selection remains explicit.
class LookAtBinding {
public:
    LookAtBinding(const pxr::UsdStagePtr& stage, LookAtBindingConfig config);
    const HumanoidBinding& Humanoid() const;
    const std::string& LookAtId() const;
    const std::vector<std::string>& Warnings() const;
    avatarVrm::ExpressionAdapterConfig AdapterConfig(
        std::string evaluatorId, avatarVrm::InputIdentity gaze,
        std::vector<std::string> after = {}) const;
private:
    struct Impl;
    std::shared_ptr<const Impl> impl_;
};
} // namespace avatarVrmUsd
