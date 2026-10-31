---
status: binding
owner: usd-avatar-runtime
---

# Scoped VRM USD Expression binding

`avatarVrmExpressionUsdBinding` extracts applied owner `VrmExpressionAPI`
definitions and their canonical output layout into the existing
[VRM registration adapter](VRM_ADAPTER.md). This advances RT-O4 without
changing the revision-5 C ABI. `vrmRig` still owns expression arbitration,
binary rounding, morph accumulation and the material colour slot vocabulary.

## Build and package boundary

The target requires `AVATAR_BUILD_USD_BINDING`, `AVATAR_BUILD_VRM_USD_BINDING`
and `AVATAR_BUILD_VRM_ADAPTER`. It consumes installed `vrmSchema`, `vrmRig`,
`motionRetarget` and OpenUSD, including `usdShade`. Core remains independent.
Hosts request `AvatarRuntime` component `vrm_expression_usd` and link
`AvatarRuntime::avatarVrmExpressionUsdBinding`. That component resolves `vrm`,
`vrm_usd` and `usd`, without importing LookAt binding, the source-clip adapter or
sampling. The generic USD binding resolves `motionUsd` for owner skeleton reading.
Schema resources and DLL discovery remain host responsibilities.

## Discovery and owned values

[`ExpressionBinding`](../../adapters/vrm-usd/include/avatarVrmUsd/ExpressionBinding.h)
takes Humanoid configuration and an optional expression subtree. An empty
selection visits the avatar root's default `UsdPrimRange`; explicit selection
must be an active, loaded descendant. At least one applied expression API is
required. Verbatim `vrm:expressionName` is the key; empty/duplicate names fail.
Absent binary/override fields mean false/none. Override parsing calls the owner;
unknown tokens fail. Parallel arrays must be readable, correctly typed and
the same length; numeric values must be finite. No fallback repairs malformed
authored binds. Forwarded relationships and reference path remapping are used.

Each morph target must name an active scoped `UsdSkelBlendShape`. Exactly one
scoped mesh with `SkelBindingAPI` must reference it. Parallel
`skel:blendShapeTargets`/`skel:blendShapes` determine the mesh and opaque target
token; neither expression nor prim names infer the correspondence. Missing,
duplicate or shared-mesh mappings fail in this scoped implementation. Multiple
expressions can drive one target. Baseline morph weights are zero; authored
animation values are not folded into expression baseline.

Material targets must be scoped `UsdShadeMaterial` prims applying the schema
named by the owner's `MaterialColorSlots` table. Explicit target indices allow
multiple slots of one material; an absent index array pairs targets by position
for older stages. Canonical RGB `color3f` and optional alpha `float` inputs
supply the default-time baseline. Connected inputs are refused because this
binder does not evaluate shader networks. Only driven canonical inputs are
included, with `overridden = 0`; shader realization nodes are not touched.

The immutable binding retains owned definitions, strings and output arrays,
sharing storage across copies with no retained stage. `Baseline()` combines
the Humanoid skeleton baseline with discovered morph/material slots.
`AdapterConfig()` adds explicit host input mappings and dependencies.
`ApplyTo()` enriches an empty LookAt expression/output configuration for the
same layout ID/version and skeleton. The host supplies a structural layout
version covering the combined outputs, uses this complete baseline when
creating the instance, and keeps the registration adapter alive until runtime
destruction. Rebuild after authored configuration changes.

Discovery errors throw `invalid_argument` with `VRM_EXPRESSION_BINDING_*`
codes and subjects; delegated Humanoid/skeleton errors retain their codes.
Provider evaluation diagnostics continue through the registration adapter.

## Evidence

Measured scope is in the [evidence log](../reports/evidence-log.md#vrm-usd-expression-binding-summary), and current state is in the
[capability matrix](../reference/CAPABILITY_MATRIX.md).
