---
status: binding
owner: usd-avatar-runtime
---

# Scoped VRM USD Expression binding

`avatarVrmExpressionUsdBinding` extracts applied owner `VrmExpressionAPI`
definitions and their canonical output layout into the existing
[VRM registration adapter](VRM_ADAPTER.md). This advances RT-O4 without
changing the revision-3 C ABI. `vrmRig` still owns expression arbitration,
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

## Evidence and remaining scope

`adapters.vrm_usd_expression` compares runtime morph and all six material colour
slots, including alpha, with direct owner evaluation at `1e-6`. It covers
override arbitration, indexed/legacy binds, shared expression targets, alias
tokens, LookAt composition, subtree selection, forwarded/reference remapping,
malformed bindings, connected-input rejection, owned copy/stage lifetime,
explicit zero/absence, reset and snapshots retained after runtime destruction.
`adapters.vrm_usd_installed` separately builds the explicit expression consumer
and checks component dependency isolation before importing LookAt for composition.

On 2026-10-06 the existing private 128-joint avatar was read in place through
the installed VRM file-format plugin: 18 expressions and 48 morph output slots,
with no material colour binds. Test scalar/gaze input -> actual Expression-type
LookAt/Expression bindings -> retained state matches direct owner output at
`1e-6`, including reset and stage/runtime lifetime. Local asset paths, hashes,
provider versions and logs stay in ignored build evidence; no asset is copied.
Material extraction has constructed-USD evidence, not real-avatar evidence.

The [motion parity tool](MOTION_USD_BINDING.md#opt-in-asset-parity-tool) now
composes seven real motion clips with these actual bindings and explicit host
scalar/gaze probes, checking every morph and joint against separate owner calls.
Native VRMA expression/gaze intake remains a distinct missing owner boundary.

Additional avatars (including
bone eyes and material binds), shared-mesh targets, live connectors, renderer
consumption and milestone/ABI acceptance remain open in the
[roadmap](../roadmap/current.md).
