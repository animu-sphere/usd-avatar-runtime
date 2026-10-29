#pragma once
#include "avatarRuntime/api.h"
#include "vrmRig/SpringBoneSolver.h"
#include <memory>

namespace avatarVrm {
struct SpringBoneAdapterConfig {
    std::string evaluatorId, layoutId, skeletonId;
    uint64_t layoutVersion = 0;
    // Full owner skeleton order, mapped by exact runtime identity.
    std::vector<std::string> jointIds;
    vrmRig::SpringBoneRig rig;
    vrmRig::SpringBoneTransform placement;
    double fixedStepSeconds = 0; // explicit positive runtime tick, no default
    std::vector<std::string> after;
};

// Instance-local, transactional owner evaluation in AR_PHASE_SECONDARY.
// First frame (also after reset) seeds history at its current animated pose;
// subsequent frames must be exactly one fixed tick apart. No accumulator.
// Runtime roots are world-space; non-root joints are parent-local metres.
// Configuration and callback code must outlive the registered runtime.
class SpringBoneAdapter {
public:
    explicit SpringBoneAdapter(SpringBoneAdapterConfig config);
    ~SpringBoneAdapter();
    SpringBoneAdapter(const SpringBoneAdapter&) = delete;
    SpringBoneAdapter& operator=(const SpringBoneAdapter&) = delete;
    ArEvaluatorDesc Descriptor() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace avatarVrm
