---
status: binding
owner: usd-avatar-runtime
---

# Scoped VRM USD Humanoid binding

The optional `avatarVrmUsdBinding` target reads the installed VRM owner's
applied `VrmHumanoidAPI` from a composed stage and supplies explicit mappings
to the generic [USD skeleton binding](USD_BINDING.md). This implements a scoped
schema-to-motion binding under RT-O4 without adding format logic to runtime
core or changing the revision-5 C ABI.

## Dependencies and installation

Enable `AVATAR_BUILD_USD_BINDING` and `AVATAR_BUILD_VRM_USD_BINDING` with installed
`vrmSchema` 0.9.0 or compatible later, `motionUsd`/`motionRetarget` 0.5.4
or compatible later and OpenUSD.
Motion evaluation and `vrmRig` evaluation are independently optional. The
adapter links installed owner targets; no sibling source is compiled here.

In an x64 developer shell with dependency DLLs on `PATH` and the matching
installed schema resources registered through `PXR_PLUGINPATH_NAME`:

```sh
cmake -S . -B build/usd-binding -G Ninja -DCMAKE_BUILD_TYPE=Release -DAVATAR_BUILD_USD_BINDING=ON -DAVATAR_BUILD_VRM_USD_BINDING=ON -DCMAKE_PREFIX_PATH="<motion-install>;<vrmSchema-install>;<usd-install>"
cmake --build build/usd-binding
ctest --test-dir build/usd-binding --output-on-failure
```

Installed consumers request the component explicitly:

```cmake
find_package(AvatarRuntime 0.2.0 EXACT CONFIG REQUIRED COMPONENTS vrm_usd)
target_link_libraries(host PRIVATE AvatarRuntime::avatarVrmUsdBinding)
```

This resolves the generic `usd` binding transitively. Requesting `usd` alone
does not resolve `vrmSchema`; requesting the core alone resolves no provider.
Add `COMPONENTS motion` for clip evaluation and `vrm` for expression/LookAt
evaluation. Schema registration/resource discovery remains the host's job;
linking a schema library does not substitute for its installed resources.

## Discovery, validation and ownership

[`HumanoidBinding`](../../adapters/vrm-usd/include/avatarVrmUsd/HumanoidBinding.h)
takes an avatar-root path, optional Humanoid prim path and host-assigned layout
ID/version. An empty Humanoid path searches the root's default `UsdPrimRange`
for exactly one applied owner API. This visits active, defined, loaded,
non-abstract prims and does not enter instance proxies. Missing or multiple
candidates fail; explicit selection accepts an active, loaded descendant
with the applied API. Prim names and joint names carry no inferred roles.

The owner's `vrm:skeleton` relationship must forward to exactly one skeleton
prim within the avatar root. Standard schema attributes with authored values
must contain nonempty default-time tokens. Exact role names are resolved by
the motion owner's `FindHumanJoint`; exact joint tokens are passed to the
generic binder. Absent roles remain absent. Empty/blocked/wrong-type authored
values fail. Duplicate targets, missing joints, rest/topology/units and rigid
placement are validated by `SkeletonBinding` with its existing diagnostics.
Partial/empty role maps do not impose an evaluator's required-bone policy.

Custom roles outside the installed standard schema are retained in
`UnsupportedBones()` for host diagnostics and are not silently mapped. They
must still have readable nonempty token values. No legacy alias conversion,
format detection, expression extraction or LookAt configuration is performed.
The separate [LookAt binding](VRM_LOOKAT_USD_BINDING.md) adds gaze configuration
and head/eye/rest extraction when the VRM registration adapter is enabled.

`Skeleton()` supplies the owned baseline, joint IDs, placement and owner
skeleton/retarget map for [motion composition](USD_BINDING.md#motion-and-vrm-composition).
`HumanoidId()` preserves the discovered prim identity. Copies share immutable
owned storage, retain no stage handle and survive stage edits/destruction.
Rebuild after authored configuration changes and change the layout identity
when bindings/layout change. Keep a binding copy alive while borrowing views.
Failures throw `invalid_argument` containing `VRM_BINDING_*` or delegated
`USD_BINDING_*` codes and a subject; the host supplies asset/binding provenance.

## Evidence

Measured scope is in the [evidence log](../reports/evidence-log.md#vrm-usd-humanoid-binding-summary), and current state is in the
[capability matrix](../reference/CAPABILITY_MATRIX.md).
