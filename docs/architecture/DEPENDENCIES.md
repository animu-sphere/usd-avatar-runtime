---
status: accepted
owner: usd-avatar-runtime
---

# Repository ownership and dependencies

This document owns placement rules. `avatarRuntime` has no external provider, OpenUSD,
renderer or OpenExec dependency. Every optional target consumes installed owner packages,
and each [component page](../README.md#architecture) states its own requirements. No
reusable target links a renderer: only the opt-in check hosts consume installed
`Toon::AvatarState`. The [project layout](PROJECT_LAYOUT.md) lists the targets.

## 1. Owners

| Repository | Owns | Canonical reference |
| --- | --- | --- |
| `motion-connectors` | external intake, normalization, source clocks, actors and observations | [connector contract](https://github.com/animu-sphere/motion-connectors/blob/main/docs/design/CONNECTOR_CONTRACT.md) |
| `usd-motion-plugins` | motion values, sampling/interpolation/filtering/blending/retargeting, generic validation/skeleton conversion/source rest, recording/replay algorithms and USD motion read/write | [motion contract](https://github.com/animu-sphere/usd-motion-plugins/blob/main/docs/design/MOTION_CONTRACT.md), [retarget policy](https://github.com/animu-sphere/usd-motion-plugins/blob/main/docs/design/RETARGETING_POLICY.md), [execution contract](https://github.com/animu-sphere/usd-motion-plugins/blob/main/docs/design/EXEC_CONTRACT.md) |
| `usd-vrm-plugins` | VRM/VRMA formats, humanoid binding rules, expressions, LookAt, SpringBone-specific evaluation and format evaluator cores | [VRM motion policy](https://github.com/animu-sphere/usd-vrm-plugins/blob/main/docs/design/VRM_MOTION_POLICY.md), [vrmRig](https://github.com/animu-sphere/usd-vrm-plugins/blob/main/libs/vrmRig/README.md) |
| `usd-mmd-plugins` | PMX/VMD formats, MMD morph/control/IK meaning, PMX modes and bone/body coupling | [MMD motion contract](https://github.com/animu-sphere/usd-mmd-plugins/blob/main/docs/design/MOTION_CONTRACT.md), [physics integration](https://github.com/animu-sphere/usd-mmd-plugins/blob/main/docs/design/PHYSICS_INTEGRATION.md), [stage contract](https://github.com/animu-sphere/usd-mmd-plugins/blob/main/docs/design/STAGE_CONTRACT.md#13-physics) |
| `usd-physics-plugins` | neutral worlds, fixed Step implementation, collision/solver/backend integration and generic UsdPhysics import/conversion | [rigid-body contract](https://github.com/animu-sphere/usd-physics-plugins/blob/main/docs/design/RIGID_BODY_CONTRACT.md), [USD bridge](https://github.com/animu-sphere/usd-physics-plugins/blob/main/docs/design/USD_BRIDGE_CONTRACT.md), [package contract](https://github.com/animu-sphere/usd-physics-plugins/blob/main/docs/architecture/PACKAGE_CONTRACT.md) |
| `usd-avatar-runtime` | discovery, composition, evaluation DAG, shared-world barrier, clock/fixed-step coordination, reset/seek, adapter lifetime and final-state publication | [evaluator](../contracts/EVALUATOR.md), [lifecycle](FRAME_LIFECYCLE.md) |
| `usd-stage-runner`, other hosts and applications | asset selection, inputs, session construction, UI, host loop and time requests; invoke runtime orchestration without duplicating format evaluation | [runtime host boundary](../contracts/EVALUATOR.md#5-shared-world-physics-orchestration) |
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
directly, and legacy `StageClip` delegates to the same helper. See the
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

## 3. Integration questions

- The renderer's existing authored-stage boundary and late-input interfaces
  need an agreed adapter for common evaluated state. A design here does not
  change the renderer's public API or prove output parity.
- MMD control dependencies, including bone morphs before IK, must fit the
  shared phase model without reimplementing the control evaluator. This is
  `RT-O3` in the [roadmap](../roadmap/current.md#open-decisions).
- Avatar scheduling, shared-world barriers and fixed-step coordination belong to
  this runtime. The MMD owner's
  [physics integration sections 2 and 6](https://github.com/animu-sphere/usd-mmd-plugins/blob/main/docs/design/PHYSICS_INTEGRATION.md)
  assign the shared-world barrier to runtime and ticks to the host.
  The scoped MMD adapter consumes that aligned boundary. Broader host binding,
  shared non-avatar participation and checkpoint recovery remain RT-O6 work.
- Owner versions are pinned in each package descriptor; only `avatarVrmSession` composes
  them so far. The other adapters still need reproducible composition evidence.

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

The [MMD adapter](MMD_ADAPTER.md) supplies a
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
