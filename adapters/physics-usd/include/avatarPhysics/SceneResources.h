#pragma once

#include <usd_physics/core/world.h>
#include <usd_physics/usd/box_scene.h>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace avatarPhysics {

// Host-owned resources above the installed snapshot reader and neutral world.
// The world outlives this object. Use only at an exclusive synchronous boundary,
// after scheduler detach on unload. No stepping, world creation or USD edits.
class SceneResources {
public:
  using BodyPolicy = std::function<void(const usd_physics::usd::PrimitiveBody &,
                                       usd_physics::core::BodyDescriptor &)>;
  SceneResources(usd_physics::core::PhysicsWorld &world,
                 const usd_physics::usd::PhysicsScene &scene,
                 BodyPolicy policy = {});
  ~SceneResources();
  SceneResources(const SceneResources &) = delete;
  SceneResources &operator=(const SceneResources &) = delete;

  const std::map<std::string, usd_physics::core::BodyHandle> &Bodies() const noexcept {
    return bodies_;
  }
  // Constraints, bodies, then shapes. Idempotent. On backend refusal, stops
  // before dependencies are destroyed; explicit callers may retry Close.
  // Do not externally destroy these handles or attach external constraints.
  bool Close() noexcept;

private:
  usd_physics::core::PhysicsWorld &world_;
  std::map<std::string, usd_physics::core::BodyHandle> bodies_;
  std::vector<usd_physics::core::ShapeHandle> shapes_;
  std::vector<usd_physics::core::ConstraintHandle> constraints_;
};

} // namespace avatarPhysics
