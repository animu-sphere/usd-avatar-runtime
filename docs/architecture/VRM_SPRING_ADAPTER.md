---
status: binding
owner: usd-avatar-runtime
---

# Scoped VRM spring evaluator

`avatarVrmSpringAdapter` registers installed `vrmRig` spring evaluation in the
existing secondary phase. It consumes owned rig values, maps current working
pose to the owner and publishes only simulated rotations. The
[owner solver](https://github.com/animu-sphere/usd-vrm-plugins/blob/main/libs/vrmRig/README.md#spring-bones)
owns spring mathematics, root expansion, collider response and reset behavior.
No solver implementation or format branch enters runtime core.

## Package and configuration

Enable `AVATAR_BUILD_VRM_SPRING_ADAPTER`. Installed consumers request component
`vrm_spring` and link `AvatarRuntime::avatarVrmSpringAdapter`. Only installed
`vrmRig` and its dependencies are required; no schema, USD binding, motion
sampling, expression adapter or physics backend is selected. Configure checks
the owner feature macro, linked methods and value-copy availability. Core-only
and existing expression-only lookups keep their previous dependency boundary.

[`SpringBoneAdapterConfig`](../../adapters/vrm-spring/include/avatarVrm/SpringBoneAdapter.h)
supplies evaluator/layout identity and version, exact skeleton/joint identities
in full owner order, owner rig values, rigid placement and an explicit positive
fixed duration representable as a float. An immutable adapter can serve several
instances; keep it and its callback code alive until runtime destruction.

The [spring stage binding](VRM_SPRING_USD_BINDING.md) provides these values:
copy `Rig()` and `RestPose().placement`, and use `Skeleton()`'s `SkeletonId()`,
`JointIds()` and `Baseline()` layout identity/version. Use that baseline when
creating the runtime instance. Neither the evaluator nor its state retains USD.
Rebinding a changed layout/configuration requires a new instance.

## Plan and pose mapping

The descriptor declares `AR_PHASE_SECONDARY`, stateful callbacks and pose
reads/writes, and supplies `avatar.vrm.springBone` version 1. Earlier motion,
LookAt and expression phases complete before it. Explicit `after` IDs can
require those providers or order same-phase work. Same-phase overlapping pose
writers still need a dependency path; registration order grants no exception.
With a shared rigid-body world, the host selects whether this evaluator belongs
to the prefix or suffix through the existing plan boundary. No independent
world Step or new phase is introduced here.

Every call validates layout, the complete selected skeleton's exact identities
and parents, unit joint scale and float-representable translations. Runtime
roots are world-space; owner roots are skeleton-local with separate placement.
The configured placement orientation converts root rotations. Common animated
root translation is carried by placement, inferred from the first root and its
owner rest offset. This preserves root motion without changing the owner's
fixed rest translations. Multiple roots must share that placement translation;
independent root displacement, animated non-root translations, scaling and
non-metre stage intake remain outside the admitted owner boundary.

The owner receives the current unsimulated working pose each time. Its tail
result identifies simulated joints, including expanded VRM 0.x roots; the
adapter does not repeat that expansion. Only those rotations are written, with
root rotations converted back to runtime world space. Translations, scales,
unsimulated joints, other skeletons, deformation, materials, visibility and
source provenance retain their working values.

## Ticks, transactions and reset

The first successful frame seeds history with owner `Reset` at that frame's
animated pose and performs no `Step`. Each subsequent successful frame must be
one configured duration later (absolute tolerance `max(1e-9, duration * 1e-6)`
seconds, capped at `duration * 1e-3`); it calls `Step` once with that duration. Equal-time display requests,
skipped ticks and time jumps are refused. Frame IDs follow the core's ordinary
monotonic rules. An increasing-generation `reset_instance` permits a new seed
instant, including seek/loop wrap, without one large or negative Step.

`begin_frame` copies the instance's committed owner solver and tick time into
independent trial values. All owner calls modify that trial. Only infallible
`end_frame(commit=1)` swaps it into committed history, after runtime final-state
validation and snapshot allocation. Abort discards the trial, including after
a later provider or output-validation failure. A failed frame can therefore
retry at the same instant without a second committed step. Reset recreates
provider state; it never pretends to restore arbitrary history by calling Reset.
The [frame lifecycle](FRAME_LIFECYCLE.md) remains authoritative.

Diagnostics use `VRM_SPRING_ADAPTER_*`. Owner refusals retain their complete
detail and installed version in the message, with `usd-vrm-plugins.vrmRig`
origin and bound skeleton subject. The string-only owner API supplies no numeric
code. Runtime stamps evaluator/instance/frame/phase. No C ABI field is added.

The evaluator validates supplied fixed instants; it owns no clock or accumulator.
This slice uses direct runtime requests for generated conformance checks. A
backend-free runtime fixed-tick session, pause/catch-up/backlog handling, clip
host, real-avatar intake and renderer delivery remain
[roadmap work](../roadmap/current.md#vrm-secondary-motion-integration).
It must initialize at the declared seed instant before advancing fixed ticks.
Display and consumer retries read retained snapshots and do not call evaluation.
If a rigid-body world participates, its existing single-Step barrier and group
failure/reset rules still apply; copying the spring solver does not roll back
that world.

The [capability matrix](../reference/CAPABILITY_MATRIX.md#vrm-spring-evaluator-evidence-2026-10-28)
records the generated source/installed tests and their scope.
