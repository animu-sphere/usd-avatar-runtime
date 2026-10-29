#pragma once
#include <avatarRuntime/api.h>
#include <mmdPhysicsAdapter/Adapter.h>
#include <array>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace avatarMmd {
struct JointBinding {
    std::string skeleton, joint;
    int32_t parent = -1; // canonical joint index, not runtime array index
};
struct MorphBinding {
    int32_t morph; // canonical leaf index; host must select compatible output types
    std::string mesh, target;
};
enum class MaterialField : uint32_t {
    DiffuseColor, SpecularColor, SpecularPower, AmbientColor, EdgeColor, EdgeSize,
    TextureTint, SphereTint, ToonTint
};
enum class MaterialValueMode : uint32_t { Resolved, Multiply, Add };
struct MaterialInputBinding {
    int32_t material; // canonical material index
    MaterialField field;
    std::string target, input; // runtime material/input identity
    std::array<double, 4> base{}; // immutable base; unused components must be zero
    // Sampled texture contributions need independent affine coefficients.
    // Multiply/Add are allowed only for tint fields, as a complete pair;
    // base must be zero. Resolved preserves the existing base*M+A contract.
    MaterialValueMode mode = MaterialValueMode::Resolved;
};
struct PhysicsAdapterConfig {
    mmd::control::Evaluator evaluator;
    mmd::binding::BoundMotion motion;
    mmd::physics::Adapter coupling;
    std::string preId, postId, layoutId;
    uint64_t layoutVersion = 1;
    // Exact canonical joint order/rest space of evaluator AND coupling.
    std::vector<JointBinding> joints;
    std::vector<MorphBinding> morphs;
    // Host-admitted primary UV leaf indices, and immutable canonical st for
    // one mesh in canonical vertex order. Additional UVs remain unsupported.
    std::vector<int32_t> uvMorphs;
    std::string uvMesh;
    std::vector<std::array<double, 2>> baseUvs;
    // Host-admitted material leaves. Each bound material requires all nine
    // typed fields. Each sampled tint binds either a legacy resolved value
    // or independent Multiply/Add inputs (required for live rendering).
    std::vector<int32_t> materialMorphs;
    std::vector<MaterialInputBinding> materials;
    std::vector<std::string> visibilityTargets;
    std::vector<std::string> after;
    // MMD frame = evaluation_seconds * framesPerSecond + frameOffset.
    double framesPerSecond = 30, frameOffset = 0;
    // Same rigid skeleton-to-world placement used when binding coupling.
    // Owner poses stay skeleton-local; runtime root joints are world-space.
    mmd::control::JointTransform placement;
    // Host resolves composed paths to its own world handles. Submit replaces
    // pending commands for this instance/tick; apply them only inside Step.
    // Snapshot must return body transforms for exactly this completed tick.
    // No callback may step/reset the world. Code/captures outlive registration.
    std::function<ArStatus(const ArEvaluationContext&, std::span<const mmd::physics::BodyTransform>)> submit;
    std::function<ArStatus(const ArEvaluationContext&, std::vector<mmd::physics::BodyTransform>&)> snapshot;
};

// Optional C++ adapter. No owner types/dependencies enter the core C ABI.
// Register BOTH descriptors and bind PostDescriptor().id as the world's cut.
// Immutable owner config, per-instance transactional continuations. Keep alive
// until runtime destruction. Unsupported/unmapped morph or hidden state fails.
// Constructor throws invalid_argument for incomplete/ambiguous configuration.
class PhysicsAdapter {
public:
    explicit PhysicsAdapter(PhysicsAdapterConfig config);
    ~PhysicsAdapter();
    PhysicsAdapter(const PhysicsAdapter&) = delete;
    PhysicsAdapter& operator=(const PhysicsAdapter&) = delete;
    ArEvaluatorDesc PreDescriptor() const;
    ArEvaluatorDesc PostDescriptor() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace avatarMmd
