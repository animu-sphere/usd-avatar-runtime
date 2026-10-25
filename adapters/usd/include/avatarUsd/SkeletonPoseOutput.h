#pragma once
#include <avatarUsd/SkeletonBinding.h>
#include <avatarRuntime/api.h>
#include <pxr/base/vt/types.h>

namespace avatarUsd {

// Display-only transport of completed joints, optional weights and primary UVs.
// Owns one anonymous session sublayer; never evaluates motion/physics or saves USD.
// Single synchronous writer. Stage structural edits require a new binding.
class SkeletonPoseOutput {
public:
    // Optional single mesh: copies runtime (mesh path, blend-shape token)
    // weights using the mesh's existing standard UsdSkel bindings.
    // Independent uvMesh selects complete unindexed per-vertex primary st.
    SkeletonPoseOutput(pxr::UsdStageRefPtr stage, SkeletonBinding binding,
                       pxr::SdfPath blendShapeMesh = {}, pxr::SdfPath uvMesh = {});
    ~SkeletonPoseOutput();
    SkeletonPoseOutput(const SkeletonPoseOutput&) = delete;
    SkeletonPoseOutput& operator=(const SkeletonPoseOutput&) = delete;

    // Copies joint values and, when configured, blend-shape weights and st.
    // Refuses changed layout/topology/placement, another instance, old frames,
    // or malformed transforms before changing the last displayable pose.
    void Apply(const ArStateView& state);
    void Apply(const ArRuntimeApi& api, ArSnapshot snapshot);
    // Detaches only this output's layer, restoring the previous composition.
    // Idempotent. Applying after close rejects. Retained snapshots are independent.
    void Close();
    const pxr::SdfLayerRefPtr& Layer() const noexcept { return layer_; }
    const pxr::SdfPath& AnimationPath() const noexcept { return animation_; }
private:
    pxr::UsdStageRefPtr stage_;
    SkeletonBinding binding_;
    pxr::SdfLayerRefPtr layer_;
    pxr::SdfPath animation_;
    pxr::SdfPath mesh_;
    pxr::VtTokenArray shapes_;
    pxr::SdfPathVector targets_;
    pxr::SdfPath uvMesh_;
    size_t uvCount_ = 0;
    pxr::VtIntArray uvFaces_, uvFaceCounts_;
    ArInstance instance_ = 0;
    uint64_t generation_ = 0, frame_ = 0;
    double units_ = 1;
};
} // namespace avatarUsd
