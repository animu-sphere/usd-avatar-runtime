#pragma once
#include <algorithm>
#include <cmath>
#include <memory>
#include <pxr/base/gf/matrix4d.h>
#include <stdexcept>
#include <toon/fast/avatar_state.hpp>

// Explicit generated resources exercise installed renderer transport. Actual
// avatar resource discovery and GPU rendering belong to the renderer host.
class MmdToonCheck {
  static void Require(bool ok) {
    if (!ok)
      throw std::runtime_error("MMD Toon transport mismatch");
  }
  Toon::FrameSnapshot scene_, output_, firstOutput_;
  Toon::AvatarBindings bindings_;
  Toon::AvatarStateAdapter adapter_;
  Toon::RetainedAvatarSnapshot first_, current_;

public:
  explicit MmdToonCheck(const ArStateView &state) {
    Require(state.joint_count && state.blend_shape_count == 1 &&
            state.visibility_count == 1 && state.material_count == 0);
    Toon::RenderWorld world;
    const auto mesh = world.CreateMesh();
    world.SetMeshPoints(mesh, {{-.4f, -.4f, 0}, {.4f, -.4f, 0}, {0, .4f, 0}});
    world.SetMeshTopology(mesh, {0, 1, 2});
    Toon::ToonSkin skin;
    skin.constant = true;
    skin.influences_per_point = 1;
    skin.influences = {{0, 1}};
    world.SetMeshSkin(mesh, skin);
    world.SetMeshSkinPose(mesh,
                          {std::vector<Toon::Matrix4>(state.joint_count), {}});
    Toon::AvatarSkinBinding palette;
    palette.mesh = mesh;
    for (uint32_t i = state.joint_count; i; --i)
      palette.joints.push_back(i - 1);
    palette.inverse_bind.resize(state.joint_count);
    bindings_.skins.push_back(std::move(palette));
    Toon::ToonMorph morph;
    morph.offsets = {{{.01f, 0, 0}, 0, {}, 0}};
    morph.ranges = {{0, 1}, {0, 1}, {0, 1}};
    world.SetMeshMorph(mesh, morph);
    world.SetMeshMorphWeights(mesh, {0});
    bindings_.morphs.push_back({0, mesh, 0});
    bindings_.visibility.push_back({0, mesh});
    scene_ = world.Commit();
    scene_.meters_per_unit = .01f;
    std::string error;
    Require(adapter_.Bind(state, scene_, 1, bindings_, error));
  }
  void Compare(const ArStateView &state,
               const Toon::FrameSnapshot &output) const {
    Require(output.time_seconds == state.evaluation_seconds &&
            output.meshes.size() == 1);
    const auto &mesh = output.meshes.front();
    Require(mesh.joints && mesh.joints->size() == state.joint_count);
    std::vector<pxr::GfMatrix4d> world(state.joint_count);
    for (uint32_t i = 0; i < state.joint_count; ++i) {
      const auto &joint = state.joints[i];
      const auto &t = joint.local;
      pxr::GfMatrix4d s(1), r(1), p(1);
      s.SetScale(pxr::GfVec3d(t.scale[0], t.scale[1], t.scale[2]));
      r.SetRotate(
          pxr::GfQuatd(t.rotation[3], pxr::GfVec3d(t.rotation[0], t.rotation[1],
                                                   t.rotation[2])));
      p.SetTranslate(
          pxr::GfVec3d(t.translation[0], t.translation[1], t.translation[2]) /
          double(scene_.meters_per_unit));
      world[i] = s * r * p;
      if (joint.parent_index >= 0)
        world[i] *= world[joint.parent_index];
    }
    for (uint32_t i = 0; i < state.joint_count; ++i) {
      for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 4; ++col) {
          const double unit =
              row == 3 && col < 3 ? double(scene_.meters_per_unit) : 1.;
          const double expected =
              world[bindings_.skins[0].joints[i]][row][col] * unit;
          const double actual = (*mesh.joints)[i].m[row * 4 + col] * unit;
          Require(std::isfinite(actual) && std::isfinite(expected) &&
                  std::abs(actual - expected) <=
                      2e-6 * std::max(1., std::abs(expected)));
        }
    }
    Require(mesh.morph_weights && mesh.morph_weights->size() == 1 &&
            mesh.morph_weights->front() == float(state.blend_shapes[0].weight));
    Require(mesh.visible == bool(state.visibility[0].visible));
    Require(mesh.points == scene_.meshes[0].points &&
            mesh.indices == scene_.meshes[0].indices);
  }
  void Apply(const ArRuntimeApi &api, ArSnapshot snapshot) {
    std::string error;
    Require(current_.Reset(api, snapshot, error));
    const auto &state = *current_.View();
    Require(adapter_.Apply(current_, scene_, 1, output_, error));
    Compare(state, output_);
    const auto &id = adapter_.IdentityInfo();
    Require(id.instance == state.instance && id.frame == state.frame_id &&
            id.generation == state.generation &&
            id.input_revision == state.input_revision &&
            id.layout == state.layout_id &&
            id.layout_version == state.layout_version && id.binding_epoch == 1);
    auto draws = Toon::ExtractDrawList(output_);
    Require(draws.draws.size() == (state.visibility[0].visible ? 1u : 0u));
    if (!draws.draws.empty()) {
      Require(Toon::ApplyFastSnapshot(output_, draws, error));
      Require(draws.draws[0].joints == output_.meshes[0].joints &&
              draws.draws[0].morph_weights == output_.meshes[0].morph_weights);
    }
    const auto previous = output_;
    Require(adapter_.Apply(current_, scene_, 1, output_, error));
    Require(
        output_.meshes[0].joints == previous.meshes[0].joints &&
        output_.meshes[0].pose_revision == previous.meshes[0].pose_revision &&
        output_.meshes[0].morph_weights == previous.meshes[0].morph_weights &&
        output_.meshes[0].morph_weights_revision ==
            previous.meshes[0].morph_weights_revision);
    if (!first_.View()) {
      Require(first_.Reset(api, snapshot, error));
      firstOutput_ = output_;
      // Prove the oracle detects a corrupt palette rather than only
      // comparing counts or successfully calling the adapter.
      auto corrupt = output_;
      auto palette = std::make_shared<std::vector<Toon::Matrix4>>(
          *corrupt.meshes[0].joints);
      palette->front().m[0] += 1;
      corrupt.meshes[0].joints = palette;
      bool refused = false;
      try {
        Compare(state, corrupt);
      } catch (const std::runtime_error &) {
        refused = true;
      }
      Require(refused);
    }
  }
  void AfterProducerDestruction() {
    Require(first_.View() && current_.View());
    Compare(*first_.View(), firstOutput_);
    Compare(*current_.View(), output_);
    std::string error;
    Require(adapter_.Apply(current_, scene_, 1, output_, error));
    const auto previous = output_;
    Require(first_.View()->generation < current_.View()->generation);
    Require(!adapter_.Apply(first_, scene_, 1, output_, error) &&
            !error.empty());
    Require(output_.meshes[0].joints == previous.meshes[0].joints);
    Compare(*current_.View(), output_);
  }
};
