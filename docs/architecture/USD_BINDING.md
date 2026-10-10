---
status: binding
owner: usd-avatar-runtime
---

# Scoped USD skeleton binding

The optional `avatarUsdBinding` target reads a composed `UsdSkelSkeleton` at
default time into owned runtime baseline and motion-owner configuration values.
It addresses the skeleton portion of RT-O4. It is not a full VRM binding
adapter. The separate [VRM USD Humanoid binding](VRM_USD_BINDING.md) supplies
schema-derived mappings and records scoped local real-avatar evidence.

## Scoped owner reading split

The adopted [boundary policy section 8](../design/BOUNDARY_POLICY.md#8-skeletonbinding-split)
splits this helper into motion-owner conversion and runtime binding. The
implemented binding calls installed `motionUsd::ReadMotionSkeleton` with the
explicit `Generic` role for a validated typed descriptor and separate rigid
placement in metres. Descriptor building and validation stay with the owner;
the runtime does not rebuild the returned skeleton from raw arrays.

Retain avatar-root identity, runtime layout ID/version, skeleton and joint-ID
mapping, baseline `ArStateView`, root placement and common-state layout binding
here. Joint IDs and parent slots come directly from the owner descriptor;
runtime transform representation and normalized-quaternion checks remain when
marshalling the baseline. `SkeletonBinding` keeps its public name and lifetime
behavior, and `Skeleton()` preserves the owner's descriptor unchanged.

The owner's resolved WS-O4 dependency makes `motionRetarget` public through
`motionUsd`. Typed skeleton/source-rest results are now consumed here;
[cleanup workstream B](../roadmap/current.md#boundary-cleanup-workstreams)
also uses direct owner motion-stage results in the parity host, retaining
`StageClip` only as a compatibility wrapper.

## Dependencies and build

Enable `AVATAR_BUILD_USD_BINDING` with installed `motionUsd` and `motionRetarget`
0.5.4 or compatible later, their `motionCore` dependency and OpenUSD.
The typed reader ships in the owner's 0.5.4 release. Configuration checks the
package version, installed header and linked symbols. The target links
`usdSkel`/`usdGeom`; `avatarRuntime` remains independent of USD and providers.
Motion evaluation and VRM evaluation are separately optional. No sibling
sources are compiled into this repository.

In an x64 developer shell with dependency DLLs on `PATH`:

```sh
cmake -S . -B build/usd-binding -G Ninja -DCMAKE_BUILD_TYPE=Release -DAVATAR_BUILD_USD_BINDING=ON -DAVATAR_BUILD_MOTION_ADAPTER=ON -DAVATAR_BUILD_VRM_ADAPTER=ON -DCMAKE_PREFIX_PATH="<motion-install>;<vrmRig-install>;<usd-install>"
cmake --build build/usd-binding
ctest --test-dir build/usd-binding --output-on-failure
```

Installed consumers request the component explicitly:

```cmake
find_package(AvatarRuntime 0.1.0 EXACT CONFIG REQUIRED COMPONENTS usd motion vrm)
target_link_libraries(host PRIVATE AvatarRuntime::avatarUsdBinding
    AvatarRuntime::avatarMotionAdapter AvatarRuntime::avatarVrmAdapter)
```

`COMPONENTS usd` alone does not resolve `motionSampling` or `vrmRig`.
A core-only package lookup resolves no owner dependency. This is a C++
configuration API above the unchanged revision-3 C ABI, not a cross-toolchain
C++ ABI guarantee.

## Binding and ownership

[`SkeletonBinding`](../../adapters/usd/include/avatarUsd/SkeletonBinding.h)
takes a stage, existing avatar-root path, descendant skeleton path, host-assigned
layout ID/version and explicit `(HumanJoint, joint-token)` bindings. Roles use
the motion owner's vocabulary. Format owners or hosts supply the mappings;
the adapter does not infer roles from names or read VRM schema attributes.
Partial/empty role maps are allowed. Required-bone policy belongs to the owner
evaluator and is supplied separately through its retarget options.

The owner reader supplies checked joint tokens, parent indices and metre rest
values in its descriptor, plus separate placement metadata. The adapter resolves
the explicit map with owner `RetargetMap::SetJointToken`. Rest decomposition,
parent derivation and descriptor validation stay with the owner. Joint tokens,
order and auxiliary/non-humanoid joints are
preserved. The absolute skeleton prim path is the runtime skeleton identity.

The scoped profile requires Y-up and the caller's assertion that the stage
uses the canonical right-handed +Z-forward motion basis. USD has no forward-axis
metadata from which to infer that assertion. Stage `metersPerUnit` must be
finite and positive; OpenUSD's fallback applies when it is unauthored.
Z-up/basis conversion is not implemented.

Missing/mismatched rest arrays, empty/duplicate/invalid joint paths, parent
ordering/topology disagreement, duplicate roles/targets, missing mapped joints,
non-finite or out-of-owner-float-range rest values, zero scale, reflection,
homogeneous-column deviations beyond `1e-12` and shear beyond `1e-6` are
rejected. Accepted homogeneous-column roundoff is canonicalized to `(0,0,0,1)`
in the owner's returned copy before decomposition; authored values stay unchanged.
Positive nonuniform rest scale
is preserved without half narrowing. The owner float decomposition is
normalized to the runtime quaternion tolerance when marshalling baseline transforms.

The skeleton's composed local-to-world transform includes placement inherited
through the avatar root and its ancestors. Placement must be rigid within
`1e-6`; scaled/reflected/sheared placement is unsupported. Its translation is
converted to metres and retained in double precision. `Baseline()` folds that
placement into every root once; descendants stay parent-local. Owner
`Skeleton()` rest values remain skeleton-local for retargeting.

All baseline strings/arrays and owner values are immutable and owned. Copies
share storage, and values remain usable after the source stage is edited or
destroyed. No stage handle is retained. This is a default-time binding snapshot,
not a live animation/notice subscription. Rebuild the binding and create a new
runtime instance after relevant stage edits. Change layout ID/version when
joint identities/order/parents or role bindings change; the host remains
responsible for honest layout identity assignment.

Runtime binding failures throw `invalid_argument` with `USD_BINDING_*` codes.
Owner reading refusals throw `MotionUsdReadError`, an `invalid_argument`
subclass retaining unmodified owner code/subject/detail and installed
`motionUsd` package identity/version. Generic rejection codes are now
`MOTION_USD_*`, rather than locally renamed `USD_BINDING_*` codes.
These are pre-instance failures, not frame diagnostics or provider callbacks.

## Motion and VRM composition

Populate `ClipPoseAdapterConfig.skeleton`, `map`, `jointIds`, `skeletonId`
from the binding getters and `rootPlacement` from `RootPlacement()`. Use the
baseline's layout ID/version for both configuration and `ArInstanceDesc`,
and `Baseline()` as the instance's initial state. Keep a binding copy alive
while borrowing its views; instance creation copies them.

The [motion adapter](MOTION_ADAPTER.md) applies rigid root placement after
owner retargeting, once per root. It does not fold placement into rest
correction or source root motion. Empty clips leave the already placed
baseline untouched. Each frame starts from baseline, so placement never
accumulates. The [VRM adapter](VRM_ADAPTER.md) then reads the same working pose
under its explicit dependency on the motion evaluator.

## Evidence and remaining scope

`adapters.usd_skeleton` checks nested translated/rotated placement, centimetre
units, multiple roots, auxiliary/nonuniform-scale rest, role/slot identities,
malformed binding rejection, owned copy/stage lifetime and retained runtime
snapshot lifetime. With motion/VRM enabled it checks root-motion placement,
empty-clip baseline, repeat/reset behavior and bone LookAt numeric parity with
the installed owner evaluator at `1e-6`. `adapters.usd_installed` repeats these
checks through a separately configured installed consumer.

The typed owner-reading integration compares the unchanged descriptor, joint-ID
and baseline parent mapping, and placement with direct installed owner calls.
It retains identical refusal code/subject/detail and package identity/version
after stage destruction. The full 16-test runtime configuration and isolated
USD/component installed-consumer checks pass; seven real clips retain the prior
avatar composition parity. See the
[capability matrix](../reference/CAPABILITY_MATRIX.md) for the checked scope.

These tests construct USD stages and supply explicit mappings; they also
check bounded affine roundoff without changing authored matrices.
Schema-derived Humanoid discovery and one local real-avatar skeleton result
are recorded by the separate [VRM USD binding](VRM_USD_BINDING.md).
Format identity, expression/material/deformation bindings,
connector intake and
renderer consumption remain open. No milestone or ABI freeze follows from
this scoped binding evidence; see the [roadmap](../roadmap/current.md).
The separate [VRM LookAt binding](VRM_LOOKAT_USD_BINDING.md) supplies gaze
configuration and head/eye/rest extraction with constructed and local-avatar
test-input evidence.
