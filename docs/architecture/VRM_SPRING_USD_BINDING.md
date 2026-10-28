---
status: binding
owner: usd-avatar-runtime
---

# Scoped VRM spring USD binding

`avatarVrmSpringUsdBinding` snapshots applied owner spring and collider schemas
into installed `vrmRig` values. It is stage intake only: no evaluator, history,
fixed clock, rigid-body world or renderer is created. The
[owner schema contract](https://github.com/animu-sphere/usd-vrm-plugins/blob/main/plugins/vrmSchema/docs/SCHEMA_CONTRACT.md)
and [solver](https://github.com/animu-sphere/usd-vrm-plugins/blob/main/libs/vrmRig/README.md#spring-bones)
own their meanings and numerical validation.

## Package boundary

Enable `AVATAR_BUILD_USD_BINDING` and `AVATAR_BUILD_VRM_SPRING_USD_BINDING`.
Only this additional target requires installed `vrmRig` with
`VRMRIG_SPRING_BONE_API >= 1` and contract-v2 `vrmSchema`. Configure checks both
headers and linked symbols, including `GetVrmRootJointsAttr`, so an older install
with the same package version cannot silently satisfy the requirement.
Existing core, Humanoid and LookAt/expression options do not acquire that gate.

Installed consumers select `AvatarRuntime` component `vrm_spring_usd` and link
`AvatarRuntime::avatarVrmSpringUsdBinding`. This imports the generic `usd`
binding and its installed owner packages, plus `vrmRig` and `vrmSchema`.
It imports no expression registration, motion sampling or source-clip adapter.
The host supplies schema resources and dynamic-library discovery.

## Discovery and ownership

[`SpringBoneBinding`](../../adapters/vrm-spring-usd/include/avatarVrmUsd/SpringBoneBinding.h)
takes the generic `SkeletonBindingConfig`: explicit avatar root, skeleton,
layout identity and version. Generic skeleton/placement reading delegates to
the existing [USD binding](USD_BINDING.md). The caller asserts its canonical
basis as for that binder. No Humanoid map or joint-name inference is required.

The avatar root must declare `vrm:schemaContractVersion = 2`. Default-time
active, defined, loaded, non-abstract descendants with applied `VrmSpringBoneAPI`
are read in traversal order. Exactly one joint declaration must have an authored
value, and every token resolves against the selected skeleton's exact joint IDs.
All five parameter arrays must be readable and parallel. Optional center tokens
also resolve against that skeleton.

Forwarded collider-group targets must name active, loaded structural prims below
the avatar root. Their direct active children must apply `VrmColliderAPI`.
Sphere/capsule values are copied without a second interpretation. The group and
collider ID arrays parallel their owner value arrays; spring IDs parallel the
spring values. Shared group targets are read once. There is no hard-coded
secondary-motion prim path or fallback to source JSON.

The first slice admits metre stages and unit-scale rigid rest/placement only.
Other stage units, scaled placement and scaled joints are refused pending the
owner's scale decision. Skeleton-local descriptor rest and separate placement
are retained; the runtime baseline has placement applied to root joints already
and must not be used as solver rest. No stage handle is retained, including by
copies. Rebind after authored configuration or layout changes.

Binding refusals use `VRM_SPRING_BINDING_*` with a subject. Generic reader
refusals retain `MotionUsdReadError`. A temporary owner solver validates the
complete rig and rest pose. `SpringBoneOwnerError` retains the unmodified owner
detail, subject, package name and installed version independently of stage life;
the solver's string-only API supplies no numeric code to forward. Empty authored
spring declarations are currently refused by that owner API. No alternative
root expansion or parameter validation is implemented here.

## Evidence and remaining scope

The [capability matrix](../reference/CAPABILITY_MATRIX.md#vrm-spring-stage-binding-evidence-2026-10-28)
records generated-stage and installed-consumer checks. This boundary provides
configuration for subsequent [runtime integration slices](../roadmap/current.md#vrm-secondary-motion-integration).
It does not establish transactional evaluator history, fixed-tick scheduling,
actual VRM-file intake, live-avatar motion or Hydra/direct publication parity.
