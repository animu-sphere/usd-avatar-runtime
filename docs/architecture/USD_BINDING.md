---
status: binding
owner: usd-avatar-runtime
---

# Scoped USD skeleton binding

The same optional target also installs the separate
[`SkeletonPoseOutput`](USD_POSE_OUTPUT.md) consumer for live pose publication;
reading/binding below remains independent of publication and changes no stage.

The optional `avatarUsdBinding` target reads a composed `UsdSkelSkeleton` at
default time into owned runtime baseline and motion-owner configuration values.
It addresses the skeleton portion of RT-O4. It is not a full VRM binding
adapter. The separate [VRM USD Humanoid binding](VRM_USD_BINDING.md) supplies
schema-derived mappings and records scoped local real-avatar evidence.

## Owner reading

The binding calls installed `motionUsd::ReadMotionSkeleton` with the
explicit `Generic` role for a validated typed descriptor and separate rigid
placement in metres. Descriptor building and validation stay with the owner;
the runtime does not rebuild the returned skeleton from raw arrays.

Retain avatar-root identity, runtime layout ID/version, skeleton and joint-ID
mapping, baseline `ArStateView`, root placement and common-state layout binding
here. Joint IDs and parent slots come directly from the owner descriptor;
runtime transform representation and normalized-quaternion checks remain when
marshalling the baseline. `SkeletonBinding` keeps its public name and lifetime
behavior, and `Skeleton()` preserves the owner's descriptor unchanged.

`motionRetarget` is public through `motionUsd`.

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
find_package(AvatarRuntime 0.2.0 EXACT CONFIG REQUIRED COMPONENTS usd motion vrm)
target_link_libraries(host PRIVATE AvatarRuntime::avatarUsdBinding
    AvatarRuntime::avatarMotionAdapter AvatarRuntime::avatarVrmAdapter)
```

`COMPONENTS usd` alone does not resolve `motionSampling` or `vrmRig`.
A core-only package lookup resolves no owner dependency. This is a C++
configuration API above the unchanged revision-5 C ABI, not a cross-toolchain
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
`motionUsd` package identity/version. Generic rejection codes are
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

## Evidence

Measured scope is in the [evidence log](../reports/evidence-log.md#usd-skeleton-binding-summary), and current state is in the
[capability matrix](../reference/CAPABILITY_MATRIX.md).
