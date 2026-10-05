#include "avatarMotionUsd/StageClip.h"
#include "avatarVrmUsd/HumanoidBinding.h"
#include "avatarMotion/ClipPoseAdapter.h"
#include "pxr/usd/usd/primRange.h"
#include "pxr/usd/usdSkel/skeleton.h"
#include "pxr/base/gf/rotation.h"
#ifdef AR_CHECK_VRM
#include "vrmRig/RequiredBones.h"
#include "VrmCheck.h"
#endif
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <memory>
#include <limits>
#include <optional>

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
        if (d->severity == AR_SEVERITY_ERROR) {
            self.error = true;
            std::cerr << key << ": " << d->message << '\n';
        }
    }
};
double compare(const ArStateView& v, const avatarUsd::SkeletonBinding& binding,
               const motion::RetargetedPose& expected,
               const std::map<std::string,pxr::GfQuatd>& eyes = {}) {
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
        const auto eye = eyes.find(binding.JointIds()[i]);
        if (eye != eyes.end()) rotation = eye->second;
        // q and -q represent the same orientation.
        const auto& actual = v.joints[i].local.rotation;
        double dot = actual[3]*rotation.GetReal();
        for (int k = 0; k < 3; ++k) dot += actual[k]*rotation.GetImaginary()[k];
        if (dot < 0) rotation = -rotation;
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
void check(const avatarUsd::SkeletonBinding& binding, const char* file
#ifdef AR_CHECK_VRM
           , avatarMotionCheck::VrmCheck* vrm = nullptr
#endif
           ) {
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
    const auto& baseline =
#ifdef AR_CHECK_VRM
        vrm ? vrm->baseline :
#endif
        binding.Baseline();
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
#ifdef AR_CHECK_VRM
    std::unique_ptr<avatarVrm::ExpressionAdapter> vrmAdapter;
    std::unique_ptr<avatarMotion::InputAssembler> assembler;
    if (vrm) {
        vrmAdapter = std::make_unique<avatarVrm::ExpressionAdapter>(vrm->config);
        vrm->inputConfig.clockScale = c.clockScale; vrm->inputConfig.clockOffset = c.clockOffset;
        assembler = std::make_unique<avatarMotion::InputAssembler>(vrm->inputConfig);
    }
#endif
    Host host;
    Diagnostics diagnostics; ArDiagnosticSink sink{&diagnostics,Diagnostics::emit};
    auto desc = adapter.Descriptor(); ok(host.api.register_evaluator(host.runtime,&desc,&sink),"register");
    std::vector<const char*> ids{desc.id};
    std::vector<ArCapability> capabilities(desc.supplies,desc.supplies+desc.supply_count);
#ifdef AR_CHECK_VRM
    if (vrmAdapter) {
        const auto vd = vrmAdapter->Descriptor();
        ok(host.api.register_evaluator(host.runtime,&vd,&sink),"register VRM");
        ids.insert(ids.begin(),vd.id); // Dependency ordering must work independently of selection order.
        capabilities.insert(capabilities.end(),vd.supplies,vd.supplies+vd.supply_count);
    }
#endif
    ArInstanceDesc d{AR_HEADER(ArInstanceDesc)};
    d.generation = 1; d.layout_id = baseline.layout_id; d.layout_version = baseline.layout_version; d.initial_state = baseline;
    d.evaluators = ids.data(); d.evaluator_count = uint32_t(ids.size());
    d.bound_capabilities = capabilities.data(); d.bound_capability_count = uint32_t(capabilities.size());
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
    std::set<uint32_t> changed, changedMotion;
    size_t scalarInputs = 0, gazeInputs = 0, nativeScalars = 0, nativeGazes = 0, changedMorphFrames = 0;
    std::set<std::string> unmapped;
    auto verifySnapshot = [&](const ArStateView& view, double seconds, bool probes, bool stale, bool absent) {
        const auto sampled = motion::SampleClip(c.clip,seconds);
        require(bool(sampled),"owner sample unavailable");
        const auto pose = owner.Retarget(*sampled.pose);
        for (size_t i = 0; i < c.skeleton.GetJoints().size(); ++i) {
            const auto& rest = c.skeleton.GetJoints()[i];
            const auto rotation = pxr::GfQuatd(pose.rotations[i]).GetNormalized();
            const auto restRotation = pxr::GfQuatd(rest.restRotation).GetNormalized();
            const double dot = rotation.GetReal()*restRotation.GetReal() +
                pxr::GfDot(rotation.GetImaginary(),restRotation.GetImaginary());
            if ((pose.translations[i]-rest.restTranslation).GetLength() > 1e-5 || 1-std::abs(dot) > 1e-5)
                changedMotion.insert(uint32_t(i));
        }
#ifdef AR_CHECK_VRM
        if (vrm) {
            auto selected = absent ? motion::MotionPose{} : vrm->Select(*sampled.pose,probes);
            const auto expected = vrm->Resolve(binding,pose,selected,stale,seconds*c.clockScale+c.clockOffset);
            maxError = std::max(maxError,compare(view,binding,pose,expected.eyes));
            maxError = std::max(maxError,vrm->Compare(view,expected));
        } else
#endif
        maxError = std::max(maxError,compare(view,binding,pose));
    };
    auto evaluate = [&](double seconds, uint64_t generation, bool probes = true, bool absent = false, bool coverage = true) {
        ArInputFrame input{AR_HEADER(ArInputFrame)};
        input.frame_id = ++frame; input.generation = generation; input.input_revision = frame;
        input.evaluation_seconds = seconds*c.clockScale+c.clockOffset;
        const double sourceSeconds = (input.evaluation_seconds-c.clockOffset)/c.clockScale;
        const bool stale = seconds < c.clip.samples.front().timestamp || seconds > c.clip.samples.back().timestamp;
#ifdef AR_CHECK_VRM
        std::unique_ptr<avatarMotion::MotionInputFrame> assembled;
        if (vrm) {
            const auto sampled = motion::SampleClip(c.clip,sourceSeconds);
            require(bool(sampled),"input sample unavailable");
            auto selected = vrm->Select(*sampled.pose,probes);
            assembled = std::make_unique<avatarMotion::MotionInputFrame>(assembler->Assemble(absent ? nullptr : &selected,input,
                stale ? AR_OBSERVATION_STALE : AR_OBSERVATION_VALID));
            input = assembled->View();
            if (coverage) {
                scalarInputs += input.scalar_count; gazeInputs += input.gaze_count;
                if (!absent) { nativeScalars += sampled.pose->channels.entries.size(); nativeGazes += sampled.pose->lookAtTarget ? 1 : 0; }
            }
            unmapped.insert(assembled->UnmappedChannels().begin(),assembled->UnmappedChannels().end());
        }
#endif
        ArSnapshot snapshot = 0;
        ok(host.api.evaluate_frame(host.runtime,instance,&input,&sink,&snapshot),"evaluate");
        // Own immediately, including if a later verification throws.
        host.retained.push_back(snapshot);
        ArStateView view{AR_HEADER(ArStateView)}; ok(host.api.get_snapshot(snapshot,&view),"get snapshot");
        verifySnapshot(view,sourceSeconds,probes,stale,absent);
        require(view.frame_id == input.frame_id && view.generation == generation &&
                view.evaluation_seconds == input.evaluation_seconds && view.input_revision == frame &&
                view.layout_id == c.layoutId && view.layout_version == c.layoutVersion &&
                view.instance == instance && view.capability_count == capabilities.size(),
                "snapshot identity mismatch");
        for (const auto& capability : capabilities) {
            bool found = false;
            for (uint32_t i = 0; i < view.capability_count; ++i)
                if (std::string(view.capabilities[i].id) == capability.id && view.capabilities[i].version == capability.version) found = true;
            require(found,"snapshot capability mismatch");
        }
        bool morphChange = false;
        for (uint32_t i = 0; i < view.blend_shape_count; ++i)
            morphChange |= std::abs(view.blend_shapes[i].weight-baseline.blend_shapes[i].weight) > 1e-6;
        if (coverage) changedMorphFrames += morphChange ? 1 : 0;
        for (uint32_t i = 0; i < view.joint_count; ++i) {
            const auto& a = view.joints[i].local; const auto& b = baseline.joints[i].local;
            double difference = 0;
            for (int k = 0; k < 3; ++k) difference += std::abs(a.translation[k]-b.translation[k]);
            // Quaternion sign is immaterial when checking whether the pose changed.
            double dot = 0; for (int k = 0; k < 4; ++k) dot += a.rotation[k]*b.rotation[k];
            if (difference > 1e-5 || 1-std::abs(dot) > 1e-5) changed.insert(i);
        }
        if (host.retained.size() > 3) {
            ok(host.api.release_snapshot(host.retained[2]),"release intermediate snapshot");
            host.retained.erase(host.retained.begin()+2);
        }
    };
    for (double time : times) evaluate(time,1);
#ifdef AR_CHECK_VRM
    if (vrm) {
        // Explicit zero and then total absence must clear prior expression effects.
        const auto saved = vrm->probeWeights;
        for (const auto& expression : vrm->config.expressions.GetExpressions()) vrm->probeWeights[expression.name] = 0;
        evaluate(*times.rbegin(),1,true,false,false);
        vrm->probeWeights = saved;
        evaluate(*times.rbegin(),1,false,true,false);
    }
#endif
    ok(host.api.reset_instance(host.runtime,instance,2),"reset");
    evaluate(c.clip.samples.front().timestamp,2,false,true,false);
    require(!diagnostics.error,"error diagnostics reported");
    ok(host.api.destroy_runtime(host.runtime),"destroy runtime"); host.runtime = 0;
    ArStateView old{AR_HEADER(ArStateView)}; ok(host.api.get_snapshot(host.retained.front(),&old),"retained snapshot");
    require(old.frame_id == 1 && old.generation == 1,"retained snapshot changed after reset/destruction");
    verifySnapshot(old,(old.evaluation_seconds-c.clockOffset)/c.clockScale,true,true,false);
    ArStateView active{AR_HEADER(ArStateView)}; ok(host.api.get_snapshot(host.retained[1],&active),"retained active snapshot");
    require(active.frame_id == 2 && active.generation == 1,"active snapshot changed after reset/destruction");
    verifySnapshot(active,(active.evaluation_seconds-c.clockOffset)/c.clockScale,true,false,false);
    ArStateView reset{AR_HEADER(ArStateView)}; ok(host.api.get_snapshot(host.retained.back(),&reset),"retained reset snapshot");
    require(reset.frame_id == frame && reset.generation == 2,"reset snapshot identity changed");
    verifySnapshot(reset,(reset.evaluation_seconds-c.clockOffset)/c.clockScale,false,false,true);
    require(!changedMotion.empty(),"Motion produced no observable avatar pose change before LookAt");
    std::cout << std::setprecision(10) << file << ": samples=" << c.clip.samples.size()
        << " duration=" << c.clip.samples.back().timestamp-c.clip.samples.front().timestamp
        << " frames=" << frame << " joints=" << baseline.joint_count << " changed_joints=" << changed.size()
        << " max_error=" << maxError << " changed_motion_joints=" << changedMotion.size() << " reset/retention=passed\n";
#ifdef AR_CHECK_VRM
    if (vrm) {
        std::cout << "  vrm expressions=" << vrm->config.expressions.GetSize() << " morphs=" << baseline.blend_shape_count
            << " materials=" << baseline.material_count << " native_scalars=" << nativeScalars << " native_gazes=" << nativeGazes
            << " selected_scalars=" << scalarInputs << " selected_gazes=" << gazeInputs << " changed_morph_frames=" << changedMorphFrames
            << " probe_weights=" << vrm->probeWeights.size() << " probe_gaze=" << bool(vrm->probeGaze) << '\n';
        for (const auto& channel : unmapped) std::cout << "  unmapped_channel=" << channel << '\n';
        if (!nativeScalars || !nativeGazes) std::cout << "  native expression/gaze coverage is incomplete; probes are host test input\n";
    }
#endif
    for (const auto& entry : diagnostics.codes) {
        std::cout << "  " << entry.first << " count=" << entry.second;
        for (const auto& subject : diagnostics.subjects[entry.first]) std::cout << " subject=" << subject;
        std::cout << '\n';
    }
}
}
int avatarMotionCheckMain(int argc, char** argv) {
    try {
        int first = 1;
        bool withVrm = false;
        std::map<std::string,float> weights;
        std::optional<pxr::GfVec3f> gaze;
        auto number = [](const std::string& s) {
            size_t end = 0; const double value = std::stod(s,&end);
            require(end == s.size() && std::isfinite(value) && std::abs(value) <= std::numeric_limits<float>::max(),"Invalid finite float option");
            return float(value);
        };
        while (first < argc && std::string(argv[first]).rfind("--",0) == 0) {
            const std::string option = argv[first++];
            if (option == "--vrm") withVrm = true;
            else if (option == "--gaze-point") {
                require(first+3 <= argc && !gaze,"--gaze-point requires exactly one XYZ point");
                const auto x = number(argv[first++]); const auto y = number(argv[first++]); const auto z = number(argv[first++]);
                gaze = pxr::GfVec3f(x,y,z);
            } else if (option == "--weight") {
                require(first < argc,"--weight requires expression=value");
                const std::string value = argv[first++]; const auto equal = value.find('=');
                require(equal != std::string::npos && equal != 0,"--weight requires expression=value");
                require(weights.emplace(value.substr(0,equal),number(value.substr(equal+1))).second,"Duplicate probe expression");
            } else throw std::runtime_error("Unknown option: "+option);
        }
        require(withVrm || (!gaze && weights.empty()),"Probe options require --vrm");
#ifndef AR_CHECK_VRM
        require(!withVrm,"--vrm requires AVATAR_BUILD_VRM_ADAPTER");
#endif
        if (argc-first < 2) {
            std::cerr << "Usage: avatarMotionCheck [--vrm [--gaze-point X Y Z] [--weight expression=value ...]] <avatar> <motion> [motion ...]\n";
            return 2;
        }
        auto stage = pxr::UsdStage::Open(argv[first++]);
        require(stage && stage->GetDefaultPrim(),"Avatar has no default prim or failed to open");
        avatarVrmUsd::HumanoidBinding binding(stage,{stage->GetDefaultPrim().GetPath(),{},"check.avatar",1});
#ifdef AR_CHECK_VRM
        std::unique_ptr<avatarVrmUsd::ExpressionBinding> expressions;
        std::unique_ptr<avatarMotionCheck::VrmCheck> vrm;
        if (withVrm) {
            const avatarVrmUsd::HumanoidBindingConfig config{stage->GetDefaultPrim().GetPath(),{},"check.avatar",1};
            expressions = std::make_unique<avatarVrmUsd::ExpressionBinding>(stage,avatarVrmUsd::ExpressionBindingConfig{config,{}});
            avatarVrmUsd::LookAtBinding look(stage,{config,{}});
            for (const auto& warning : look.Warnings()) std::cerr << "vrmRig: " << warning << '\n';
            vrm = std::make_unique<avatarMotionCheck::VrmCheck>(*expressions,look,std::move(weights),gaze);
        }
#endif
        stage.Reset();
        for (int i = first; i < argc; ++i) check(binding.Skeleton(),argv[i]
#ifdef AR_CHECK_VRM
            ,vrm.get()
#endif
            );
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
#ifndef AR_CHECK_TEST_ENTRY
int main(int argc, char** argv) { return avatarMotionCheckMain(argc,argv); }
#endif
