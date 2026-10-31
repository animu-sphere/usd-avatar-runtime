#include <avatarPhysics/StageWorld.h>
#include <stdexcept>
#include <utility>

namespace avatarPhysics {

StageWorld::StageWorld(const pxr::UsdStageRefPtr &stage, WorldFactory factory,
                       SceneResources::BodyPolicy policy,
                       SceneResources::JointPolicy jointPolicy,
                       SceneResources::JointAdmission jointAdmission) {
  if (!factory)
    throw std::invalid_argument("StageWorld requires a world factory");
  // Owner validation precedes backend selection and any world mutation.
  const auto scene = usd_physics::usd::readPhysicsScene(stage);
  settings_ = scene.settings;
  world_ = factory(settings_);
  if (!world_)
    throw std::runtime_error("StageWorld factory returned no world");
  resources_ = std::make_unique<SceneResources>(*world_, scene, std::move(policy),
                                               std::move(jointPolicy), std::move(jointAdmission));
}

StageWorld::~StageWorld() { (void)Close(); }

bool StageWorld::Close() noexcept {
  if (resources_ && !resources_->Close())
    return false;
  resources_.reset();
  world_.reset();
  return true;
}

} // namespace avatarPhysics
