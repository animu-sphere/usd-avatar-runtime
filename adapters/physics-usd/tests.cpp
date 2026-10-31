#include <avatarPhysics/SceneResources.h>
#include <avatarPhysics/StageWorld.h>
#include <usd_physics/core/six_dof.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdPhysics/scene.h>
#include <pxr/usd/usdPhysics/fixedJoint.h>
#include <pxr/usd/sdf/copyUtils.h>
#include <cstdlib>
#include <iostream>
#include <set>

#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " << #x << '\n'; std::exit(1); } } while (0)
namespace core = usd_physics::core;
namespace usd = usd_physics::usd;
using Resources = avatarPhysics::SceneResources;

struct BareWorld : core::PhysicsWorld {
  uint64_t next = 0;
  int creates = 0, failAt = 0;
  bool invalid = false, refuseConstraint = false, refuseBody = false, refuseShape = false;
  std::set<uint64_t> shapes;
  std::map<uint64_t, core::BodyDescriptor> bodies;
  std::map<uint64_t, core::SixDofConstraintDescriptor> constraints;
  std::vector<std::string> destroyed;
  uint64_t allocate() {
    if (++creates == failAt) {
      if (invalid) return 0;
      throw std::runtime_error("injected creation failure");
    }
    return ++next;
  }
  core::ShapeHandle createShape(const core::ShapeDescriptor &) override {
    auto id = allocate(); if (id) shapes.insert(id); return core::ShapeHandle(id);
  }
  bool destroyShape(core::ShapeHandle h) noexcept override {
    if (refuseShape) return false;
    for (const auto &[id, body] : bodies) CHECK(body.shape != h);
    CHECK(shapes.erase(h.value()) == 1); destroyed.push_back("shape"); return true;
  }
  core::BodyHandle createBody(const core::BodyDescriptor &d) override {
    CHECK(shapes.count(d.shape.value()));
    auto id = allocate(); if (id) bodies.emplace(id, d); return core::BodyHandle(id);
  }
  bool destroyBody(core::BodyHandle h) noexcept override {
    if (refuseBody) return false;
    for (const auto &[id, joint] : constraints)
      CHECK(joint.firstBody != h && joint.secondBody != h);
    CHECK(bodies.erase(h.value()) == 1); destroyed.push_back("body"); return true;
  }
  core::ConstraintHandle createConstraint(const core::ConstraintDescriptor &) override {
    std::abort();
  }
  bool destroyConstraint(core::ConstraintHandle h) noexcept override {
    if (refuseConstraint) return false;
    CHECK(constraints.erase(h.value()) == 1); destroyed.push_back("joint"); return true;
  }
  bool applyForce(core::BodyHandle, core::Vector3) override { std::abort(); }
  bool setLinearVelocity(core::BodyHandle, core::Vector3) override { std::abort(); }
  core::BodyState bodyState(core::BodyHandle) const override { std::abort(); }
  void step(Duration) override { std::abort(); }
  std::vector<core::BodyState> takeChangedBodyStates() override { std::abort(); }
};

struct World : BareWorld, core::SixDofConstraints {
  core::ConstraintHandle createSixDofConstraint(const core::SixDofConstraintDescriptor &d) override {
    CHECK(!d.firstBody || bodies.count(d.firstBody.value()));
    CHECK(!d.secondBody || bodies.count(d.secondBody.value()));
    auto id = allocate(); if (id) constraints.emplace(id, d); return core::ConstraintHandle(id);
  }
};

usd::PhysicsScene scene() {
  usd::PhysicsScene result;
  for (int i = 0; i < 3; ++i) {
    usd::PrimitiveBody body;
    body.path = "/Asset/body" + std::to_string(i);
    body.motionType = i ? core::MotionType::dynamicBody : core::MotionType::kinematicBody;
    body.transform.translation = {double(i), 2, 0};
    result.bodies.push_back(body);
  }
  usd::Joint fixed;
  fixed.path = "/Asset/fixed"; fixed.type = usd::JointType::fixed;
  fixed.body0 = result.bodies[0].path; fixed.body1 = result.bodies[1].path;
  fixed.localFrame0.translation = {1, 0, 0};
  result.joints.push_back(fixed);
  usd::Joint world;
  world.path = "/Asset/world"; world.body1 = result.bodies[2].path;
  world.limits[0] = {true, -.5, .5};
  result.joints.push_back(world);
  usd::Joint disabled = world;
  disabled.path = "/Asset/disabled"; disabled.enabled = false;
  result.joints.push_back(disabled);
  return result;
}

void lifetime() {
  World world;
  Resources survivor(world, scene());
  const auto survivorHandles = survivor.Bodies();
  const auto baselineBodies = world.bodies.size(), baselineShapes = world.shapes.size();
  {
    auto snapshot = scene();
    Resources removed(world, snapshot, [](const auto &, auto &d) {
      d.damping = {0, 0}; d.collisionFilter = {1, 0};
      d.initialTransform.rotation = {.5, 0, 0, .8660254037844386};
    });
    // Identical paths in another resource set resolve to different world handles.
    CHECK(removed.Bodies().at("/Asset/body0") != survivorHandles.at("/Asset/body0"));
    bool fixedMapped = false, worldMapped = false;
    for (const auto &[id, joint] : world.constraints) {
      if (joint.firstBody == removed.Bodies().at("/Asset/body0")) {
        CHECK(joint.secondBody == removed.Bodies().at("/Asset/body1"));
        CHECK(joint.firstFrame.translation.x == 1);
        fixedMapped = true;
      }
      if (joint.secondBody == removed.Bodies().at("/Asset/body2")) {
        CHECK(!joint.firstBody);
        worldMapped = true;
      }
    }
    CHECK(fixedMapped && worldMapped);
    for (const auto &[path, handle] : removed.Bodies()) {
      const auto &d = world.bodies.at(handle.value());
      CHECK(d.damping.linear == 0 && d.collisionFilter.collidesWith == 0);
      CHECK(d.initialTransform.rotation.w == .5);
    }
    snapshot = {}; // No borrowed scene data survives construction.
    CHECK(removed.Close() && removed.Bodies().empty());
    CHECK(removed.Close());
    CHECK((world.destroyed == std::vector<std::string>{
      "joint", "joint", "body", "body", "body", "shape", "shape", "shape"}));
  }
  CHECK(world.bodies.size() == baselineBodies && world.shapes.size() == baselineShapes);
  CHECK(survivor.Bodies() == survivorHandles && world.constraints.size() == 2);
  for (auto [path, handle] : survivorHandles) CHECK(world.bodies.count(handle.value()));
  CHECK(survivor.Close());
  CHECK(world.shapes.empty() && world.bodies.empty() && world.constraints.empty());
  { Resources automatic(world, scene()); }
  CHECK(world.shapes.empty() && world.bodies.empty() && world.constraints.empty());
}

void failures() {
  for (bool invalid : {false, true}) for (int failure = 1; failure <= 8; ++failure) {
    World world;
    Resources survivor(world, scene());
    const auto handles = survivor.Bodies();
    world.failAt = world.creates + failure; world.invalid = invalid;
    bool rejected = false;
    try { Resources resources(world, scene()); }
    catch (const std::runtime_error &) { rejected = true; }
    CHECK(rejected);
    CHECK(world.shapes.size() == 3 && world.bodies.size() == 3 && world.constraints.size() == 2);
    CHECK(survivor.Bodies() == handles);
  }
  for (int refusal = 0; refusal < 3; ++refusal) {
    World world;
    Resources resources(world, scene());
    world.refuseConstraint = refusal == 0;
    world.refuseBody = refusal == 1;
    world.refuseShape = refusal == 2;
    CHECK(!resources.Close());
    if (refusal == 0) CHECK(world.destroyed.empty());
    if (refusal == 1) CHECK(world.constraints.empty() && world.bodies.size() == 3 && world.shapes.size() == 3);
    if (refusal == 2) CHECK(world.constraints.empty() && world.bodies.empty() && world.shapes.size() == 3);
    world.refuseConstraint = world.refuseBody = world.refuseShape = false;
    CHECK(resources.Close() && resources.Bodies().empty());
    CHECK(world.shapes.empty() && world.bodies.empty() && world.constraints.empty());
  }
}

void validation() {
  auto rejects = [](auto mutate) {
    World world; auto snapshot = scene(); mutate(snapshot);
    bool rejected = false;
    try { Resources resources(world, snapshot); }
    catch (const std::exception &) { rejected = true; }
    CHECK(rejected && world.creates == 0);
  };
  rejects([](auto &s) { s.bodies.push_back(s.bodies.front()); });
  rejects([](auto &s) { s.bodies.front().path = "relative"; });
  rejects([](auto &s) { s.joints.push_back(s.joints.front()); });
  rejects([](auto &s) { s.joints.front().body1 = "/Other/body"; });
  rejects([](auto &s) { s.joints[1].limits[0] = {true, 1, 1}; });
  rejects([](auto &s) { s.bodies.front().shape.halfExtents.x = -1; });
  World world;
  for (bool shape : {false, true}) {
    bool rejected = false;
    try {
      Resources resources(world, scene(), [shape](const auto &, auto &d) {
        if (shape) d.shape = core::ShapeHandle(99);
        else throw std::runtime_error("policy refusal");
      });
    } catch (const std::exception &) { rejected = true; }
    CHECK(rejected && world.creates == 0);
  }
  BareWorld plain;
  bool rejected = false;
  try { Resources resources(plain, scene()); }
  catch (const std::invalid_argument &) { rejected = true; }
  CHECK(rejected && plain.creates == 0);
  auto snapshot = scene();
  for (auto &joint : snapshot.joints) joint.enabled = false;
  { Resources resources(plain, snapshot); }
  CHECK(plain.bodies.empty() && plain.shapes.empty());
  { Resources empty(plain, {}); CHECK(empty.Bodies().empty() && empty.Close()); }
}

void jointPolicy() {
  World world;
  int calls = 0;
  Resources resources(world, scene(), {}, [&](const auto &joint, auto &d) {
    ++calls;
    CHECK(world.creates == 0 && joint.enabled);
    if (joint.path == "/Asset/world") {
      d.translationSprings[0] = {7, 0, .1};
      d.rotationSprings[2] = {3, 0};
    }
  });
  CHECK(calls == 2); // Disabled joints skip policy; enabled callbacks run once.
  bool found = false;
  for (const auto &[id, d] : world.constraints)
    if (d.secondBody == resources.Bodies().at("/Asset/body2")) {
      CHECK(!d.firstBody && d.translationSprings[0].stiffness == 7);
      CHECK(d.translationSprings[0].equilibrium == .1 && d.rotationSprings[2].stiffness == 3);
      found = true;
    }
  CHECK(found);
  for (int failure = 0; failure < 4; ++failure) {
    World refused;
    bool rejected = false;
    try {
      Resources invalid(refused, scene(), {}, [failure](const auto &, auto &d) {
        if (failure == 0) throw std::runtime_error("joint policy refusal");
        if (failure == 1) d.firstBody = core::BodyHandle(99);
        if (failure == 2) d.translationSprings[0].stiffness = -1;
        if (failure == 3) d.translationSprings[0].stiffness = 1; // fixed locked axis
      });
    } catch (const std::exception &) { rejected = true; }
    CHECK(rejected && refused.creates == 0);
  }
}

pxr::UsdStageRefPtr stage(bool settings = true) {
  auto result = pxr::UsdStage::CreateInMemory();
  CHECK(result->GetRootLayer()->ImportFromString(R"(#usda 1.0
(
    upAxis = "Y"
    metersPerUnit = 1
)
def Cube "Body" (
    prepend apiSchemas = ["PhysicsRigidBodyAPI", "PhysicsCollisionAPI", "PhysicsMassAPI"]
)
{
    float physics:mass = 2
}
def PhysicsFixedJoint "Anchor"
{
    rel physics:body1 = </Body>
}
)"));
  if (settings) {
    auto scene = pxr::UsdPhysicsScene::Define(result, pxr::SdfPath("/Physics"));
    scene.CreateGravityDirectionAttr().Set(pxr::GfVec3f(0, -1, 0));
    scene.CreateGravityMagnitudeAttr().Set(3.0f);
  }
  return result;
}

struct OwnedWorld : World {
  int &destroyedWorlds;
  explicit OwnedWorld(int &count) : destroyedWorlds(count) {}
  ~OwnedWorld() override {
    CHECK(constraints.empty() && bodies.empty() && shapes.empty());
    ++destroyedWorlds;
  }
};

void stageWorld() {
  using StageWorld = avatarPhysics::StageWorld;
  int created = 0, destroyed = 0;
  auto factory = [&](const auto &settings) {
    ++created;
    CHECK(settings && settings->path == "/Physics");
    CHECK(settings->gravity.x == 0 && settings->gravity.y == -3 && settings->gravity.z == 0);
    return std::make_unique<OwnedWorld>(destroyed);
  };
  {
    auto input = stage();
    int jointCalls = 0;
    StageWorld owned(input, factory, [](const auto &, auto &body) { body.mass = 7; },
        [&](const auto &, auto &joint) { ++jointCalls; joint.collisionEnabled = true; });
    auto *world = dynamic_cast<OwnedWorld *>(owned.World());
    CHECK(world && world->bodies.size() == 1 && world->constraints.size() == 1);
    CHECK(world->bodies.begin()->second.mass == 7 && created == 1);
    CHECK(jointCalls == 1 && world->constraints.begin()->second.collisionEnabled);
    CHECK(owned.Resources()->Bodies().size() == 1);
    // The binding is a snapshot, with no stage retention or edit subscription.
    CHECK(input->RemovePrim(pxr::SdfPath("/Body")));
    input.Reset();
    CHECK(owned.Resources()->Bodies().count("/Body") && owned.Settings()->gravity.y == -3);
    world->refuseConstraint = true;
    CHECK(!owned.Close() && owned.World() == world && owned.Resources() && destroyed == 0);
    world->refuseConstraint = false;
    CHECK(owned.Close() && !owned.World() && !owned.Resources() && destroyed == 1);
    CHECK(owned.Close() && destroyed == 1);
    CHECK(owned.Settings()->path == "/Physics");
  }
  {
    // An absent declaration is passed as absent; backend defaults are caller policy.
    StageWorld defaults(stage(false), [&](const auto &settings) {
      CHECK(!settings); return std::make_unique<OwnedWorld>(destroyed);
    });
    CHECK(!defaults.Settings());
  }
  CHECK(destroyed == 2);
  {
    // Cross-root joints belong to the whole-stage lifetime, not separate sets.
    auto input = stage();
    CHECK(pxr::SdfCopySpec(input->GetRootLayer(), pxr::SdfPath("/Body"),
                           input->GetRootLayer(), pxr::SdfPath("/OtherBody")));
    auto joint = pxr::UsdPhysicsFixedJoint::Define(input, pxr::SdfPath("/CrossRoot"));
    CHECK(joint.CreateBody0Rel().SetTargets({pxr::SdfPath("/Body")}));
    CHECK(joint.CreateBody1Rel().SetTargets({pxr::SdfPath("/OtherBody")}));
    StageWorld owned(input, factory);
    auto *world = dynamic_cast<OwnedWorld *>(owned.World());
    CHECK(world && world->bodies.size() == 2 && world->constraints.size() == 2);
    const auto &handles = owned.Resources()->Bodies();
    bool mapped = false;
    for (const auto &[id, constraint] : world->constraints)
      if (constraint.firstBody == handles.at("/Body")) {
        CHECK(constraint.secondBody == handles.at("/OtherBody"));
        mapped = true;
      }
    CHECK(mapped && owned.Close());
  }
  CHECK(destroyed == 3);
  for (int failure = 1; failure <= 3; ++failure) {
    for (bool invalid : {false, true}) {
      bool rejected = false;
      try {
        StageWorld failed(stage(), [&](const auto &) {
          auto world = std::make_unique<OwnedWorld>(destroyed);
          world->failAt = failure; world->invalid = invalid;
          return world;
        });
      } catch (const std::runtime_error &) { rejected = true; }
      CHECK(rejected);
    }
  }
  CHECK(destroyed == 9);
  bool rejected = false;
  try {
    StageWorld failed(stage(), factory, [](const auto &, auto &) {
      throw std::runtime_error("policy refusal");
    });
  } catch (const std::runtime_error &) { rejected = true; }
  CHECK(rejected && destroyed == 10);
  for (int invalid = 0; invalid < 3; ++invalid) {
    auto input = stage();
    if (invalid == 0) input.Reset();
    if (invalid == 1) pxr::UsdPhysicsScene::Define(input, pxr::SdfPath("/OtherPhysics"));
    if (invalid == 2) pxr::UsdGeomSetStageMetersPerUnit(input, .01);
    const auto before = created;
    rejected = false;
    try { StageWorld failed(input, factory); }
    catch (const usd::ImportError &error) {
      rejected = true;
      if (invalid == 2) CHECK(error.code() == usd::ImportErrorCode::unsupportedUnits);
    }
    CHECK(rejected && created == before);
  }
  rejected = false;
  try { StageWorld failed(stage(), {}); }
  catch (const std::invalid_argument &) { rejected = true; }
  CHECK(rejected);
  rejected = false;
  try {
    StageWorld failed(stage(), [](const auto &) -> std::unique_ptr<core::PhysicsWorld> { return {}; });
  } catch (const std::runtime_error &) { rejected = true; }
  CHECK(rejected);
  rejected = false;
  try {
    StageWorld failed(stage(), [](const auto &) -> std::unique_ptr<core::PhysicsWorld> {
      throw std::runtime_error("factory refusal");
    });
  } catch (const std::runtime_error &) { rejected = true; }
  CHECK(rejected);
}

void jointAdmission() {
  World world;
  int calls = 0;
  {
    Resources resources(world, scene(), {}, {}, [&](const auto &joint) {
      CHECK(world.creates == 0 && joint.enabled); ++calls;
      return joint.path == "/Asset/world";
    });
    CHECK(calls == 2 && world.constraints.size() == 1 && world.bodies.size() == 3);
    CHECK(!world.constraints.begin()->second.firstBody);
  }
  CHECK(world.constraints.empty() && world.bodies.empty() && world.shapes.empty());
  BareWorld bare;
  { Resources resources(bare, scene(), {}, {}, [](const auto &) { return false; }); }
  CHECK(bare.bodies.empty());
  // Omitted joints still undergo conversion and policy validation.
  for (int failure = 0; failure < 3; ++failure) {
    auto input = scene();
    if (failure == 0) input.joints[1].limits[0] = {true, 1, 1};
    bool rejected = false;
    const int before = world.creates;
    try {
      Resources resources(world, input, {}, [&](const auto &, auto &d) {
        if (failure == 1) d.firstBody = core::BodyHandle(99);
      }, [&](const auto &) {
        if (failure == 2) throw std::runtime_error("admission failure");
        return false;
      });
    } catch (const std::exception &) { rejected = true; }
    CHECK(rejected && world.creates == before);
  }
}

int main() {
  lifetime(); failures(); validation(); jointPolicy(); stageWorld(); jointAdmission();
  std::cout << "Scene resource construction, rollback and selective lifetime passed\n";
}
