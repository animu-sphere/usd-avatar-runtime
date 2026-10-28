---
status: accepted
owner: usd-avatar-runtime
---

# Repository ownership and dependencies

This document owns placement rules. The initial `avatarRuntime` CMake target
has no external provider, OpenUSD, renderer or OpenExec dependency. It exports
an experimental installable CMake package. The optional
[`avatarVrmAdapter`](VRM_ADAPTER.md) consumes the installed `vrmRig` package,
with transitive `motionCore`/OpenUSD dependencies isolated from runtime core.
The optional [`avatarMotionAdapter`](MOTION_ADAPTER.md) consumes installed
`motionSampling`/`motionRetarget` packages with the same core isolation and
maps selected owner scalar/gaze fields through a host-side input assembler.
The optional [`avatarUsdBinding`](USD_BINDING.md) reads USD skeleton/rest and
placement into owned baseline and motion-owner values, using installed
`motionUsd` typed strict reading and its public `motionRetarget` dependency,
alongside `usdSkel`/`usdGeom`; it adds no core dependency.
The motion and USD components require the owner's 0.5.4 release or a compatible
later package, which supplies validation and typed skeleton/clip/rest APIs.
Installed component lookup keeps the same version requirements and header/link
checks; core-only lookup still resolves no provider.
The optional [`avatarVrmUsdBinding`](VRM_USD_BINDING.md) adds installed
`vrmSchema` only to a separate schema discovery target above the generic binder.
The optional [`avatarVrmLookAtUsdBinding`](VRM_LOOKAT_USD_BINDING.md) composes
that target with the VRM registration adapter for gaze configuration extraction;
requesting only `vrm_usd` still imports no evaluator.
The optional [Expression USD binding](VRM_EXPRESSION_USD_BINDING.md) composes
the same owner packages plus `usdShade` to extract expression definitions and
canonical output baseline; its component imports neither LookAt binding nor
motion sampling or the source-clip adapter. It shares the generic owner's
skeleton-reading dependency.
The optional [`avatarMotionUsdBinding`](MOTION_USD_BINDING.md) connects installed
`motionUsd` semantic clip reading and owner source rest to motion evaluation,
above the generic USD binder. The opt-in `avatarMotionCheck` host composes that
binding, VRM schema Humanoid discovery and clip pose evaluation.
The host's optional `AVATAR_MOTION_CHECK_TOON` mode consumes installed
`Toon::AvatarState` for resolved-state probe transport only; no reusable runtime
or provider target links the renderer. The optional
[MMD adapter](MMD_ADAPTER.md) consumes installed `mmdPhysicsAdapter`/`mmdControl`
through a C++ registration boundary; its host supplies path/handle mapping and
world snapshots. Its opt-in test host links `physicsUsd`/`physicsJolt`.
The separate optional [USD physics resources](PHYSICS_USD_RESOURCES.md) target
consumes installed `physicsUsd`/`physicsCore` for host-owned construction and
teardown. It imports no backend or format dependency and changes no core or
MMD-registration dependency. The Stage Runner smoke composes this target with
its explicitly selected Jolt backend.
Its separately opt-in `AVATAR_MMD_TOON_SMOKE` executable also consumes installed
`Toon::AvatarState`; that renderer dependency is absent from the reusable MMD
adapter and its installed component.
Connector adapters and OST composition remain unconfigured.

The optional [VRM spring USD binding](VRM_SPRING_USD_BINDING.md) consumes the
generic USD binder plus installed `vrmSchema` and `vrmRig`. Its separate option
and `vrm_spring_usd` installed component check the solver feature API and v2
root declarations without adding those requirements to existing expression or
core builds. It creates no evaluator, physics world or scheduler.

The optional [live usdview MMD host](../guides/live-mmd-physics.md) composes the
installed MMD, physics/Jolt and Stage Runner packages with USD binding/output
and Python. Its backend/UI dependencies belong only to the application module;
the reusable adapter and core boundaries above are unchanged.

The adopted [boundary cleanup policy](../design/BOUNDARY_POLICY.md) refines
these placement rules. The descriptions above record current integrations;
generic validation in the motion adapter now delegates to installed owner
APIs. USD skeleton and canonical clip-space validation now also delegate to
installed owner readers. Coherent typed descriptor/source-rest read results
are consumed directly, with no runtime rebuilding from raw arrays.
See the [cleanup workstreams](../roadmap/current.md#boundary-cleanup-workstreams).

## 1. Owners

| Repository | Owns | Canonical reference |
| --- | --- | --- |
| `motion-connectors` | external intake, normalization, source clocks, actors and observations | [connector contract](https://github.com/animu-sphere/motion-connectors/blob/main/docs/design/CONNECTOR_CONTRACT.md) |
| `usd-motion-plugins` | motion values, sampling/interpolation/filtering/blending/retargeting, generic validation/skeleton conversion/source rest, recording/replay algorithms and USD motion read/write | [motion contract](https://github.com/animu-sphere/usd-motion-plugins/blob/main/docs/design/MOTION_CONTRACT.md), [retarget policy](https://github.com/animu-sphere/usd-motion-plugins/blob/main/docs/design/RETARGETING_POLICY.md), [execution contract](https://github.com/animu-sphere/usd-motion-plugins/blob/main/docs/design/EXEC_CONTRACT.md) |
| `usd-vrm-plugins` | VRM/VRMA formats, humanoid binding rules, expressions, LookAt, SpringBone-specific evaluation and format evaluator cores | [VRM motion policy](https://github.com/animu-sphere/usd-vrm-plugins/blob/main/docs/design/VRM_MOTION_POLICY.md), [vrmRig](https://github.com/animu-sphere/usd-vrm-plugins/blob/main/libs/vrmRig/README.md) |
| `usd-mmd-plugins` | PMX/VMD formats, MMD morph/control/IK meaning, PMX modes and bone/body coupling | [MMD motion contract](https://github.com/animu-sphere/usd-mmd-plugins/blob/main/docs/design/MOTION_CONTRACT.md), [physics integration](https://github.com/animu-sphere/usd-mmd-plugins/blob/main/docs/design/PHYSICS_INTEGRATION.md), [stage contract](https://github.com/animu-sphere/usd-mmd-plugins/blob/main/docs/design/STAGE_CONTRACT.md#13-physics) |
| `usd-physics-plugins` | neutral worlds, fixed Step implementation, collision/solver/backend integration and generic UsdPhysics import/conversion | [rigid-body contract](https://github.com/animu-sphere/usd-physics-plugins/blob/main/docs/design/RIGID_BODY_CONTRACT.md), [USD bridge](https://github.com/animu-sphere/usd-physics-plugins/blob/main/docs/design/USD_BRIDGE_CONTRACT.md), [package contract](https://github.com/animu-sphere/usd-physics-plugins/blob/main/docs/architecture/PACKAGE_CONTRACT.md) |
| `usd-avatar-runtime` | discovery, composition, evaluation DAG, shared-world barrier, clock/fixed-step coordination, reset/seek, adapter lifetime and final-state publication | [evaluator](../contracts/EVALUATOR.md), [lifecycle](FRAME_LIFECYCLE.md) |
| `usd-stage-runner` and other hosts | host loop, inputs and time requests; invoke runtime orchestration | [runtime host boundary](../contracts/EVALUATOR.md#5-shared-world-physics-orchestration) |
| `hydra-toon` | shader/material realization, GPU resources, backend and rendering transport consumption | [integration scope](https://github.com/animu-sphere/hydra-toon/blob/main/docs/design/INTEGRATION_SCOPE_POLICY.md) |
| `open-strata` | OST build/package/runtime composition tooling | [project documentation](https://github.com/animu-sphere/open-strata/tree/main/docs) |

A new format follows the same boundary: it contributes bindings and evaluators
that produce the common resolved state. It does not add format branches to
every output consumer.

## 2. Allowed integration direction

These are data-flow and responsibility edges, not a claim that every arrow is
a link-time dependency:

```text
connectors -> input bridge -> common input
generic/format evaluator cores -> registration adapter -> runtime plan
authored stage -> binding adapters -> instance configuration
runtime plan -> resolved state -> Hydra/direct/recording adapters
```

Provider adapters may depend on the runtime's small registration contract.
The orchestration core must not depend on provider-private parser/model APIs.
An adapter marshals existing library values into the common contract; it does
not move or copy the algorithm into the runtime.

The `MotionPose -> AvatarInputFrame` bridge remains runtime-owned: it maps
runtime channel/source/actor identities, owns input arrays and reports unmapped
fields. External source clock/actor normalization stays in `motion-connectors`.
Supplied affine mappings are preserved rather than recomputed by the bridge.

USD integration follows the same rule. `motionUsd` owns stage motion reading,
source skeleton interpretation, generic validation and skeleton/rest conversion.
Runtime binding owns avatar/layout/skeleton identity, joint-ID mapping, baseline
state and root placement. `SkeletonBinding` consumes typed owner skeleton results;
the parity host uses the owner `MotionStageRead` value returned by `ReadStageClip`
directly, and legacy `StageClip` delegates to the same helper. The owner's WS-O4
dependency/result decision is resolved. See the
[motion USD binding](MOTION_USD_BINDING.md) for compatibility and host evidence.
Recording adapters publish resolved motion to
the owner recorder; runtime does not own resampling, compression or clip construction.

Renderer consumers receive resolved outputs. They do not call back into VRM
LookAt, expression arbitration, MMD morph interpretation, IK or Physics Step. Rendering
late latching and GPU realization remain renderer responsibilities, with
frame/state identity preserved across publication.

Generic motion values are consumed from their owner. Cross-package ABI views
must be explicitly mapped to them, rather than defining a competing joint
vocabulary, coordinate basis, sampler or recording format.

The near-term integration order is VRM, motion/connector completion,
`hydra-toon` direct consumption, then MMD generality validation. These adapters
are the evidence needed to correct/freeze the common contract; core remains
independent of provider-private types. See the
[implementation sequence](../roadmap/current.md#near-term-implementation-sequence).

## 3. Integration questions

- The renderer's existing authored-stage boundary and late-input interfaces
  need an agreed adapter for common evaluated state. A design here does not
  change the renderer's public API or prove output parity.
- MMD control dependencies, including bone morphs before IK, must fit the
  shared phase model without reimplementing the control evaluator. This is
  `RT-O3` in the [roadmap](../roadmap/current.md#open-decisions).
- The 2026-10-20 direction assigns avatar scheduling, shared-world barriers
  and fixed-step coordination to this runtime. The MMD owner's
  [physics integration sections 2 and 6](https://github.com/animu-sphere/usd-mmd-plugins/blob/main/docs/design/PHYSICS_INTEGRATION.md)
  now assign the shared-world barrier to runtime and ticks to the host.
  The scoped MMD adapter consumes that aligned boundary. Broader host binding,
  shared non-avatar participation and checkpoint recovery remain RT-O6 work.
- Package versions, optional dependency selection and runtime discovery need
  reproducible OST composition evidence. No package pins are selected by
  these documents.

When an owner lacks an output or a callable evaluator boundary, document the
gap and extend that owner's contract. Do not fill it with a private second
implementation here.

## 4. Physics boundary

```text
host loop / inputs / time request (stage-runner, Mimikuri or another host)
              |
avatar-runtime DAG / clock / shared-world barrier
              |
format pre-state -> physics adapter -> owner world Step / snapshot
              |                              |
              +------ format feedback <------+
              |
final EvaluatedAvatarState -> renderer consumer
```

This is the adopted ownership; the [MMD adapter](MMD_ADAPTER.md) supplies a
scoped registration/world-bridge boundary and a generated backend test host.
The [evaluator contract](../contracts/EVALUATOR.md#5-shared-world-physics-orchestration)
is the sole owner of the barrier/order rules; the
[frame lifecycle](FRAME_LIFECYCLE.md#5-physics-clock-and-discontinuities) owns
clock, catch-up, reset/seek and unload. Runtime controls when to call fixed
Step; the physics owner implements the world and Step. Backend object lifetime
stays with physics, while runtime owns adapter lifetime and coordinates safe
attachment/detachment. Format-specific coupling stays with MMD/VRM owners.

Core must have no mandatory `usd-mmd-plugins`, Jolt or `hydra-toon` dependency.
Integrate through optional MMD, physics, VRM and renderer adapters. The MMD
target/option is defined above. The separate optional
[Stage Runner session](STAGE_RUNNER_SESSION.md) consumes installed `runtimeCore`
over a caller-created world; automatic application/world binding remains open.
Normal CI should cover core units and
headless smoke in the applicable configuration, with heavy asset/backend/render
end-to-end checks opt-in. Optional integration dependencies must not leak into
core-only build/install/import. Planned work and acceptance gates live in the
[physics orchestration roadmap](../roadmap/current.md#physics-orchestration-milestones).
