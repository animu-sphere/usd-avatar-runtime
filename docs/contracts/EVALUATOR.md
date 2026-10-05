---
status: proposed
owner: usd-avatar-runtime
---

# Evaluator contract and phase order

This proposal develops [design policy sections 7–8 and 16](../design/DESIGN_POLICY.md).
It owns invocation and order, not the provider's motion/format algorithm.

## 1. Initial phase sequence

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
