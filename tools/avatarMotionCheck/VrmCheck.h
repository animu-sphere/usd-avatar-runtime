#pragma once
#include "avatarMotion/MotionPoseInputBridge.h"
#include "motionRetarget/PoseRetargeter.h"
#include "avatarVrmUsd/ExpressionBinding.h"
#include "avatarVrmUsd/LookAtBinding.h"
#include "vrmRig/MaterialColorSlots.h"
#include "pxr/base/gf/rotation.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>

// Tool-local owner oracle. It derives the head from the separately retargeted
// pose, never from runtime output, so an ordering error cannot validate itself.
namespace avatarMotionCheck {
namespace motion = openstrata::motion;
inline void verify(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
struct VrmCheck {
    avatarVrm::ExpressionAdapterConfig config;
    avatarMotion::MotionPoseInputBridgeConfig inputConfig;
    const ArStateView& baseline;
    std::map<std::string,float> probeWeights;
    std::optional<pxr::GfVec3f> probeGaze;

    VrmCheck(const avatarVrmUsd::ExpressionBinding& expressions,
             const avatarVrmUsd::LookAtBinding& look,
             std::map<std::string,float> weights, std::optional<pxr::GfVec3f> gaze)
        : config(look.AdapterConfig("check.vrm",{"check.motion","avatar","gaze:point"},{"check.motion"})),
          baseline(expressions.Baseline()), probeWeights(std::move(weights)), probeGaze(gaze) {
        expressions.ApplyTo(config);
        inputConfig.source = "check.motion"; inputConfig.actor = "avatar";
        inputConfig.gazeChannel = "gaze:point";
        for (const auto& e : expressions.Rig().GetExpressions()) {
            const auto channel = "vrm:" + e.name;
            inputConfig.channels.push_back({channel,channel});
            config.inputs.push_back({{inputConfig.source,inputConfig.actor,channel},e.name});
        }
        for (const auto& weight : probeWeights)
            verify(expressions.Rig().Find(weight.first) != nullptr,"Probe expression is not declared by avatar");
    }
    motion::MotionPose Select(const motion::MotionPose& sampled, bool probes) const {
        auto selected = sampled;
        if (probes) {
            for (const auto& weight : probeWeights) selected.channels.Set("vrm:"+weight.first,weight.second);
            if (probeGaze) selected.lookAtTarget = probeGaze;
        }
        return selected;
    }
    vrmRig::LookAtHead Head(const avatarUsd::SkeletonBinding& binding,
                           const motion::RetargetedPose& pose) const {
        std::vector<pxr::GfQuatd> rotations;
        std::vector<pxr::GfVec3d> positions;
        const auto& joints = binding.Skeleton().GetJoints();
        for (size_t i = 0; i < joints.size(); ++i) {
            auto rotation = pxr::GfQuatd(pose.rotations[i]).GetNormalized();
            auto position = pxr::GfVec3d(pose.translations[i]);
            if (joints[i].parent < 0) {
                const auto& p = binding.RootPlacement();
                const pxr::GfQuatd placement(p.rotation[3],pxr::GfVec3d(p.rotation[0],p.rotation[1],p.rotation[2]));
                position = placement.Transform(position)+pxr::GfVec3d(p.translation[0],p.translation[1],p.translation[2]);
                rotation = (placement*rotation).GetNormalized();
            } else {
                const auto parent = size_t(joints[i].parent);
                position = positions[parent]+rotations[parent].Transform(position);
                rotation = (rotations[parent]*rotation).GetNormalized();
            }
            rotations.push_back(rotation); positions.push_back(position);
            if (binding.JointIds()[i] == config.headJoint)
                return {pxr::GfQuatf(rotation),pxr::GfVec3f(position)};
        }
        throw std::runtime_error("Owner head is missing");
    }
    struct Expected {
        vrmRig::ResolvedExpressions expressions;
        std::map<std::string,pxr::GfQuatd> eyes;
    };
    Expected Resolve(const avatarUsd::SkeletonBinding& binding, const motion::RetargetedPose& pose,
                     const motion::MotionPose& selected, bool stale, double evaluationSeconds) const {
        motion::MotionChannelSet weights;
        for (const auto& input : config.inputs)
            if (const auto* value = selected.channels.Find(input.input.channel)) weights.Set(input.expression,*value);
        Expected result;
        if (selected.lookAtTarget && !stale) {
            vrmRig::LookAtInput input;
            input.head = Head(binding,pose); input.target = selected.lookAtTarget; input.timestamp = evaluationSeconds;
            const auto gaze = vrmRig::LookAtEvaluator(*config.lookAt).Evaluate(input);
            for (const auto& value : gaze.expressions.entries) weights.Set(value.name,value.value);
            for (const auto& eye : gaze.eyeRotations) {
                const auto b = std::find_if(config.eyes.begin(),config.eyes.end(),
                    [&](const avatarVrm::EyeBinding& e) { return e.ownerJoint == eye.joint; });
                verify(b != config.eyes.end(),"Owner eye mapping missing");
                const auto& q = b->restRotation;
                result.eyes.emplace(b->joint,(pxr::GfQuatd(eye.rotation)*
                    pxr::GfQuatd(q[3],pxr::GfVec3d(q[0],q[1],q[2]))).GetNormalized());
            }
        }
        result.expressions = vrmRig::ExpressionResolver(config.expressions).Resolve(weights);
        return result;
    }
    double Compare(const ArStateView& state, const Expected& expected) const {
        verify(state.blend_shape_count == baseline.blend_shape_count && state.material_count == baseline.material_count,
               "VRM output layout count mismatch");
        double error = 0;
        auto delta = [&](double a, double b) {
            verify(std::isfinite(a) && std::isfinite(b),"Nonfinite VRM output");
            error = std::max(error,std::abs(a-b));
        };
        for (uint32_t i = 0; i < state.blend_shape_count; ++i) {
            const auto& actual = state.blend_shapes[i]; const auto& base = baseline.blend_shapes[i];
            verify(std::string(actual.mesh_id) == base.mesh_id && std::string(actual.target_id) == base.target_id,
                   "Morph identity mismatch");
            double weight = base.weight;
            for (const auto& e : expected.expressions.morphTargets)
                for (const auto& b : config.morphs)
                    if (b.ownerTarget == e.target && b.mesh == actual.mesh_id && b.target == actual.target_id) weight = e.weight;
            delta(actual.weight,weight);
        }
        for (uint32_t i = 0; i < state.material_count; ++i) {
            const auto& actual = state.materials[i]; const auto& base = baseline.materials[i];
            verify(std::string(actual.material_id) == base.material_id && std::string(actual.input_id) == base.input_id &&
                   actual.value_type == base.value_type,"Material identity/type mismatch");
            double value[4]; std::copy_n(base.value,4,value); bool overridden = base.overridden != 0;
            for (const auto& e : expected.expressions.materialColors) {
                const auto* slot = vrmRig::FindMaterialColorSlot(e.colorType);
                verify(slot != nullptr,"Owner material slot missing");
                if (e.material != actual.material_id) continue;
                if (std::string(actual.input_id) != slot->colorInput &&
                    (!slot->alphaInput || std::string(actual.input_id) != slot->alphaInput)) continue;
                pxr::GfVec4f original(0,0,0,1);
                for (uint32_t j = 0; j < baseline.material_count; ++j) {
                    const auto& authored = baseline.materials[j];
                    if (e.material != authored.material_id) continue;
                    if (std::string(authored.input_id) == slot->colorInput)
                        for (int k = 0; k < 3; ++k) original[k] = float(authored.value[k]);
                    if (slot->alphaInput && std::string(authored.input_id) == slot->alphaInput) original[3] = float(authored.value[0]);
                }
                const auto resolved = e.Apply(original); overridden = true;
                if (actual.value_type == AR_VALUE_VEC3) for (int k = 0; k < 3; ++k) value[k] = resolved[k];
                else value[0] = resolved[3];
            }
            verify(actual.overridden == uint32_t(overridden),"Material override flag mismatch");
            for (int k = 0; k < 4; ++k) delta(actual.value[k],value[k]);
        }
        verify(error <= 1e-6,"Owner/runtime VRM output mismatch");
        return error;
    }
};
} // namespace avatarMotionCheck
