---
status: proposed
owner: usd-avatar-runtime
---

# Evaluator contract and phase order

This proposal develops [design policy sections 7–8 and 16](../design/DESIGN_POLICY.md).
It owns invocation and order, not the provider's motion/format algorithm.

[`evaluator.h`](../../include/avatarRuntime/evaluator.h) and the direct runtime
implement experimental registration, phase barriers and explicit predecessor
dependencies. This does not freeze RT-O3: actual motion/VRM/MMD adapter plans
still need conformance evidence.

The [near-term direction](../design/NEAR_TERM_PLAN.md#5-validate-evaluator-dependencies)
requires real-provider validation during overlapping Runtime Phases A/B:

```text
VRM: motion pose -> humanoid resolution -> LookAt -> expression arbitration
                                                    -> final state
MMD: base pose -> bone morph -> control / IK -> physics boundary
                                               -> final pose / morph state
```

These dependencies validate or correct the proposed phases below. MMD is a
second-family test of the scheduler's generality; do not freeze a VRM-only
model. A physics boundary can be tested without integrating a full physics
backend in the initial slice. Real owner calls may combine substeps atomically.

## 1. Initial phase sequence

The adopted [boundary policy section 10](../design/BOUNDARY_POLICY.md#10-shared-phase-model)
adds a conceptual common graph: input -> motion sample -> retarget -> format
pre-physics -> physics boundary -> format post-physics -> expression/LookAt/control
-> resolve -> publish. Its labels are not new ABI enum values. Runtime owns
the graph, dependency validation and deterministic handoff; format owners
register calls without exposing their algorithms to core.

The table below records the earlier proposal and current prototype mapping.
Reconcile it with pre/post-physics provider requirements under RT-O3 before
freeze. Preserve MMD bone-morph/control/IK dependencies and atomic owner calls;
the conceptual graph does not require all control work to occur after physics.

Numeric positions preserve the policy's conceptual order. They are not frozen
C ABI enum values.

| Position | Phase |
| --- | --- |
| 0 | input collection |
| 1 | timeline / clip sampling |
| 2 | motion blending |
| 3 | retargeting |
| 4 | base pose construction |
| 5 | constraints / IK |
| 6 | LookAt / gaze evaluation |
| 7 | expression / morph evaluation |
| 8 | secondary motion / physics |
| 9 | final pose consolidation |
| 10 | appearance/material overrides |
| 11 | output publication |

Collection and publication are runtime boundaries; a provider need not
implement each phase. Expression resolution may compute material values in
phase 7; phase 10 consolidates appearance, rather than resolving expressions
again. Final pose consolidation includes prior gaze, morph and secondary
motion contributions exactly once.

This order must be validated against format-specific dependency requirements
before freezing. MMD bone morph/control dependencies can require work before
IK; the runtime must not move them after IK merely because the general
expression/morph label appears later. Decide whether staged contributions,
explicit dependency substeps or a revised common sequence are appropriate
with the format owner (`RT-O3`). Do not give the plugin an independent loop.

Prototype phase constants cover provider positions 1–10; collection and
publication remain runtime boundaries. Dependencies in `after` must name
selected registered evaluators and cannot point backwards across a phase
barrier. Topological sorting uses `(phase, UTF-8 ID byte order)` for independent
steps; descriptor registration/selection order has no influence. Same-phase
overlapping writers need a dependency path. Across phases the barrier supplies
explicit order. A later write replaces the resolved channel value; additive
composition must be performed by the owning provider using its working view.

Sibling inspection supports treating `mmdControl::Evaluator::Evaluate` as one
atomic control step in the constraints phase: it already handles bone morphs,
appends and IK internally in MMD order. Do not schedule its bone effects again
in the expression phase. `vrmRig`'s expression-type LookAt returns contributions
for `ExpressionResolver`, which should run after gaze and resolve them once.
The optional [VRM adapter](../architecture/VRM_ADAPTER.md) implements an atomic
expression-phase callback that invokes LookAt before the owner
expression resolver and writes resolved effects once. Expression rigs feed
named contributions to the resolver; bone rigs write eye rotations composed
with bound authored rest rotations and declare pose writes. Constructed-rig
parity tests validate both types, including working head pose from an earlier
base-pose evaluator. The optional [motion adapter](../architecture/MOTION_ADAPTER.md)
executes owner clip sampling/retarget atomically in `AR_PHASE_RETARGET` and
supplies an explicit predecessor for VRM. Constructed-rig tests verify owner
motion -> working head -> LookAt -> Expression numeric parity. MMD and actual
motion/avatar bindings still require conformance evidence before freeze.

## 2. Registration descriptor

The proposed descriptor declares stable provider/evaluator identity and
version, ABI range, phase membership, supplied/required capabilities,
read/write state domains, dependencies and lifecycle callbacks.
Registration discovers code; instance binding supplies avatar-specific data.
Discovery must not imply one global mutable evaluator instance.

Validate duplicate identifiers, missing required capabilities, cycles and
conflicting state writers before execution. Multiple writes require an explicit
composition policy or ordered dependency. A tie-breaker must be stable and
documented; discovery order is not an evaluation policy. Phase crossings and
configurable exceptions must be declared and validated centrally.

Registration copies identity/version, required/supplied capability lists and
predecessor IDs. Binding selects a subset of evaluators for one instance and
creates independent provider state. Read/write domains are pose, deformation,
material and visibility. Callback working/prior views expose only the union of
declared read/write domains; writer hooks reject undeclared domains, invalid
indices and invalid values. An ignored writer error still fails the frame.
Providers must treat const views as immutable and cannot retain callback views.
Read-after-write requirements must be declared as dependencies; lexical order
of otherwise independent readers does not imply a provider-specific policy.

## 3. Invocation

Conceptually, `Evaluate(context, mutableState)` receives an immutable input,
bound configuration, explicit time, permitted prior state, diagnostic hooks
and access to the declared working-state domains. Its writes are scoped to
those declarations. The conceptual C++ interface is a convenience model; the
stable package boundary is the [C ABI proposal](ABI.md).

No renderer/backend dependency is allowed in evaluator logic. Device I/O and
USD writeback do not occur inside frame evaluation. Any stochastic operation
needs an explicit captured seed. Identical input, configuration, evaluator
versions and prior state must yield identical results within declared numeric
rules; stateful simulation cannot promise this from input alone.

Reset/restore and failed-frame behavior follow the
[frame lifecycle](../architecture/FRAME_LIFECYCLE.md); callback shapes and
checkpoint ownership remain open. Structured diagnostics are returned even
when an evaluator refuses an input.

## 4. OpenExec and scheduling

OpenExec wrappers invoke the same evaluator cores as direct calls. Driver
integration respects the motion owner's
[execution contract](https://github.com/animu-sphere/usd-motion-plugins/blob/main/docs/design/EXEC_CONTRACT.md).
Execution-mechanism differences require parity evidence; they are not license
to change algorithms or silently author results onto the stage.

Start with ordered serial execution. Read/write declarations prepare later
dependency scheduling, but do not authorize implicit parallelism now.
Multi-avatar parallelism also needs resource lifetime and shared-state checks.
