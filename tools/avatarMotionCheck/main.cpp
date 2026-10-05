#include "avatarMotionUsd/StageClip.h"
#include "avatarVrmUsd/HumanoidBinding.h"
#include "avatarMotion/ClipPoseAdapter.h"
#include "pxr/usd/usd/primRange.h"
#include "pxr/usd/usdSkel/skeleton.h"
#include "pxr/base/gf/rotation.h"
#ifdef AR_CHECK_VRM
#include "vrmRig/RequiredBones.h"
#endif
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>

namespace {
namespace motion = openstrata::motion;
void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
void ok(ArStatus status, const char* operation) {
    require(status == AR_OK, std::string(operation) + " failed: " + std::to_string(status));
}
struct Host {
    ArRuntimeApi api{};
    ArRuntime runtime = 0;
    std::vector<ArSnapshot> retained;
    Host() { ok(arGetApi(AR_ABI_VERSION,sizeof(api),&api),"get API"); ok(api.create_runtime(&runtime),"create runtime"); }
    ~Host() {
        if (runtime) api.destroy_runtime(runtime);
        for (auto snapshot : retained) api.release_snapshot(snapshot);
    }
};
struct Diagnostics {
    std::map<std::string,size_t> codes;
    std::map<std::string,std::set<std::string>> subjects;
    bool error = false;
    static void AR_CALL emit(void* data, const ArDiagnostic* d) {
        auto& self = *static_cast<Diagnostics*>(data);
        const auto key = std::string(d->origin) + ":" + d->code;
        ++self.codes[key];
        if (d->subject && *d->subject) self.subjects[key].insert(d->subject);
        if (d->severity == AR_SEVERITY_ERROR) self.error = true;
    }
};
double compare(const ArStateView& v, const avatarUsd::SkeletonBinding& binding,
               const motion::RetargetedPose& expected) {
    require(v.joint_count == binding.JointIds().size(),"joint count mismatch");
    double error = 0;
    auto delta = [&](double a, double b) {
        require(std::isfinite(a) && std::isfinite(b),"nonfinite output");
        error = std::max(error,std::abs(a-b));
    };
    for (uint32_t i = 0; i < v.joint_count; ++i) {
        require(v.joints[i].joint_id == binding.JointIds()[i],"joint identity mismatch");
        require(v.joints[i].skeleton_id == binding.SkeletonId(),"skeleton identity mismatch");
        require(v.joints[i].parent_index == binding.Skeleton().GetJoints()[i].parent,"parent mismatch");
        auto position = pxr::GfVec3d(expected.translations[i]);
        auto rotation = pxr::GfQuatd(expected.rotations[i]).GetNormalized();
        if (v.joints[i].parent_index < 0) {
            const auto& p = binding.RootPlacement();
            const pxr::GfQuatd q(p.rotation[3],pxr::GfVec3d(p.rotation[0],p.rotation[1],p.rotation[2]));
            position = pxr::GfRotation(q).TransformDir(position) + pxr::GfVec3d(p.translation[0],p.translation[1],p.translation[2]);
            rotation = (q * rotation).GetNormalized();
        }
        for (int k = 0; k < 3; ++k) {
            delta(v.joints[i].local.translation[k],position[k]);
            delta(v.joints[i].local.rotation[k],rotation.GetImaginary()[k]);
            delta(v.joints[i].local.scale[k],binding.Skeleton().GetJoints()[i].restScale[k]);
        }
        delta(v.joints[i].local.rotation[3],rotation.GetReal());
    }
    require(error <= 1e-6,"owner/runtime pose mismatch: " + std::to_string(error));
    return error;
}
void check(const avatarUsd::SkeletonBinding& binding, const char* file) {
    auto stage = pxr::UsdStage::Open(file);
    require(bool(stage),std::string("Cannot open motion: ") + file);
    pxr::SdfPath path;
    for (const auto& prim : stage->Traverse()) if (prim.IsA<pxr::UsdSkelSkeleton>()) {
        require(path.IsEmpty(),"Motion must contain exactly one skeleton"); path = prim.GetPath();
    }
    require(!path.IsEmpty(),"Motion contains no skeleton");
    avatarMotionUsd::StageClip loaded(stage,path);
    stage.Reset();
    for (const auto& warning : loaded.Read().warnings) std::cerr << "motionUsd: " << warning << '\n';
    const auto& baseline = binding.Baseline();
    avatarMotion::ClipPoseAdapterConfig c;
    c.evaluatorId = "check.motion"; c.layoutId = baseline.layout_id; c.layoutVersion = baseline.layout_version;
    c.skeletonId = binding.SkeletonId(); c.skeleton = binding.Skeleton(); c.map = binding.HumanoidMap();
    c.jointIds = binding.JointIds(); c.rootPlacement = binding.RootPlacement();
    c.clip = loaded.Read().clip; c.sourceRest = loaded.SourceRest();
    c.clockScale = 1.25; c.clockOffset = 2.0;
#ifdef AR_CHECK_VRM
    c.options.requiredBones = vrmRig::GetRequiredBones();
#endif
    require(!c.clip.samples.empty(),"Motion clip is empty");
    avatarMotion::ClipPoseAdapter adapter(c);
    motion::PoseRetargeter owner(c.skeleton,c.map,c.sourceRest,c.options);
    Host host;
    Diagnostics diagnostics; ArDiagnosticSink sink{&diagnostics,Diagnostics::emit};
    auto desc = adapter.Descriptor(); ok(host.api.register_evaluator(host.runtime,&desc,&sink),"register");
    ArInstanceDesc d{AR_HEADER(ArInstanceDesc)};
    d.generation = 1; d.layout_id = baseline.layout_id; d.layout_version = baseline.layout_version; d.initial_state = baseline;
    d.evaluators = &desc.id; d.evaluator_count = 1;
    d.bound_capabilities = desc.supplies; d.bound_capability_count = desc.supply_count;
    ArInstance instance = 0; ok(host.api.create_instance(host.runtime,&d,&sink,&instance),"create instance");
    std::set<double> times;
    for (size_t i = 0; i < c.clip.samples.size(); ++i) {
        times.insert(c.clip.samples[i].timestamp);
        if (i) times.insert(c.clip.samples[i-1].timestamp + (c.clip.samples[i].timestamp-c.clip.samples[i-1].timestamp)*0.5);
    }
    times.insert(c.clip.samples.front().timestamp - 0.1);
    times.insert(c.clip.samples.back().timestamp + 0.1);
    uint64_t frame = 0;
    double maxError = 0;
    std::set<uint32_t> changed;
    auto evaluate = [&](double seconds, uint64_t generation) {
        ArInputFrame input{AR_HEADER(ArInputFrame)};
        input.frame_id = ++frame; input.generation = generation; input.input_revision = frame;
        input.evaluation_seconds = seconds*c.clockScale+c.clockOffset;
        ArSnapshot snapshot = 0;
        ok(host.api.evaluate_frame(host.runtime,instance,&input,&sink,&snapshot),"evaluate");
        // Own immediately, including if a later verification throws.
        host.retained.push_back(snapshot);
        ArStateView view{AR_HEADER(ArStateView)}; ok(host.api.get_snapshot(snapshot,&view),"get snapshot");
        const auto sampled = motion::SampleClip(c.clip,(input.evaluation_seconds-c.clockOffset)/c.clockScale);
        require(bool(sampled),"owner sample unavailable");
        maxError = std::max(maxError,compare(view,binding,owner.Retarget(*sampled.pose)));
        require(view.frame_id == input.frame_id && view.generation == generation &&
                view.evaluation_seconds == input.evaluation_seconds && view.input_revision == frame &&
                view.layout_id == c.layoutId && view.layout_version == c.layoutVersion && view.capability_count == 1,
                "snapshot identity mismatch");
        for (uint32_t i = 0; i < view.joint_count; ++i) {
            const auto& a = view.joints[i].local; const auto& b = baseline.joints[i].local;
            double difference = 0;
            for (int k = 0; k < 3; ++k) difference += std::abs(a.translation[k]-b.translation[k]);
            // Quaternion sign is immaterial when checking whether the pose changed.
            double dot = 0; for (int k = 0; k < 4; ++k) dot += a.rotation[k]*b.rotation[k];
            if (difference > 1e-5 || 1-std::abs(dot) > 1e-5) changed.insert(i);
        }
        if (host.retained.size() > 2) {
            ok(host.api.release_snapshot(host.retained[1]),"release intermediate snapshot");
            host.retained.erase(host.retained.begin()+1);
        }
    };
    for (double time : times) evaluate(time,1);
    ok(host.api.reset_instance(host.runtime,instance,2),"reset");
    evaluate(c.clip.samples.front().timestamp,2);
    require(!diagnostics.error,"error diagnostics reported");
    ok(host.api.destroy_runtime(host.runtime),"destroy runtime"); host.runtime = 0;
    ArStateView old{AR_HEADER(ArStateView)}; ok(host.api.get_snapshot(host.retained.front(),&old),"retained snapshot");
    require(old.frame_id == 1 && old.generation == 1,"retained snapshot changed after reset/destruction");
    compare(old,binding,owner.Retarget(*motion::SampleClip(c.clip,c.clip.samples.front().timestamp-0.1).pose));
    ArStateView reset{AR_HEADER(ArStateView)}; ok(host.api.get_snapshot(host.retained.back(),&reset),"retained reset snapshot");
    require(reset.frame_id == frame && reset.generation == 2,"reset snapshot identity changed");
    compare(reset,binding,owner.Retarget(*motion::SampleClip(c.clip,c.clip.samples.front().timestamp).pose));
    require(!changed.empty(),"Motion produced no observable avatar pose change");
    std::cout << std::setprecision(10) << file << ": samples=" << c.clip.samples.size()
        << " duration=" << c.clip.samples.back().timestamp-c.clip.samples.front().timestamp
        << " frames=" << frame << " joints=" << baseline.joint_count << " changed_joints=" << changed.size()
        << " max_error=" << maxError << " reset/retention=passed\n";
    for (const auto& entry : diagnostics.codes) {
        std::cout << "  " << entry.first << " count=" << entry.second;
        for (const auto& subject : diagnostics.subjects[entry.first]) std::cout << " subject=" << subject;
        std::cout << '\n';
    }
}
}
int main(int argc, char** argv) {
    try {
        if (argc < 3) { std::cerr << "Usage: avatarMotionCheck <avatar> <motion> [motion ...]\n"; return 2; }
        auto stage = pxr::UsdStage::Open(argv[1]);
        require(stage && stage->GetDefaultPrim(),"Avatar has no default prim or failed to open");
        avatarVrmUsd::HumanoidBinding binding(stage,{stage->GetDefaultPrim().GetPath(),{},"check.avatar",1});
        stage.Reset();
        for (int i = 2; i < argc; ++i) check(binding.Skeleton(),argv[i]);
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
