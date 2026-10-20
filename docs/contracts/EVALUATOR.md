---
status: proposed
owner: usd-avatar-runtime
---

# Evaluator contract and phase order

This proposal develops [design policy sections 7–8 and 16](../design/DESIGN_POLICY.md).
It owns invocation and order, not the provider's motion/format algorithm.

The physics orchestration direction supplied on 2026-10-20 assigns the runtime
sole responsibility for the evaluation DAG, shared-world barriers and physics
tick coordination. The target scheduling rules are in
[section 5](#5-shared-world-physics-orchestration); clock and invalidation rules
are owned by the [frame lifecycle](../architecture/FRAME_LIFECYCLE.md#5-physics-clock-and-discontinuities).
These refine the earlier phase sketches without freezing new ABI values.

[`evaluator.h`](../../include/avatarRuntime/evaluator.h) and the direct runtime
implement experimental registration, phase barriers and explicit predecessor
dependencies. This does not freeze RT-O3: actual motion/VRM/MMD adapter plans
still need conformance evidence.

The [near-term direction](../design/NEAR_TERM_PLAN.md#5-validate-evaluator-dependencies)
requires real-provider validation during overlapping Runtime Phases A/B:

```text
VRM: motion pose -> humanoid resolution -> LookAt -> expression arbitration
                                                    -> final state
MMD: motion / retarget -> morph / append / IK -> bone-to-body update
     -> shared physics barrier -> body-to-bone feedback -> final state
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

## 5. Shared-world physics orchestration

The optional experimental [`physics.h`](../../include/avatarRuntime/physics.h)
table implements the serial shared-world barrier and fixed clock without
changing the revision-4 core table or adding phase constants. The optional
[MMD adapter](../architecture/MMD_ADAPTER.md) supplies owner continuation/coupling
callbacks with generated Jolt host evidence. Broader provider ordering and
host/renderer composition remain integration gates.
Do not step a managed world inside an individual avatar evaluator.

```text
host frame request
    -> motion sample / retarget
    -> MMD morph / append / IK in owner-defined order
    -> pre-physics pose ready for ALL participating avatars
    -> MMD bone-to-body kinematic updates / VRM pre-state
    -> SHARED PHYSICS BARRIER: one Step per world per physics tick
    -> MMD body-to-bone feedback / VRM secondary motion
    -> final pose + morph + visibility + material state
    -> immutable EvaluatedAvatarState
    -> Hydra adapter or hydra-toon fast-path consumer
```

The diagram is the initial MMD integration path with a VRM extension point;
it does not require every VRM secondary-motion evaluator to use a rigid-body
world. SpringBone computations remain with the VRM owner.

For every due fixed tick, the runtime must:

1. Complete pre-physics evaluation for every avatar participating in the world,
   sampling motion at the tick's evaluation time rather than advancing each
   avatar independently at display cadence.
2. Submit all required kinematic targets through the format/physics adapters.
3. Invoke exactly one fixed `Step` for each participating world at that tick.
4. Collect physics results and complete all participating avatars' post-physics
   feedback before final consolidation and publication.

The runtime owns the DAG, dependency validation, barrier membership, adapter
lifetime and final-state handoff. The physics adapter calls the physics owner's
world/Step/snapshot boundary; the MMD adapter calls owner bone/body coupling.
Core does not implement PMX modes, body/bone mathematics, UsdPhysics import,
Jolt solving or VRM SpringBone algorithms. Provider handles stay behind adapters.

First validate whether existing evaluator dependencies can express owner
pre/post-physics requirements, including model-dependent post-physics IK.
Keep MMD ordering in the MMD evaluator's dependency graph. Do not encode model
semantics in new generic phase enum values before that validation (RT-O3).
The current prohibition on backward phase dependencies still applies; any
required scheduler extension needs explicit contract review and evidence.

`usd-stage-runner` owns its host loop, input collection and time requests;
it calls this runtime and must not duplicate its internal evaluation sequence
or independently step an avatar-managed world. Other hosts, including Mimikuri,
must be able to use the same runtime without stage-runner. Renderer consumers
receive only final snapshots and never execute Physics Step or IK.

### Experimental world binding

`arGetPhysicsApi(AR_PHYSICS_API_VERSION, ...)` supplies `create_world`,
`advance_world`, `reset_world` and `destroy_world`. Each descriptor uses the
core `AR_HEADER` convention. The runtime copies the world ID and participant
plan boundaries; callback code and `user_data` remain borrowed until unbinding
or runtime destruction. The caller supplies an already initialized solver and
gives this binding exclusive stepping/reset authority. The Step callback may
also manage non-avatar objects; runtime never creates solver resources.

Each participant selects the first post-physics evaluator by ID in its existing
validated plan. The prefix runs before Step and the entire suffix after Step;
the suffix may begin in the same phase as the prefix when explicit dependencies
order those evaluators. No backward phase edge is admitted. MMD owners must
split actual control calls at a valid owner boundary; this scheduler does not
split `mmdControl::Evaluator::Evaluate` or reinterpret after-physics flags.

Membership is immutable for a world binding and exclusive per instance.
Instances must be freshly created or reset before binding. Runtime orders
participants by instance handle, independently of descriptor/input array order.
All prefixes complete before one Step, then all suffixes complete before any
participant's provider state or prior snapshot commits. Each participant keeps
its existing transactional callbacks; abort reverses its own begin order.
Only complete ticks commit, and snapshots contain the existing dense final
state with tick identity/time. Step callbacks receive the fixed duration and
evaluation time; provider diagnostics keep their owner identity through the
runtime forwarding sink.

Managed instances reject direct `evaluate_frame`, `reset_instance` and
`destroy_instance`. All calls remain serialized and callback reentry returns
`AR_BUSY`. Hosts and external world users must not independently step the same
solver; the runtime cannot detect an out-of-band backend call.

For membership changes, stop requests, destroy the world binding, detach the
relevant external bodies/adapters, reset the remaining instances and bind the
new group to an initialized world. Unbinding requires instance reset before
direct evaluation resumes; retained snapshots survive. Dynamic per-avatar
unload without group reinitialization remains open. The
[lifecycle](../architecture/FRAME_LIFECYCLE.md#5-physics-clock-and-discontinuities)
owns clock acceptance and post-Step recovery. RT-O3/RT-O5/RT-O6 remain open for
real owner adapters, checkpoint recovery and broader lifecycle evidence.
