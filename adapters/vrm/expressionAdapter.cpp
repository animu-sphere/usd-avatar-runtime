#include "avatarVrm/ExpressionAdapter.h"
#include "vrmRig/MaterialColorSlots.h"
#include "pxr/base/gf/quatd.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <set>
#include <stdexcept>
#include <tuple>

#if !defined(VRMRIG_LOOKAT_DIRECTION_API) || VRMRIG_LOOKAT_DIRECTION_API < 1
#error "avatarVrmAdapter requires vrmRig with EvaluateDirection support; rebuild/install the owner package"
#endif

namespace avatarVrm {
namespace {
bool valid(const InputIdentity& i) {
    return !i.source.empty() && !i.actor.empty() && i.channel.find(':') != std::string::npos;
}
bool matches(const InputIdentity& i, const char* source, const char* actor, const char* channel) {
    return i.source == source && i.actor == actor && i.channel == channel;
}
void emit(const ArEvaluationContext& c, const char* code, const std::string& subject,
          const std::string& message, ArStatus status = AR_OK, uint32_t severity = AR_SEVERITY_WARNING) {
    ArDiagnostic d{AR_HEADER(ArDiagnostic)};
    d.code = code; d.origin = "usd-vrm-plugins.vrmRig";
    d.subject = subject.c_str(); d.message = message.c_str();
    d.status = status; d.severity = severity;
    if (c.diagnostics.emit) c.diagnostics.emit(c.diagnostics.user_data, &d);
}
ArStatus failure(const ArEvaluationContext& c, const char* code, const std::string& subject,
                 const char* message) {
    emit(c, code, subject, message, AR_INVALID_STATE, AR_SEVERITY_ERROR);
    return AR_INVALID_STATE;
}
uint32_t materialIndex(const ArStateView& v, const std::string& material, const char* input) {
    for (uint32_t i = 0; i < v.material_count; ++i)
        if (material == v.materials[i].material_id && std::strcmp(input, v.materials[i].input_id) == 0) return i;
    return v.material_count;
}
uint32_t jointIndex(const ArStateView& v, const std::string& skeleton, const std::string& joint) {
    for (uint32_t i = 0; i < v.joint_count; ++i)
        if (skeleton == v.joints[i].skeleton_id && joint == v.joints[i].joint_id) return i;
    return v.joint_count;
}
pxr::GfQuatd quaternion(const ArTransform& t) {
    return pxr::GfQuatd(t.rotation[3], pxr::GfVec3d(t.rotation[0], t.rotation[1], t.rotation[2]));
}
bool narrowable(double value) {
    return std::isfinite(value) && std::abs(value) <= std::numeric_limits<float>::max();
}
} // namespace

struct ExpressionAdapter::Impl {
    ExpressionAdapterConfig config;
    vrmRig::ExpressionResolver resolver;
    std::optional<vrmRig::LookAtEvaluator> gaze;
    std::vector<const char*> after;
    std::vector<ArCapability> capabilities;

    explicit Impl(ExpressionAdapterConfig c) : config(std::move(c)), resolver(config.expressions) {
        if (config.evaluatorId.empty() || config.layoutId.empty() || !config.layoutVersion)
            throw std::invalid_argument("VRM adapter requires evaluator and layout identity/version");
        std::set<std::string> names, targets;
        std::set<std::tuple<std::string, std::string, std::string>> identities;
        std::set<std::pair<std::string, std::string>> destinations;
        for (const auto& b : config.inputs) {
            if (!valid(b.input) || b.expression.empty() || !names.insert(b.expression).second ||
                !identities.emplace(b.input.source, b.input.actor, b.input.channel).second)
                throw std::invalid_argument("VRM expression input mapping is incomplete or ambiguous");
        }
        for (const auto& b : config.morphs) {
            if (b.ownerTarget.empty() || b.mesh.empty() || b.target.empty() ||
                !targets.insert(b.ownerTarget).second || !destinations.emplace(b.mesh, b.target).second)
                throw std::invalid_argument("VRM morph mapping is incomplete or ambiguous");
        }
        if (config.lookAt) {
            if ((config.lookAt->type != vrmRig::LookAtType::Expression &&
                 config.lookAt->type != vrmRig::LookAtType::Bone) || !valid(config.gaze) ||
                config.headSkeleton.empty() || config.headJoint.empty() ||
                identities.count({config.gaze.source, config.gaze.actor, config.gaze.channel}))
                throw std::invalid_argument("VRM LookAt requires a supported rig and distinct gaze/head binding");
            gaze.emplace(*config.lookAt);
        }
        if (gaze && config.lookAt->type == vrmRig::LookAtType::Bone) {
            std::set<std::string> ownerEyes;
            for (const auto& eye : {config.lookAt->leftEyeJoint, config.lookAt->rightEyeJoint})
                if (!eye.empty() && !ownerEyes.insert(eye).second)
                    throw std::invalid_argument("VRM LookAt eye identities must be distinct");
            if (ownerEyes.empty())
                throw std::invalid_argument("VRM bone LookAt requires at least one named eye");
            std::set<std::pair<std::string, std::string>> runtimeEyes;
            for (const auto& eye : config.eyes) {
                double norm = 0;
                for (double value : eye.restRotation) {
                    if (!std::isfinite(value))
                        throw std::invalid_argument("VRM eye rest rotation must be finite");
                    norm += value * value;
                }
                if (!ownerEyes.erase(eye.ownerJoint) || eye.skeleton != config.headSkeleton ||
                    eye.joint.empty() || eye.joint == config.headJoint ||
                    !runtimeEyes.emplace(eye.skeleton, eye.joint).second || std::abs(norm - 1.0) > 1e-6)
                    throw std::invalid_argument("VRM eye mapping/rest rotation is incomplete or ambiguous");
            }
            if (!ownerEyes.empty())
                throw std::invalid_argument("Every named VRM eye requires a runtime binding");
        } else if (!config.eyes.empty()) {
            throw std::invalid_argument("VRM eye bindings require bone LookAt");
        }
        for (const auto& id : config.after) after.push_back(id.c_str());
        capabilities.push_back({"avatar.vrm.expression.effects", 1});
        if (gaze) capabilities.push_back({config.lookAt->type == vrmRig::LookAtType::Bone
            ? "avatar.vrm.lookAt.bone" : "avatar.vrm.lookAt.expression", 1});
    }

    ArStatus worldPose(const ArEvaluationContext& c, uint32_t joint, bool translation,
                       const char* scaleCode, pxr::GfVec3d& position, pxr::GfQuatd& rotation) const {
        const auto& v = *c.working;
        std::vector<uint32_t> chain;
        for (int32_t i = int32_t(joint); i >= 0; i = v.joints[i].parent_index) chain.push_back(uint32_t(i));
        position = pxr::GfVec3d(0);
        rotation = pxr::GfQuatd(1);
        for (auto i = chain.rbegin(); i != chain.rend(); ++i) {
            const auto& t = v.joints[*i].local;
            for (double scale : t.scale)
                if (std::abs(scale - 1.0) > 1e-6)
                    return failure(c, scaleCode, v.joints[joint].joint_id, "LookAt reference ancestry must have unit scale");
            if (translation) position += rotation.Transform(pxr::GfVec3d(t.translation[0], t.translation[1], t.translation[2]));
            rotation = (rotation * quaternion(t)).GetNormalized();
        }
        return AR_OK;
    }

    ArStatus evaluate(const ArEvaluationContext& c, const ArStateWriter& writer) const {
        const auto& v = *c.working;
        if (!v.layout_id || config.layoutId != v.layout_id || config.layoutVersion != v.layout_version)
            return failure(c, "VRM_ADAPTER_LAYOUT", config.layoutId, "Binding layout identity/version mismatch");

        std::vector<uint32_t> eyeIndices;
        for (const auto& eye : config.eyes) {
            const auto index = jointIndex(v, eye.skeleton, eye.joint);
            const auto head = jointIndex(v, config.headSkeleton, config.headJoint);
            if (head == v.joint_count)
                return failure(c, "VRM_ADAPTER_HEAD", config.headJoint, "Bound head joint is missing");
            if (index == v.joint_count)
                return failure(c, "VRM_ADAPTER_EYE", eye.ownerJoint, "Bound eye joint is missing");
            if (v.joints[index].parent_index != int32_t(head))
                return failure(c, "VRM_ADAPTER_EYE_PARENT", eye.ownerJoint, "Bone LookAt requires eyes parented directly to the head");
            eyeIndices.push_back(index);
        }

        // Validate every rig effect before calling its owner, even if this frame
        // reports no expression touching it. Unsupported output never vanishes.
        std::vector<uint32_t> morphIndices;
        for (const auto& b : config.morphs) {
            uint32_t index = v.blend_shape_count;
            for (uint32_t i = 0; i < v.blend_shape_count; ++i)
                if (b.mesh == v.blend_shapes[i].mesh_id && b.target == v.blend_shapes[i].target_id) index = i;
            if (index == v.blend_shape_count)
                return failure(c, "VRM_ADAPTER_TARGET", b.ownerTarget, "Bound morph target is missing from snapshot layout");
            morphIndices.push_back(index);
        }
        for (const auto& expression : config.expressions.GetExpressions()) {
            for (const auto& bind : expression.morphTargets) {
                auto found = std::find_if(config.morphs.begin(), config.morphs.end(),
                    [&](const MorphBinding& b) { return b.ownerTarget == bind.target; });
                if (found == config.morphs.end())
                    return failure(c, "VRM_ADAPTER_TARGET", bind.target, "Owner morph target has no runtime binding");
            }
            for (const auto& bind : expression.materialColors) {
                const auto* slot = vrmRig::FindMaterialColorSlot(bind.colorType);
                if (!slot) return failure(c, "VRM_ADAPTER_OUTPUT", bind.colorType, "Unsupported owner material color slot");
                auto color = materialIndex(v, bind.material, slot->colorInput);
                if (color == v.material_count || v.materials[color].value_type != AR_VALUE_VEC3)
                    return failure(c, "VRM_ADAPTER_TARGET", bind.material, "Canonical RGB material input is missing or mistyped");
                if (slot->alphaInput) {
                    auto alpha = materialIndex(v, bind.material, slot->alphaInput);
                    if (alpha == v.material_count || v.materials[alpha].value_type != AR_VALUE_SCALAR)
                        return failure(c, "VRM_ADAPTER_TARGET", bind.material, "Canonical alpha material input is missing or mistyped");
                }
            }
        }

        openstrata::motion::MotionChannelSet weights;
        for (const auto& b : config.inputs) {
            for (uint32_t i = 0; i < c.input->scalar_count; ++i) {
                const auto& scalar = c.input->scalars[i];
                if (matches(b.input, scalar.source_id, scalar.actor_id, scalar.channel_id)) {
                    // Owner clamps to [0,1]; saturating the representation keeps
                    // finite doubles out of infinity during float marshalling.
                    double value = std::clamp(scalar.value, -double(std::numeric_limits<float>::max()),
                                              double(std::numeric_limits<float>::max()));
                    weights.Set(b.expression, float(value));
                }
            }
        }
        if (gaze) {
            const ArGazeInput* selected = nullptr;
            for (uint32_t i = 0; i < c.input->gaze_count; ++i) {
                const auto& observation = c.input->gazes[i];
                if (matches(config.gaze, observation.source_id, observation.actor_id, observation.channel_id)) selected = &observation;
            }
            if (selected && selected->validity != AR_OBSERVATION_VALID) {
                emit(c, "VRM_ADAPTER_GAZE_UNAVAILABLE", config.gaze.channel, "Stale/unavailable gaze contributes no LookAt output");
            } else if (selected) {
                const bool direction = selected->kind == AR_GAZE_DIRECTION;
                const auto head = jointIndex(v, config.headSkeleton, config.headJoint);
                if (head == v.joint_count)
                    return failure(c, "VRM_ADAPTER_HEAD", config.headJoint, "Bound head joint is missing");
                pxr::GfQuatd rotation(1.0);
                pxr::GfVec3d position(0.0);
                auto status = worldPose(c, head, !direction, "VRM_ADAPTER_HEAD_SCALE", position, rotation);
                if (status != AR_OK) return status;
                pxr::GfVec3d target(selected->value[0], selected->value[1], selected->value[2]);
                if (selected->space == AR_GAZE_JOINT_LOCAL) {
                    const auto reference = jointIndex(v, selected->skeleton_id, selected->joint_id);
                    if (reference == v.joint_count)
                        return failure(c, "VRM_ADAPTER_GAZE_JOINT", selected->joint_id, "Bound gaze reference joint is missing");
                    pxr::GfVec3d referencePosition;
                    pxr::GfQuatd referenceRotation;
                    status = worldPose(c, reference, !direction, "VRM_ADAPTER_GAZE_SCALE", referencePosition, referenceRotation);
                    if (status != AR_OK) return status;
                    target = referenceRotation.Transform(target);
                    if (!direction) target += referencePosition;
                }
                for (int i = 0; i < 3; ++i)
                    if (!narrowable(position[i]) || !narrowable(target[i]))
                        return failure(c, "VRM_ADAPTER_RANGE", config.headJoint, "Head/target cannot be represented by owner float values");
                vrmRig::LookAtInput input;
                input.timestamp = c.input->evaluation_seconds;
                input.head.position = pxr::GfVec3f(position);
                input.head.orientation = pxr::GfQuatf(rotation);
                vrmRig::LookAtDiagnostics diagnostics;
                vrmRig::ResolvedLookAt result;
                if (direction) {
                    // Core has already checked the unit vector. Remove allowed
                    // double norm error before float marshalling so rounding
                    // cannot turn a valid boundary sample into invalid gaze.
                    result = gaze->EvaluateDirection(pxr::GfVec3f(target.GetNormalized()), input.head, input.timestamp, &diagnostics);
                } else {
                    input.target = pxr::GfVec3f(target);
                    result = gaze->Evaluate(input, &diagnostics);
                }
                for (const auto& warning : diagnostics.warnings) emit(c, "VRM_LOOKAT_WARNING", config.headJoint, warning);
                for (const auto& eye : result.eyeRotations) {
                    const auto binding = std::find_if(config.eyes.begin(), config.eyes.end(),
                        [&](const EyeBinding& b) { return b.ownerJoint == eye.joint; });
                    if (binding == config.eyes.end())
                        return failure(c, "VRM_ADAPTER_EYE", eye.joint, "Owner eye rotation has no runtime binding");
                    const auto& rest = binding->restRotation;
                    // Match the owner's bake caller: resolved gaze * authored
                    // rest, replacing animated rotation rather than accumulating.
                    const auto eyeRotation = (pxr::GfQuatd(eye.rotation) *
                        pxr::GfQuatd(rest[3], pxr::GfVec3d(rest[0], rest[1], rest[2]))).GetNormalized();
                    const auto index = eyeIndices[size_t(binding - config.eyes.begin())];
                    auto local = v.joints[index].local;
                    for (int i = 0; i < 3; ++i) local.rotation[i] = eyeRotation.GetImaginary()[i];
                    local.rotation[3] = eyeRotation.GetReal();
                    const auto eyeStatus = writer.set_joint(writer.context, index, &local);
                    if (eyeStatus != AR_OK) return eyeStatus;
                }
                for (const auto& value : result.expressions.entries) {
                    if (weights.Find(value.name))
                        emit(c, "VRM_ADAPTER_GAZE_PRECEDENCE", value.name, "Selected LookAt contribution replaces mapped input of the same name", AR_OK, AR_SEVERITY_INFO);
                    weights.Set(value.name, value.value);
                }
            }
        }
        vrmRig::ExpressionDiagnostics diagnostics;
        const auto result = resolver.Resolve(weights, &diagnostics);
        for (const auto& name : diagnostics.unresolvedNames) emit(c, "VRM_EXPRESSION_UNRESOLVED", name, "Expression is not declared by this rig");
        for (const auto& name : diagnostics.clampedNames) emit(c, "VRM_EXPRESSION_CLAMPED", name, "Owner clamped the expression weight");
        for (const auto& name : diagnostics.suppressedNames) emit(c, "VRM_EXPRESSION_SUPPRESSED", name, "Owner expression arbitration suppressed this contribution", AR_OK, AR_SEVERITY_INFO);
        for (const auto& warning : diagnostics.warnings) emit(c, "VRM_EXPRESSION_WARNING", config.evaluatorId, warning);
        for (const auto& target : result.morphTargets) {
            const auto b = std::find_if(config.morphs.begin(), config.morphs.end(),
                [&](const MorphBinding& binding) { return binding.ownerTarget == target.target; });
            auto index = morphIndices[size_t(b - config.morphs.begin())];
            auto status = writer.set_blend_shape(writer.context, index, target.weight);
            if (status != AR_OK) return status;
        }
        for (const auto& material : result.materialColors) {
            const auto* slot = vrmRig::FindMaterialColorSlot(material.colorType);
            auto color = materialIndex(v, material.material, slot->colorInput);
            auto alpha = slot->alphaInput ? materialIndex(v, material.material, slot->alphaInput) : v.material_count;
            pxr::GfVec4f base;
            for (int i = 0; i < 3; ++i) {
                if (!narrowable(v.materials[color].value[i]))
                    return failure(c, "VRM_ADAPTER_RANGE", material.material, "Material base cannot be represented by owner float values");
                base[i] = float(v.materials[color].value[i]);
            }
            base[3] = 1.0f;
            if (alpha != v.material_count) {
                if (!narrowable(v.materials[alpha].value[0]))
                    return failure(c, "VRM_ADAPTER_RANGE", material.material, "Material alpha cannot be represented by owner float values");
                base[3] = float(v.materials[alpha].value[0]);
            }
            auto value = material.Apply(base);
            double rgb[4]{value[0], value[1], value[2], 0};
            auto status = writer.set_material(writer.context, color, 1, rgb);
            if (status != AR_OK) return status;
            if (alpha != v.material_count) {
                double a[4]{value[3], 0, 0, 0};
                status = writer.set_material(writer.context, alpha, 1, a);
                if (status != AR_OK) return status;
            }
        }
        return AR_OK;
    }
    static ArStatus AR_CALL callback(void* user, void*, const ArEvaluationContext* c, const ArStateWriter* w) {
        return static_cast<const Impl*>(user)->evaluate(*c, *w);
    }
};

ExpressionAdapter::ExpressionAdapter(ExpressionAdapterConfig config) : impl_(std::make_unique<Impl>(std::move(config))) {}
ExpressionAdapter::~ExpressionAdapter() = default;
ArEvaluatorDesc ExpressionAdapter::Descriptor() const {
    ArEvaluatorDesc d{AR_HEADER(ArEvaluatorDesc)};
    d.id = impl_->config.evaluatorId.c_str(); d.provider_id = "usd-vrm-plugins.vrmRig";
    d.provider_version = AR_VRM_PROVIDER_VERSION;
    // Atomic owner sequence: LookAt produces expression weights or eye pose;
    // the expression resolver runs once. No cross-frame intermediate state.
    d.phase = AR_PHASE_EXPRESSIONS;
    d.reads = AR_DOMAIN_MATERIAL | (impl_->gaze ? AR_DOMAIN_POSE : 0);
    d.writes = AR_DOMAIN_DEFORMATION | AR_DOMAIN_MATERIAL | (impl_->config.eyes.empty() ? 0 : AR_DOMAIN_POSE);
    d.after = impl_->after.data(); d.after_count = uint32_t(impl_->after.size());
    d.supplies = impl_->capabilities.data(); d.supply_count = uint32_t(impl_->capabilities.size());
    d.user_data = impl_.get(); d.evaluate = Impl::callback;
    return d;
}
} // namespace avatarVrm
