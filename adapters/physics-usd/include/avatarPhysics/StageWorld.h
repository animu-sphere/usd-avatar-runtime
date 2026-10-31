#pragma once

#include <avatarPhysics/SceneResources.h>
#include <memory>
#include <optional>

namespace avatarPhysics {

// A whole-stage resource scope. Detach runtime registrations before Close;
// the caller remains the exclusive driver and selects the backend factory.
class StageWorld {
public:
  using WorldFactory = std::function<std::unique_ptr<usd_physics::core::PhysicsWorld>(
      const std::optional<usd_physics::usd::SceneSettings> &)>;

  StageWorld(const pxr::UsdStageRefPtr &stage, WorldFactory factory,
             SceneResources::BodyPolicy policy = {},
             SceneResources::JointPolicy jointPolicy = {},
             SceneResources::JointAdmission jointAdmission = {});
  ~StageWorld();
  StageWorld(const StageWorld &) = delete;
  StageWorld &operator=(const StageWorld &) = delete;

  // Borrowed until successful Close or destruction. Null when closed.
  usd_physics::core::PhysicsWorld *World() const noexcept { return world_.get(); }
  const SceneResources *Resources() const noexcept { return resources_.get(); }
  const std::optional<usd_physics::usd::SceneSettings> &Settings() const noexcept {
    return settings_;
  }
  // Refusal retains the world and remaining resources for explicit retry.
  bool Close() noexcept;

private:
  std::optional<usd_physics::usd::SceneSettings> settings_;
  std::unique_ptr<usd_physics::core::PhysicsWorld> world_;
  std::unique_ptr<SceneResources> resources_;
};

} // namespace avatarPhysics
