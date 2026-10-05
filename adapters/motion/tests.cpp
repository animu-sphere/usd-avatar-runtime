#include "avatarMotion/ClipPoseAdapter.h"
#include "avatarMotion/InputAssembler.h"
#ifdef AR_TEST_VRM
#include "avatarVrm/ExpressionAdapter.h"
#include "vrmRig/RequiredBones.h"
#endif
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>

#define CHECK(x) do { if (!(x)) { std::cerr << "Line " << __LINE__ << ": " << #x << '\n'; std::exit(1); } } while (0)
namespace {
namespace motion = openstrata::motion;
ArRuntimeApi api{};
bool near(double a, double b) { return std::abs(a-b) < 1e-6; }
struct Log {
    std::vector<std::string> codes, origins;
    static void AR_CALL emit(void* user, const ArDiagnostic* d) {
        auto& log = *static_cast<Log*>(user); log.codes.emplace_back(d->code); log.origins.emplace_back(d->origin);
    }
    bool has(const char* code) const { return std::find(codes.begin(), codes.end(), code) != codes.end(); }
    ArDiagnosticSink sink() { return {this, emit}; }
};
avatarMotion::ClipPoseAdapterConfig config() {
    avatarMotion::ClipPoseAdapterConfig c;
    c.evaluatorId = "motion.body"; c.layoutId = "test.layout"; c.layoutVersion = 4; c.skeletonId = "rig";
    motion::SkeletonJoint root; root.token = "Pelvis"; root.restTranslation = pxr::GfVec3f(0,1,0);
    motion::SkeletonJoint head; head.token = "Pelvis/Skull"; head.parent = 0; head.restTranslation = pxr::GfVec3f(0,0.5f,0);
    motion::SkeletonJoint extra; extra.token = "Pelvis/Skull/Extra"; extra.parent = 1;
    extra.restTranslation = pxr::GfVec3f(0,0.2f,0); extra.restScale = pxr::GfVec3f(0.5f,1,2);
    extra.restRotation = pxr::GfQuatf(float(std::cos(0.2)),pxr::GfVec3f(0,0,float(std::sin(0.2))));
    c.skeleton = motion::SkeletonDescriptor({root, head, extra});
    c.jointIds = {"hips", "head", "extra"};
    CHECK(c.map.SetJointIndex(motion::HumanJoint::Hips, 0, 3));
    CHECK(c.map.SetJointIndex(motion::HumanJoint::Head, 1, 3));
    c.options.requiredBones = {motion::HumanJoint::Hips, motion::HumanJoint::Head};
    c.sourceRest.localTranslations[size_t(motion::HumanJoint::Hips)] = pxr::GfVec3f(0,1,0);
    for (int i = 0; i < 2; ++i) {
        motion::MotionPose p; p.timestamp = double(i); p.root.hasPosition = true;
        p.root.worldPosition = pxr::GfVec3f(float(2*i),1,0);
        p.validRotations.set(size_t(motion::HumanJoint::Head));
        p.localRotations[size_t(motion::HumanJoint::Head)] =
            pxr::GfQuatf(float(std::cos(i*0.3)), pxr::GfVec3f(0,float(std::sin(i*0.3)),0));
        c.clip.samples.push_back(p);
    }
    c.clockScale = 2; c.clockOffset = 10;
    return c;
}
ArInputFrame frame(uint64_t id, double time) {
    ArInputFrame f{AR_HEADER(ArInputFrame)}; f.frame_id = id; f.generation = 1;
    f.evaluation_seconds = time; f.input_revision = 8; return f;
}
ArStateView view(ArSnapshot s) {
    ArStateView v{AR_HEADER(ArStateView)}; CHECK(api.get_snapshot(s, &v) == AR_OK); return v;
}
struct Fixture {
    avatarMotion::ClipPoseAdapter adapter;
    ArRuntime runtime = 0;
    // Extra unrelated skeleton precedes the bound rig: slot mapping cannot
    // assume owner indices equal runtime indices.
    ArJoint joints[4]{{"other", "unrelated", -1, {{7,0,0},{0,0,0,1},{1,1,1}}},
                     {"rig", "hips", -1, {{0,1,0},{0,0,0,1},{1,1,1}}},
                     {"rig", "head", 1, {{0,0.5,0},{0,0,0,1},{1,1,1}}},
                     {"rig", "extra", 2, {{0,0.2,0},{0,0,0,1},{0.5,1,2}}}};
    explicit Fixture(avatarMotion::ClipPoseAdapterConfig c = config()) : adapter(std::move(c)) {
        CHECK(api.create_runtime(&runtime) == AR_OK);
        auto d = adapter.Descriptor(); CHECK(api.register_evaluator(runtime, &d, nullptr) == AR_OK);
    }
    ~Fixture() { if (runtime) CHECK(api.destroy_runtime(runtime) == AR_OK); }
    ArInstance make(const char* layout = "test.layout", uint64_t version = 4) {
        const char* selected[] = {"motion.body"};
        auto evaluator = adapter.Descriptor();
        ArInstanceDesc d{AR_HEADER(ArInstanceDesc)}; d.generation = 1; d.layout_id = layout; d.layout_version = version;
        d.evaluators = selected; d.evaluator_count = 1;
        d.bound_capabilities = evaluator.supplies; d.bound_capability_count = evaluator.supply_count;
        d.required_capabilities = evaluator.supplies; d.required_capability_count = evaluator.supply_count;
        d.initial_state = {AR_HEADER(ArStateView)}; d.initial_state.joints = joints; d.initial_state.joint_count = 4;
        ArInstance id = 0; CHECK(api.create_instance(runtime, &d, nullptr, &id) == AR_OK); return id;
    }
};
void parity(const ArStateView& output, const avatarMotion::ClipPoseAdapterConfig& c, double time) {
    auto sample = motion::SampleClip(c.clip, (time-c.clockOffset)/c.clockScale); CHECK(sample);
    auto expected = motion::PoseRetargeter(c.skeleton,c.map,c.sourceRest,c.options).Retarget(*sample.pose);
    CHECK(output.joints[0].local.translation[0] == 7);
    for (size_t i = 0; i < c.skeleton.GetSize(); ++i) {
        const auto& local = output.joints[i+1].local;
        for (int k = 0; k < 3; ++k) {
            CHECK(near(local.translation[k],expected.translations[i][k]));
            CHECK(near(local.rotation[k],expected.rotations[i].GetImaginary()[k]));
            CHECK(near(local.scale[k],c.skeleton.GetJoints()[i].restScale[k]));
        }
        CHECK(near(local.rotation[3],expected.rotations[i].GetReal()));
    }
}
void evaluation() {
    Fixture f; auto instance = f.make(); auto other = f.make();
    auto input = frame(1,11); ArSnapshot s = 0; Log log; auto sink = log.sink();
    CHECK(api.evaluate_frame(f.runtime,instance,&input,&sink,&s) == AR_OK);
    auto output = view(s); parity(output,config(),11);
    CHECK(near(output.joints[1].local.translation[0],1));
    CHECK(output.input_revision == 8 && output.layout_version == 4 && output.evaluation_seconds == 11);
    CHECK(output.capability_count == 1 && std::string(output.capabilities[0].id) == "avatar.motion.clipPose");
    auto held = frame(2,14); ArSnapshot s2 = 0;
    CHECK(api.evaluate_frame(f.runtime,instance,&held,&sink,&s2) == AR_OK);
    CHECK(log.has("MOTION_ADAPTER_HELD")); parity(view(s2),config(),14); CHECK(api.release_snapshot(s2) == AR_OK);
    CHECK(near(view(s).joints[1].local.translation[0],1));
    auto first = frame(1,10);
    CHECK(api.evaluate_frame(f.runtime,other,&first,nullptr,&s2) == AR_OK); parity(view(s2),config(),10);
    CHECK(api.release_snapshot(s2) == AR_OK);
    CHECK(api.reset_instance(f.runtime,instance,2) == AR_OK); first.generation = 2;
    CHECK(api.evaluate_frame(f.runtime,instance,&first,nullptr,&s2) == AR_OK);
    CHECK(view(s2).generation == 2); CHECK(api.release_snapshot(s2) == AR_OK);
    CHECK(api.destroy_runtime(f.runtime) == AR_OK); f.runtime = 0;
    CHECK(near(view(s).joints[1].local.translation[0],1)); CHECK(api.release_snapshot(s) == AR_OK);
}
void invalidBindings() {
    auto rejects = [](avatarMotion::ClipPoseAdapterConfig c) {
        bool threw = false; try { avatarMotion::ClipPoseAdapter adapter(std::move(c)); }
        catch (const std::invalid_argument&) { threw = true; } CHECK(threw);
    };
    auto c = config(); c.jointIds[1] = c.jointIds[0]; rejects(c);
    c = config(); c.clockScale = 0; rejects(c);
    c = config(); c.clip.samples[0].timestamp = -std::numeric_limits<double>::max();
    c.clip.samples[1].timestamp = std::numeric_limits<double>::max(); rejects(c);
    c = config(); std::swap(c.clip.samples[0],c.clip.samples[1]); rejects(c);
    c = config(); c.clip.samples[0].localRotations[size_t(motion::HumanJoint::Head)] = pxr::GfQuatf(0); rejects(c);
    c = config(); c.clip.samples[0].root.worldPosition[0] = std::numeric_limits<float>::infinity(); rejects(c);
    c = config(); c.sourceRest.parents[0] = 0; rejects(c);
    c = config(); CHECK(c.map.SetJointIndex(motion::HumanJoint::Neck,1,3)); rejects(c);
    c = config(); CHECK(c.map.SetJointIndex(motion::HumanJoint::Neck,9,10)); rejects(c);
    for (int test = 0; test < 3; ++test) {
        Fixture f; if (test == 1) f.joints[2].joint_id = "missing";
        if (test == 2) f.joints[3].parent_index = 1;
        auto instance = test == 0 ? f.make("wrong",4) : f.make(); auto input = frame(1,11);
        Log log; auto sink = log.sink(); ArSnapshot s = 0;
        CHECK(api.evaluate_frame(f.runtime,instance,&input,&sink,&s) == AR_PROVIDER_ERROR && s == 0);
        CHECK(log.has(test == 0 ? "MOTION_ADAPTER_LAYOUT" : test == 1 ? "MOTION_ADAPTER_JOINT" : "MOTION_ADAPTER_PARENT"));
        CHECK(log.origins.front() == "usd-motion-plugins.motionRetarget");
    }
    c = config(); c.clockScale = std::numeric_limits<double>::denorm_min(); Fixture f(c); auto id = f.make();
    auto input = frame(1,11); ArSnapshot s = 0; Log log; auto sink = log.sink();
    CHECK(api.evaluate_frame(f.runtime,id,&input,&sink,&s) == AR_PROVIDER_ERROR && s == 0);
    CHECK(log.has("MOTION_ADAPTER_TIME"));
    input.evaluation_seconds = 10; // Corrected retry of same frame ID.
    CHECK(api.evaluate_frame(f.runtime,id,&input,nullptr,&s) == AR_OK); CHECK(api.release_snapshot(s) == AR_OK);
}
void unavailableAndDiagnostics() {
    auto c = config(); c.clip.samples.clear(); Fixture empty(c); auto id = empty.make();
    auto input = frame(1,11); ArSnapshot s = 0; Log log; auto sink = log.sink();
    CHECK(api.evaluate_frame(empty.runtime,id,&input,&sink,&s) == AR_OK);
    CHECK(log.has("MOTION_ADAPTER_UNAVAILABLE") && view(s).joints[1].local.translation[0] == 0);
    CHECK(api.release_snapshot(s) == AR_OK);
    c = config(); c.options.requiredBones.push_back(motion::HumanJoint::Neck);
    c.clip.samples[0].validRotations.set(size_t(motion::HumanJoint::Neck));
    CHECK(c.clip.samples[0].channels.Set("happy",0.5f));
    c.clip.samples[0].lookAtTarget = pxr::GfVec3f(0);
    Fixture missing(c); id = missing.make(); input.evaluation_seconds = 10;
    CHECK(api.evaluate_frame(missing.runtime,id,&input,&sink,&s) == AR_OK);
    CHECK(log.has("MOTION_RETARGET_MISSING_REQUIRED_BONE") && log.has("MOTION_RETARGET_UNBOUND_DRIVEN_BONE"));
    CHECK(log.has("MOTION_ADAPTER_CHANNELS_UNSUPPORTED") && log.has("MOTION_ADAPTER_GAZE_UNSUPPORTED"));
    CHECK(api.release_snapshot(s) == AR_OK);
}
ArStatus AR_CALL failAfterPose(void* user, void*, const ArEvaluationContext*, const ArStateWriter*) {
    return *static_cast<bool*>(user) ? AR_INVALID_STATE : AR_OK;
}
void rollback() {
    bool fail = false; Fixture f;
    ArEvaluatorDesc d{AR_HEADER(ArEvaluatorDesc)}; d.id = "test.failure"; d.provider_id = "test"; d.provider_version = "1";
    d.phase = AR_PHASE_FINAL_POSE; d.reads = AR_DOMAIN_POSE; d.user_data = &fail; d.evaluate = failAfterPose;
    CHECK(api.register_evaluator(f.runtime,&d,nullptr) == AR_OK);
    const char* selected[]{"motion.body","test.failure"};
    ArInstanceDesc desc{AR_HEADER(ArInstanceDesc)}; desc.generation = 1; desc.layout_id = "test.layout"; desc.layout_version = 4;
    desc.evaluators = selected; desc.evaluator_count = 2;
    desc.initial_state = {AR_HEADER(ArStateView)}; desc.initial_state.joints = f.joints; desc.initial_state.joint_count = 4;
    ArInstance id = 0; CHECK(api.create_instance(f.runtime,&desc,nullptr,&id) == AR_OK);
    auto input = frame(1,10); ArSnapshot first = 0, next = 0;
    CHECK(api.evaluate_frame(f.runtime,id,&input,nullptr,&first) == AR_OK);
    fail = true; input = frame(2,11);
    CHECK(api.evaluate_frame(f.runtime,id,&input,nullptr,&next) == AR_PROVIDER_ERROR && next == 0);
    CHECK(view(first).frame_id == 1 && view(first).joints[1].local.translation[0] == 0);
    fail = false;
    CHECK(api.evaluate_frame(f.runtime,id,&input,nullptr,&next) == AR_OK); parity(view(next),config(),11);
    CHECK(api.release_snapshot(first) == AR_OK && api.release_snapshot(next) == AR_OK);
}
avatarMotion::InputAssemblerConfig inputConfig() {
    avatarMotion::InputAssemblerConfig c;
    c.source = "test"; c.actor = "actor"; c.clockScale = 2; c.clockOffset = 10;
    c.channels = {{"happy", "face:happy"}, {"lookRight", "face:lookRight"}, {"absent", "face:absent"}};
    c.gazeChannel = "gaze:point";
    return c;
}
void inputAssembly() {
    // Return an owned frame after all source/config/assembler storage dies.
    const auto owned = [] {
        auto c = inputConfig(); avatarMotion::InputAssembler assembler(c);
        motion::MotionPose pose; pose.timestamp = 0.25;
        pose.channels.Set("happy", 0); pose.channels.Set("lookRight", 1.5f);
        pose.channels.Set("nativeUnmapped", -0.25f); pose.lookAtTarget = pxr::GfVec3f(0);
        auto context = frame(1,11); context.has_usd_mapping = 1;
        context.usd_time_codes_per_second = 30; context.usd_time_code_offset = 7;
        return assembler.Assemble(&pose,context);
    }();
    auto copy = owned; const auto& input = copy.View();
    CHECK(input.scalar_count == 2 && input.gaze_count == 1);
    CHECK(input.scalars[0].value == 0 && input.scalars[1].value == 1.5);
    CHECK(std::string(input.scalars[0].source_id) == "test" && std::string(input.scalars[0].actor_id) == "actor");
    CHECK(std::string(input.scalars[0].channel_id) == "face:happy");
    CHECK(input.scalars[0].source_seconds == 0.25 && input.scalars[0].clock_scale == 2 && input.scalars[0].clock_offset == 10);
    CHECK(input.evaluation_seconds == 11 && input.input_revision == 8 && input.frame_id == 1 && input.generation == 1);
    CHECK(input.has_usd_mapping == 1 && input.usd_time_codes_per_second == 30 && input.usd_time_code_offset == 7);
    const auto& gaze = input.gazes[0];
    CHECK(gaze.kind == AR_GAZE_POINT && gaze.space == AR_GAZE_RUNTIME_WORLD && gaze.validity == AR_OBSERVATION_VALID);
    CHECK(!gaze.skeleton_id && !gaze.joint_id && gaze.value[0] == 0 && gaze.value[1] == 0 && gaze.value[2] == 0);
    CHECK(gaze.source_seconds == 0.25 && gaze.clock_scale == 2 && gaze.clock_offset == 10);
    CHECK(copy.UnmappedChannels() == std::vector<std::string>{"nativeUnmapped"} && !copy.HasUnmappedGaze());
    Fixture fixture; const auto id = fixture.make(); ArSnapshot snapshot = 0;
    CHECK(api.evaluate_frame(fixture.runtime,id,&input,nullptr,&snapshot) == AR_OK);
    CHECK(view(snapshot).input_revision == 8); CHECK(api.release_snapshot(snapshot) == AR_OK);

    avatarMotion::InputAssembler assembler(inputConfig());
    auto context = frame(2,12);
    auto missing = assembler.Assemble(nullptr,context);
    CHECK(!missing.View().scalar_count && !missing.View().scalars && !missing.View().gaze_count && !missing.View().gazes);
    motion::MotionPose pose; pose.timestamp = 1;
    auto absent = assembler.Assemble(&pose,context);
    CHECK(!absent.View().scalar_count && !absent.View().gaze_count);
    pose.lookAtTarget = pxr::GfVec3f(1,2,3);
    auto stale = assembler.Assemble(&pose,context,AR_OBSERVATION_STALE);
    CHECK(stale.View().gazes[0].validity == AR_OBSERVATION_STALE && stale.View().gazes[0].value[2] == 3);
    auto noGaze = inputConfig(); noGaze.gazeChannel.clear();
    auto unmapped = avatarMotion::InputAssembler(noGaze).Assemble(&pose,context);
    CHECK(unmapped.HasUnmappedGaze() && !unmapped.View().gaze_count);
    // A later assembly does not mutate an earlier borrowed view.
    CHECK(owned.View().scalars[1].value == 1.5 && owned.View().gazes[0].value[2] == 0);

    auto rejectsConfig = [](avatarMotion::InputAssemblerConfig c) {
        bool threw = false; try { avatarMotion::InputAssembler rejected(c); }
        catch (const std::invalid_argument&) { threw = true; } CHECK(threw);
    };
    auto c = inputConfig(); c.channels.push_back(c.channels.front()); rejectsConfig(c);
    c = inputConfig(); c.channels[1].channel = c.channels[0].channel; rejectsConfig(c);
    c = inputConfig(); c.gazeChannel = c.channels[0].channel; rejectsConfig(c);
    c = inputConfig(); c.channels[0].channel = "missingNamespace"; rejectsConfig(c);
    c = inputConfig(); c.gazeChannel = "gaze:"; rejectsConfig(c);
    c = inputConfig(); c.source.clear(); rejectsConfig(c);
    c = inputConfig(); c.actor = std::string("actor\0hidden",12); rejectsConfig(c);
    c = inputConfig(); c.clockScale = 0; rejectsConfig(c);
    c = inputConfig(); c.clockOffset = std::numeric_limits<double>::infinity(); rejectsConfig(c);
    auto rejectsSample = [&](motion::MotionPose p, ArInputFrame f, uint32_t validity = AR_OBSERVATION_VALID) {
        bool threw = false; try { auto rejected = assembler.Assemble(&p,f,validity); }
        catch (const std::invalid_argument&) { threw = true; } CHECK(threw);
    };
    auto invalid = pose; invalid.timestamp = std::numeric_limits<double>::max(); rejectsSample(invalid,context);
    invalid = pose; invalid.channels.entries = {{"z",1},{"a",1}}; rejectsSample(invalid,context);
    invalid = pose; invalid.channels.entries = {{"a",1},{"a",1}}; rejectsSample(invalid,context);
    invalid = pose; invalid.channels.Set("ignored",std::numeric_limits<float>::quiet_NaN()); rejectsSample(invalid,context);
    invalid = pose; (*invalid.lookAtTarget)[0] = std::numeric_limits<float>::infinity(); rejectsSample(invalid,context);
    rejectsSample(pose,context,AR_OBSERVATION_UNAVAILABLE);
    auto invalidFrame = context; invalidFrame.scalars = input.scalars; invalidFrame.scalar_count = input.scalar_count;
    rejectsSample(pose,invalidFrame);
    invalidFrame = context; invalidFrame.abi_version = 2; rejectsSample(pose,invalidFrame);
    invalidFrame = context; invalidFrame.struct_size = 0; rejectsSample(pose,invalidFrame);
    invalidFrame = context; invalidFrame.frame_id = 0; rejectsSample(pose,invalidFrame);
    invalidFrame = context; invalidFrame.evaluation_seconds = std::numeric_limits<double>::infinity(); rejectsSample(pose,invalidFrame);
    invalidFrame = context; invalidFrame.has_usd_mapping = 1; rejectsSample(pose,invalidFrame);
    // Invalid assembly has no source cursor/state; corrected input can retry.
    CHECK(assembler.Assemble(&pose,context).View().gaze_count == 1);
}
#ifdef AR_TEST_VRM
void vrmSequence() {
    auto c = config(); c.options.requiredBones = vrmRig::GetRequiredBones();
    for (size_t i = 0; i < c.clip.samples.size(); ++i) {
        c.clip.samples[i].channels.Set("happy", float(i));
        c.clip.samples[i].channels.Set("lookRight", 0.9f);
        c.clip.samples[i].lookAtTarget = pxr::GfVec3f(2,1.7f,float(i*2));
    }
    Fixture f(c);
    avatarVrm::ExpressionAdapterConfig face;
    face.evaluatorId = "vrm.face"; face.layoutId = c.layoutId; face.layoutVersion = c.layoutVersion;
    face.after = {c.evaluatorId}; face.headSkeleton = c.skeletonId; face.headJoint = "head";
    vrmRig::LookAtRig gaze; gaze.type = vrmRig::LookAtType::Expression;
    gaze.horizontalInner.outputScale = gaze.horizontalOuter.outputScale = 1;
    gaze.verticalUp.outputScale = gaze.verticalDown.outputScale = 1;
    face.lookAt = gaze; face.gaze = {"test", "actor", "gaze:point"};
    face.inputs = {{{"test","actor","face:happy"},"happy"}, {{"test","actor","face:lookRight"},"lookRight"}};
    for (const char* name : {"lookLeft","lookRight","lookUp","lookDown","happy"}) {
        vrmRig::ExpressionDefinition e; e.name = name; e.morphTargets.push_back({name,1}); CHECK(face.expressions.Add(e));
        face.morphs.push_back({name,"mesh",name});
    }
    avatarVrm::ExpressionAdapter adapter(face); auto d = adapter.Descriptor();
    CHECK(api.register_evaluator(f.runtime,&d,nullptr) == AR_OK);
    ArBlendShape morphs[]{{"mesh","lookLeft",0},{"mesh","lookRight",0},{"mesh","lookUp",0},{"mesh","lookDown",0},{"mesh","happy",0.25}};
    const char* selected[]{"vrm.face","motion.body"}; // Deliberately reverse registration/selection order.
    std::vector<ArCapability> capabilities{f.adapter.Descriptor().supplies[0]};
    capabilities.insert(capabilities.end(),d.supplies,d.supplies+d.supply_count);
    ArInstanceDesc instance{AR_HEADER(ArInstanceDesc)}; instance.generation = 1;
    instance.layout_id = c.layoutId.c_str(); instance.layout_version = c.layoutVersion;
    instance.evaluators = selected; instance.evaluator_count = 2;
    instance.bound_capabilities = capabilities.data(); instance.bound_capability_count = uint32_t(capabilities.size());
    instance.initial_state = {AR_HEADER(ArStateView)}; instance.initial_state.joints = f.joints; instance.initial_state.joint_count = 4;
    instance.initial_state.blend_shapes = morphs; instance.initial_state.blend_shape_count = 5;
    ArInstance id = 0; CHECK(api.create_instance(f.runtime,&instance,nullptr,&id) == AR_OK);
    avatarMotion::InputAssembler assembler(inputConfig());
    const auto sample = motion::SampleClip(c.clip,0.5); CHECK(sample);
    const auto assembled = assembler.Assemble(&*sample.pose,frame(1,11));
    const auto& input = assembled.View();
    Log log; auto sink = log.sink(); ArSnapshot s = 0;
    CHECK(api.evaluate_frame(f.runtime,id,&input,&sink,&s) == AR_OK);
    auto output = view(s); parity(output,c,11);
    const auto pose = motion::PoseRetargeter(c.skeleton,c.map,c.sourceRest,c.options).Retarget(*sample.pose);
    vrmRig::LookAtInput ownerInput; ownerInput.timestamp = input.evaluation_seconds;
    CHECK(motion::GetJointWorldTransform(c.skeleton,pose,1,&ownerInput.head.orientation,&ownerInput.head.position));
    ownerInput.target = *sample.pose->lookAtTarget;
    const auto looked = vrmRig::LookAtEvaluator(gaze).Evaluate(ownerInput);
    auto weights = sample.pose->channels;
    for (const auto& entry : looked.expressions.entries) weights.Set(entry.name,entry.value);
    const auto expected = vrmRig::ExpressionResolver(face.expressions).Resolve(weights);
    CHECK(!expected.morphTargets.empty());
    for (const auto& effect : expected.morphTargets) {
        auto binding = std::find_if(face.morphs.begin(),face.morphs.end(),[&](const auto& b) { return b.ownerTarget == effect.target; });
        CHECK(binding != face.morphs.end());
        CHECK(near(output.blend_shapes[size_t(binding-face.morphs.begin())].weight,effect.weight));
    }
    CHECK(log.has("MOTION_RETARGET_MISSING_REQUIRED_BONE")); // Partial rig does not claim real VRM acceptance.
    CHECK(log.has("VRM_ADAPTER_GAZE_PRECEDENCE") && near(output.blend_shapes[4].weight,0.5));
    CHECK(api.release_snapshot(s) == AR_OK);
    // A stale held point is selected by host policy; scalar face input remains
    // usable and LookAt no longer replaces the explicitly mapped lookRight.
    const auto held = motion::SampleClip(c.clip,2); CHECK(held.status == motion::PoseSampleStatus::Held);
    const auto stale = assembler.Assemble(&*held.pose,frame(2,14),AR_OBSERVATION_STALE);
    // Preserve the owner's restamped sample time, not a guessed boundary key.
    CHECK(stale.View().scalars[0].source_seconds == held.pose->timestamp && held.pose->timestamp == 2);
    CHECK(api.evaluate_frame(f.runtime,id,&stale.View(),nullptr,&s) == AR_OK);
    CHECK(near(view(s).blend_shapes[4].weight,1) && near(view(s).blend_shapes[1].weight,0.9));
    CHECK(api.release_snapshot(s) == AR_OK);
    const auto absent = assembler.Assemble(nullptr,frame(3,15));
    CHECK(api.evaluate_frame(f.runtime,id,&absent.View(),nullptr,&s) == AR_OK);
    CHECK(near(view(s).blend_shapes[4].weight,0.25)); CHECK(api.release_snapshot(s) == AR_OK);
    CHECK(api.reset_instance(f.runtime,id,2) == AR_OK);
    const auto first = motion::SampleClip(c.clip,0); CHECK(first);
    auto restarted = frame(1,10); restarted.generation = 2;
    const auto zero = assembler.Assemble(&*first.pose,restarted);
    CHECK(api.evaluate_frame(f.runtime,id,&zero.View(),nullptr,&s) == AR_OK);
    CHECK(near(view(s).blend_shapes[4].weight,0)); CHECK(api.release_snapshot(s) == AR_OK);
    // Runtime must be destroyed before either borrowed adapter object.
    CHECK(api.destroy_runtime(f.runtime) == AR_OK); f.runtime = 0;
}
#endif
} // namespace
int main() {
    CHECK(arGetApi(AR_ABI_VERSION,sizeof(api),&api) == AR_OK);
    evaluation(); invalidBindings(); unavailableAndDiagnostics(); rollback(); inputAssembly();
#ifdef AR_TEST_VRM
    vrmSequence();
#endif
    std::cout << "Motion adapter contracts passed\n";
}
