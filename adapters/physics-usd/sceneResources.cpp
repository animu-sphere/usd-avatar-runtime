#include <avatarPhysics/SceneResources.h>
#include <usd_physics/usd/joint_conversion.h>
#include <pxr/usd/sdf/path.h>
#include <set>
#include <stdexcept>

namespace avatarPhysics {
namespace core = usd_physics::core;
namespace usd = usd_physics::usd;

SceneResources::SceneResources(core::PhysicsWorld &world,
                               const usd::PhysicsScene &scene, BodyPolicy policy,
                               JointPolicy jointPolicy, JointAdmission jointAdmission)
    : world_(world), shapes_(scene.bodies.size()),
      constraints_(scene.joints.size()) {
  // Allocate bookkeeping and validate all declarations/policy before touching
  // the world. Synthetic handles are only used by the owner's pure converter.
  std::map<std::string, core::BodyHandle> provisional;
  std::vector<core::BodyDescriptor> prepared;
  prepared.reserve(scene.bodies.size());
  for (const auto &body : scene.bodies) {
    const pxr::SdfPath path(body.path);
    if (!path.IsAbsolutePath() || !path.IsPrimPath() ||
        !bodies_.emplace(body.path, core::BodyHandle{}).second)
      throw std::invalid_argument("SceneResources requires unique absolute body paths");
    provisional.emplace(body.path, core::BodyHandle(provisional.size() + 1));
    core::validateShapeDescriptor(body.shape);
    core::BodyDescriptor descriptor;
    descriptor.motionType = body.motionType;
    descriptor.initialTransform = body.transform;
    descriptor.mass = body.mass;
    if (policy)
      policy(body, descriptor);
    if (descriptor.shape)
      throw std::invalid_argument("SceneResources owns body shape assignment");
    descriptor.shape = core::ShapeHandle(1);
    core::validateBodyDescriptor(descriptor);
    prepared.push_back(descriptor);
  }
  auto endpoint = [](const auto &handles, const std::string &path) {
    if (path.empty())
      return core::BodyHandle{};
    const auto found = handles.find(path);
    if (found == handles.end())
      throw std::invalid_argument("SceneResources joint endpoint outside resource set: " + path);
    return found->second;
  };
  std::set<std::string> paths;
  std::vector<std::optional<core::SixDofConstraintDescriptor>> preparedJoints;
  preparedJoints.reserve(scene.joints.size());
  auto *sixDof = dynamic_cast<core::SixDofConstraints *>(&world_);
  for (const auto &joint : scene.joints) {
    preparedJoints.emplace_back();
    const pxr::SdfPath path(joint.path);
    if (!path.IsAbsolutePath() || !path.IsPrimPath() ||
        !paths.insert(joint.path).second)
      throw std::invalid_argument("SceneResources requires unique absolute joint paths");
    if (!joint.enabled)
      continue;
    auto descriptor = usd::toSixDofConstraint(joint, endpoint(provisional, joint.body0),
                                             endpoint(provisional, joint.body1));
    const auto first = descriptor->firstBody, second = descriptor->secondBody;
    if (jointPolicy)
      jointPolicy(joint, *descriptor);
    if (descriptor->firstBody != first || descriptor->secondBody != second)
      throw std::invalid_argument("SceneResources owns joint endpoint assignment");
    core::validateSixDofConstraintDescriptor(*descriptor);
    if (jointAdmission && !jointAdmission(joint))
      continue;
    if (!sixDof)
      throw std::invalid_argument("SceneResources requires SixDofConstraints for admitted joints");
    preparedJoints.back() = std::move(descriptor);
  }
  try {
    for (size_t i = 0; i < scene.bodies.size(); ++i) {
      shapes_[i] = world_.createShape(scene.bodies[i].shape);
      if (!shapes_[i])
        throw std::runtime_error("SceneResources backend returned an invalid shape");
      prepared[i].shape = shapes_[i];
      auto &handle = bodies_.at(scene.bodies[i].path);
      handle = world_.createBody(prepared[i]);
      if (!handle)
        throw std::runtime_error("SceneResources backend returned an invalid body");
    }
    for (size_t i = 0; i < scene.joints.size(); ++i) {
      const auto &joint = scene.joints[i];
      if (!preparedJoints[i])
        continue;
      auto descriptor = *preparedJoints[i];
      descriptor.firstBody = endpoint(bodies_, joint.body0);
      descriptor.secondBody = endpoint(bodies_, joint.body1);
      constraints_[i] = sixDof->createSixDofConstraint(descriptor);
      if (!constraints_[i])
        throw std::runtime_error("SceneResources backend returned an invalid constraint");
    }
  } catch (...) {
    (void)Close();
    throw;
  }
}

SceneResources::~SceneResources() { (void)Close(); }

bool SceneResources::Close() noexcept {
  for (auto it = constraints_.rbegin(); it != constraints_.rend(); ++it) {
    if (*it && !world_.destroyConstraint(*it))
      return false;
    *it = {};
  }
  for (auto it = bodies_.begin(); it != bodies_.end();) {
    if (it->second && !world_.destroyBody(it->second))
      return false;
    it = bodies_.erase(it);
  }
  for (auto it = shapes_.rbegin(); it != shapes_.rend(); ++it) {
    if (*it && !world_.destroyShape(*it))
      return false;
    *it = {};
  }
  return true;
}
} // namespace avatarPhysics
