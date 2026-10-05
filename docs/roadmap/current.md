# Current implementation roadmap

Runtime Phases A–F remain open and preserve the supplied implementation
policy's ordering. No release names, dates or package pins have been assigned.
Related provider work is tracked by its owner; this page tracks runtime
integration and the evidence needed here.

## Runtime Phase A — freeze contracts

Define and review input frames, evaluated-state views, evaluator phases,
capabilities, diagnostics and the compact registration/execution ABI.
The [contract drafts](../README.md#contracts) are the starting point.

Acceptance requires:

- Explicit clock/space mappings, channel identity, absence/zero semantics,
  final rig representation and output snapshot/update rules.
- An ordered evaluator plan validated against motion, VRM and MMD dependencies,
  including MMD bone morph/IK ordering and expression-based LookAt arbitration.
- Defined input/output ownership, handle lifetime, errors, reset/restore and
  stateful commit/abort behavior.
- Version/size negotiation, capability meanings and diagnostic aggregation
  rules, with a compilable C boundary and separately built provider/consumer
  compatibility evidence.
- Minimal direct execution evidence independent of Hydra/OpenExec, with
  deterministic state from specified inputs/configuration/prior state.
- Resolution or explicitly scoped deferral of the decisions below. Blocking
  ABI and phase-order questions cannot be deferred past freeze.

Documentation adoption alone does not satisfy this phase.

The first implementation supplies experimental C headers, direct serial
execution, capability/plan validation, transactional provider state, immutable
snapshots, reset and separately compiled/installed C provider-consumer evidence.
These completed foundations are recorded in the
[capability matrix](../reference/CAPABILITY_MATRIX.md); they are not ABI freeze.

Remaining implementation/review work before Phase A acceptance:

- Marshal owner motion values and define gaze spaces and observation validity;
  establish semantic intent/mapping precedence using real bindings (RT-O1/RT-O2).
- Validate actual motion/VRM/MMD adapters against phase dependencies, particularly
  atomic MMD control and gaze-to-expression flow (RT-O3).
- Prove rig/material/deformation layout conformance with those adapters;
  negotiate effects outside the current dense snapshot subset (RT-O4).
- Add provider checkpoint restore and test discontinuities with real stateful
  evaluators; current reset/commit/abort are implemented (RT-O5).
- Review/freeze the experimental ABI and semantic capability meanings with
  separately versioned real providers. Current evidence is synthetic Windows
  x64/MSVC, not cross-toolchain or Web support (RT-O7).

## Runtime Phase B — integrate current repositories

Add adapters for generic motion evaluation, VRM LookAt/expressions, MMD
morph/control/IK evaluation and connector input assembly. Compose providers
through registration and explicit per-avatar bindings. Preserve owner libraries
and extend missing boundaries with their owners instead of copying algorithms.

Acceptance: representative motion-to-VRM and motion-to-MMD frames use one
runtime lifecycle; format-specific writes are consolidated once; connector
actors/time mappings and missing channels are diagnosed. Direct calls work
without a renderer/OpenExec dependency. Record actual package versions and
composition evidence when adopted.

## Runtime Phase C — output paths

Prototype the evaluation Scene Index and direct consumer API over the same
resolved snapshot. Agree an initial renderer adapter with its owner.

Acceptance: state parity for pose, deformation, appearance and visibility;
value-only dirtiness and structural rebinding checks; supported/unsupported
output negotiation; snapshot lifetime checks; no mandatory stage writeback.
Renderer pixels/latency are separately measured evidence.

## Runtime Phase D — OpenExec

Wrap stable cores in optional execution integration. Preserve the direct
fallback and consume existing motion/format nodes from their owners.

Acceptance: equivalent input/configuration/prior state gives equivalent state
and diagnostics through direct and OpenExec paths; driver invalidation/time
behavior follows the owner's execution contract. Disabling OpenExec leaves
direct frame evaluation functional.

## Runtime Phase E — runtime quality

Add deterministic capture/replay, inspection, phase/provider profiling,
structured negotiation and multi-avatar execution/scheduling. Capability and
error basics are already required in Phase A; this phase expands their tooling
and runtime coverage.

Acceptance: reproduce captured evaluation from initial state/checkpoints;
separate evaluation replay from output replay; demonstrate instance isolation;
exercise failure/reset/rebinding; measure instrumentation overhead. Parallelism
needs explicit state dependencies and shared-resource lifetime evidence.

## Runtime Phase F — Web / XR / application runtime

Develop a reduced runtime profile, WASM-friendly packaging, WebXR input
composition and a WebGPU-capable consumer integration.

Acceptance: actual target builds and runtime evidence, optional-dependency
selection, host-memory/lifetime checks and preserved contract semantics.
Device adapters stay with `motion-connectors`; WebGPU realization stays with
the renderer. Native support does not establish browser support.

## Open decisions

Owning documents now record scoped prototype choices for clocks/channels, dense
output, lifecycle, plan order and C layout. IDs below stay open until the full
acceptance evidence, including actual provider adapters, is available.

IDs remain stable; resolution updates the owning contract and records the
decision/evidence. These are runtime design questions, not claims of upstream
bugs or requests already sent to another project.

| ID | Decision | Owner here | Gate |
| --- | --- | --- | --- |
| RT-O1 | evaluation seconds, source clock, USD time-code mapping and gaze space descriptors | [input](../contracts/INPUT_FRAME.md) | A |
| RT-O2 | minimal semantic intent vocabulary, custom/native channel names and explicit mapping precedence | [input](../contracts/INPUT_FRAME.md) | A |
| RT-O3 | phase/substep dependencies across MMD morph/control/IK, gaze and expressions | [evaluator](../contracts/EVALUATOR.md) | A |
| RT-O4 | final rig pose representation, target identities and complete/sparse/delta output semantics | [state](../contracts/EVALUATED_STATE.md) | A |
| RT-O5 | stateful evaluator reset/checkpoint/commit/abort and publication-failure handling | [lifecycle](../architecture/FRAME_LIFECYCLE.md) | A |
| RT-O6 | shared physics stepping, stage-runner ownership and format feedback boundary | [dependencies](../architecture/DEPENDENCIES.md) | before physics integration |
| RT-O7 | C layout/calling convention, strings, allocator ownership, handles, provider lifetime and ABI negotiation | [ABI](../contracts/ABI.md) | A |
| RT-O8 | capture envelope/checkpoint encoding and motion-recording reuse | [recording/replay](../design/RECORDING_AND_REPLAY.md) | E; lifetime prerequisites in A |
| RT-O9 | Hydra locators/dirtiness, direct-consumer binding and output parity tolerances | [output paths](../architecture/OUTPUT_PATHS.md) | C; output semantics in A |
