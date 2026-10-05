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
`motionSampling`/`motionRetarget` packages with the same core isolation.
MMD/connector adapters and OST composition remain unconfigured.

## 1. Owners

| Repository | Owns | Canonical reference |
| --- | --- | --- |
| `motion-connectors` | external intake, normalization, source clocks, actors and observations | [connector contract](https://github.com/animu-sphere/motion-connectors/blob/main/docs/design/CONNECTOR_CONTRACT.md) |
| `usd-motion-plugins` | motion values, sampling, blending, retargeting and recording primitives | [motion contract](https://github.com/animu-sphere/usd-motion-plugins/blob/main/docs/design/MOTION_CONTRACT.md), [retarget policy](https://github.com/animu-sphere/usd-motion-plugins/blob/main/docs/design/RETARGETING_POLICY.md), [execution contract](https://github.com/animu-sphere/usd-motion-plugins/blob/main/docs/design/EXEC_CONTRACT.md) |
| `usd-vrm-plugins` | VRM/VRMA formats, humanoid binding rules, expressions, LookAt and format evaluator cores | [VRM motion policy](https://github.com/animu-sphere/usd-vrm-plugins/blob/main/docs/design/VRM_MOTION_POLICY.md), [vrmRig](https://github.com/animu-sphere/usd-vrm-plugins/blob/main/libs/vrmRig/README.md) |
| `usd-mmd-plugins` | PMX/VMD formats, MMD morph/control/IK meaning and format adapters | [MMD motion contract](https://github.com/animu-sphere/usd-mmd-plugins/blob/main/docs/design/MOTION_CONTRACT.md), [physics integration](https://github.com/animu-sphere/usd-mmd-plugins/blob/main/docs/design/PHYSICS_INTEGRATION.md) |
| `usd-avatar-runtime` | discovery, composition, order, lifecycle, runtime state, scheduling and publication | [design policy](../design/DESIGN_POLICY.md) |
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

Renderer consumers receive resolved outputs. They do not call back into VRM
LookAt, expression arbitration, MMD morph interpretation or IK. Rendering
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
- The MMD physics proposal names shared physics/backend and stage-runner
  owners. Avatar evaluation order, shared-world stepping, fixed timestep and
  feedback ownership must be reconciled before an adapter is implemented
  (`RT-O6`). No physics solver or stage-runner implementation is added here.
- Package versions, optional dependency selection and runtime discovery need
  reproducible OST composition evidence. No package pins are selected by
  these documents.

When an owner lacks an output or a callable evaluator boundary, document the
gap and extend that owner's contract. Do not fill it with a private second
implementation here.

## 4. Physics boundary

```text
avatar pre-physics evaluation
              |
shared physics world / usd-stage-runner / physics runtime
              |
        physics result
              |
avatar post-physics evaluation -> EvaluatedAvatarState
```

This is the intended ownership boundary, not an implemented physics adapter.
The runtime owns avatar evaluation order. Physics backend/world lifetime,
fixed timestep and shared-world stepping ownership remain RT-O6 decisions.
Format-specific MMD/VRM physics semantics remain with their owners. Validate
this boundary in the MMD slice without requiring full physics integration or
putting a solver/world into runtime core.
