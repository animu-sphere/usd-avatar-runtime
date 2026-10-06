---
status: binding
owner: usd-avatar-runtime
---

# Scoped VRM USD LookAt binding

`avatarVrmLookAtUsdBinding` connects the installed owner's applied
`VrmLookAtAPI` and [Humanoid binding](VRM_USD_BINDING.md) to the existing
[VRM registration adapter](VRM_ADAPTER.md). This advances RT-O4 by extracting
authored gaze configuration without changing the revision-3 core C ABI or
copying format evaluation into the runtime.

## Build and package boundary

The target is built when `AVATAR_BUILD_USD_BINDING`,
`AVATAR_BUILD_VRM_USD_BINDING` and `AVATAR_BUILD_VRM_ADAPTER` are enabled.
It consumes the existing installed `vrmSchema`, `vrmRig`, `motionRetarget`
and OpenUSD dependencies, with `motionUsd` strict skeleton reading through the
generic USD binding. Motion sampling and the source-clip adapter are optional.
Schema resources and dependency DLL discovery remain the host's responsibility.

Installed hosts request the target explicitly:

```cmake
find_package(AvatarRuntime 0.1.0 EXACT CONFIG REQUIRED COMPONENTS vrm_lookat_usd)
target_link_libraries(host PRIVATE AvatarRuntime::avatarVrmLookAtUsdBinding)
```

This resolves `vrm`, `vrm_usd` and `usd` transitively. The existing `vrm_usd`
component still imports no `vrmRig`; the core imports no provider dependencies.

## Discovery and owned configuration

[`LookAtBinding`](../../adapters/vrm-usd/include/avatarVrmUsd/LookAtBinding.h)
takes a `HumanoidBindingConfig` and optional LookAt prim path. Empty selection
requires exactly one applied `VrmLookAtAPI` in the avatar root's default
`UsdPrimRange`. Explicit selection requires an active, loaded descendant with
that API. Humanoid/skeleton validation is delegated to the existing binders.
No prim-name or joint-name heuristics are used.

`vrm:lookAt:raw`, when present, must be a JSON-object string. The owner
`vrmRig::ParseLookAtRangeMaps` reads VRM 0.x curves and VRM 1.0 range maps;
its warnings remain available through `Warnings()`. Missing raw data retains
owner defaults. Typed `vrm:type` must be a readable `bone` or `expression`
token and overrides the raw type. Raw head offsets remain source VRM metres;
they are not multiplied by USD stage units. Skeleton rest and placement use
the generic binder's metre conversion.

An authored `vrm:skeleton` relationship must forward to the same single
skeleton as the Humanoid binding. When absent, the Humanoid skeleton supplies
the explicit context. The Humanoid head role is required; its ancestry must
have unit scale. Authored eye tokens must be nonempty readable tokens naming
distinct joints in that skeleton. Bone LookAt requires at least one eye, each
directly parented to the head; its authored parent-local rest quaternion is
retained. Expression LookAt permits no eyes and supplies no eye mappings.
Copies share immutable owned storage and retain no stage handle.

`AdapterConfig(evaluatorId, gaze, after)` returns an owned
`ExpressionAdapterConfig` with layout identity/version, owner rig, head/eye
bindings and explicit evaluator dependencies. The host selects source, actor
and namespaced gaze channel, then keeps the registration adapter alive for
its runtime. Expression-type LookAt still needs host-supplied expression,
morph and material bindings to realize resolved weights. This binder does
not discover those output targets or invent actual-avatar expression binds.
The separate [Expression binding](VRM_EXPRESSION_USD_BINDING.md) can now
populate those fields through `ApplyTo()`, with its complete output baseline.

Rebuild after authored configuration changes and update layout identity for
structural changes. Discovery failures throw `invalid_argument` with
`VRM_LOOKAT_BINDING_*` codes and a subject; delegated failures retain their
codes. Owner parser warnings are separate from frame diagnostics; the host
associates the LookAt prim and asset/provider provenance with them.

## Evidence and remaining scope

`adapters.vrm_usd_lookat` covers scoped/explicit discovery, both schema types,
typed-over-raw precedence, both raw formats, defaults/warnings, forwarded
relationships/reference remapping, malformed type/raw/head/eye/skeleton
rejection, scale ancestry, centimetre rest/placement, nonidentity eye rest,
single-eye rigs, stage/copy lifetime, reset, absence and retained snapshots.
Bone quaternions and expression weights match direct owner calls at `1e-6`;
expression output uses constructed test sinks. `adapters.vrm_usd_installed`
also runs this check through installed `vrm_lookat_usd`, after verifying that
requesting only `vrm_usd` still imports no evaluator.

On 2026-10-05 a private avatar was opened read-only through the installed VRM
file-format plugin: 128 joints, Expression-type LookAt and no parser warnings.
Its extracted configuration drives constructed expression test sinks with
owner-weight parity at `1e-6`, reset and retained state after stage/runtime
destruction. Paths/hashes and commands remain in ignored build evidence; no
asset is copied or included in default tests.

This validates one real-avatar LookAt configuration with test gaze. The
[motion parity tool](MOTION_USD_BINDING.md#opt-in-asset-parity-tool) additionally
checks that test gaze against the head pose from seven real clips, using actual
expression outputs and an independent owner head calculation. Explicit native
VRMA keyed/default gaze fixtures now drive the same avatar without probes,
through the tool's owner-selected reader handoff and clip placement policy.
Representative captured gaze, real-avatar bone-eye conformance, connectors, rendering,
milestones and ABI freeze remain open in
the [roadmap](../roadmap/current.md).
Actual expression/output extraction and test-gaze composition now have scoped
evidence in the separate Expression binding document.
