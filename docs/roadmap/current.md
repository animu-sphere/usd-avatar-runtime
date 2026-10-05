# Current implementation roadmap

Runtime Phases A–F remain open. The adopted
[near-term direction](../design/NEAR_TERM_PLAN.md) refines the original phase
sequence: A/B overlap, early direct consumer work from C validates the state,
and full Hydra publication follows real-provider evidence. No release names,
dates or package pins have been assigned.
Related provider work is tracked by its owner; this page tracks runtime
integration and the evidence needed here.

## Near-term implementation sequence

The first slice is connector/test input -> `AvatarInputFrame` -> owner motion
evaluation -> VRM Humanoid -> LookAt -> Expression -> `EvaluatedAvatarState`
-> `hydra-toon` fast-path. Use direct serial execution; OpenExec, a Scene Index,
full physics, parallel scheduling, Web/WASM/XR packaging, comprehensive replay
tooling and a stable frozen ABI are not prerequisites.

| Step | Work | Acceptance evidence | Runtime Phase |
| --- | --- | --- | --- |
| 1 | contract hardening | settle pose representation, gaze space/validity and expression identity/arbitration; validate snapshot/layout identity with real adapters; identify real-provider gaps (RT-O1–RT-O4/RT-O7) | A with B feedback |
| 2 | VRM adapter | callable owner evaluators, Humanoid binding, LookAt and Expression work with a real VRM avatar; test input may bring up the adapter | A/B |
| 3 | motion integration | map owner motion values without a competing vocabulary; connect connector input, actor/time mappings and motion evaluation to VRM | A/B |
| 4 | `hydra-toon` fast-path | consume retained snapshots for pose/morph/expression/appearance/visibility; distinguish layout change from value update; measure latency, copies and allocations | early C feeding A/B |
| 5 | MMD adapter | real bone morph/control/IK evidence under the same model; validate atomic owner ordering and the physics boundary without requiring full physics | A/B |
| 6 | Phase A freeze candidate | review C ABI revision, phases, state, capabilities and lifecycle using separately versioned VRM/MMD provider evidence and all Phase A gates | A |
| 7 | Hydra publication | Scene Index/overlay adapter over fast-path-validated semantics and shared frame/state identity; resolved output parity | C |

Registration adapters must not expose provider-private types in core ABI.
A USD binding adapter builds instance configuration for avatar root,
skeleton/joint mapping, format identity, expression bindings, LookAt
configuration and material/deformation targets. Integration diagnostics must
identify missing joints, unsupported expressions, invalid gaze spaces, stale
input, capability/layout mismatch and unsupported outputs with provider
provenance. These are adapter/state tasks, not renderer semantics.

## Evidence milestones

These milestones describe end-to-end evidence, not completion of the similarly
lettered Runtime Phases:

| Milestone | Required evidence |
| --- | --- |
| A | real VRM avatar + real motion input -> Humanoid/LookAt/Expression -> `EvaluatedAvatarState` |
| B | `hydra-toon` fast-path consumes the same state and renders in real time |
| C | real MMD evaluator works on the same scheduler and state contract |

Successful integration keeps format semantics out of core, GPU realization in
the renderer and input acquisition in connectors. It requires no per-frame
stage authoring, renderer callbacks into evaluators or format branches in
runtime core/renderer. Future Hydra and direct outputs share evaluated values
and identity; freeze is based on real-provider evidence.

## Validation tiers

| Tier | Required coverage | Gate |
| --- | --- | --- |
| 1: core contract | ABI size/revision, lifetime, rollback, reset, instance isolation, capability negotiation and dependency validation; preserve synthetic tests | A |
| 2: real provider | motion -> runtime; VRM Humanoid/LookAt/Expression; MMD morph/control/IK and real bindings | A/B before freeze |
| 3: end-to-end | input -> evaluator -> retained state -> `hydra-toon` fast-path; numeric pose/expression/gaze parity and update/lifetime behavior | milestones A/B; early C feedback |
| 4: output parity | fast-path versus Hydra resolved state, identities and negotiated channels | C after fast-path validation |

Pixel parity is separate renderer evidence. Record actual assets (with usable
rights), provider versions, configuration, tolerances and diagnostics for each
integration result. Do not infer provider correctness from a successful C call.

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
- Real VRM and MMD adapters on the same scheduler/state model, plus owner
  motion input and binding evidence. Synthetic tests alone cannot freeze ABI,
  phase order, state representation or capability meanings.
- Resolution or explicitly scoped deferral of the decisions below. Blocking
  ABI and phase-order questions cannot be deferred past freeze.

Documentation adoption alone does not satisfy this phase. Use contract design
-> synthetic tests -> real provider integration -> contract correction -> ABI
freeze, rather than blocking provider integration on an already frozen ABI.

The first implementation supplies experimental C headers, direct serial
execution, typed gaze observations with explicit space/validity/clocks,
capability/plan validation, transactional provider state, immutable
snapshots, reset and separately compiled/installed C provider-consumer evidence.
These completed foundations are recorded in the
[capability matrix](../reference/CAPABILITY_MATRIX.md); they are not ABI freeze.

Remaining implementation/review work before Phase A acceptance:

- Marshal owner motion values and validate the revision-3 gaze spaces and
  observation validity against actual connector/motion/VRM mappings;
  establish semantic intent/mapping precedence using real bindings (RT-O1/RT-O2).
- Validate actual motion/VRM/MMD adapters against phase dependencies, particularly
  atomic MMD control and gaze-to-expression flow (RT-O3).
- Prove rig/material/deformation layout conformance with those adapters;
  negotiate effects outside the current dense snapshot subset, including
  explicit expression/gaze results and retained provenance (RT-O4).
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

Begin this work during Phase A. VRM Humanoid/LookAt/Expression is the first
family; motion completes that slice, then MMD challenges scheduler generality.

Acceptance: representative motion-to-VRM and motion-to-MMD frames use one
runtime lifecycle; format-specific writes are consolidated once; connector
actors/time mappings and missing channels are diagnosed. Direct calls work
without a renderer/OpenExec dependency. Record actual package versions and
composition evidence when adopted.

## Runtime Phase C — output paths

Connect the retained direct snapshot API to `hydra-toon` first, using the
same resolved state as evaluation. This work starts before Phase A freeze
to validate state semantics and measure consumer cost. Implement the evaluation
Scene Index/overlay after the fast-path and real-provider evidence.

Acceptance: state parity for pose, deformation, appearance and visibility;
value-only dirtiness and structural rebinding checks; supported/unsupported
output negotiation; snapshot lifetime checks; no mandatory stage writeback.
Renderer pixels are separate evidence; measure fast-path latency, copy cost and
allocations without treating them as proof of resolved state parity.

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
| RT-O2 | minimal semantic intent vocabulary, custom/native channel names, mapping precedence and resolved expression identity/arbitration/provenance/availability | [input](../contracts/INPUT_FRAME.md), [state](../contracts/EVALUATED_STATE.md) | A |
| RT-O3 | phase/substep dependencies across MMD morph/control/IK, gaze and expressions | [evaluator](../contracts/EVALUATOR.md) | A |
| RT-O4 | final rig pose/spaces, target/layout/version identities, complete/sparse/delta semantics, gaze results and snapshot input revision/capabilities | [state](../contracts/EVALUATED_STATE.md) | A |
| RT-O5 | stateful evaluator reset/checkpoint/commit/abort and publication-failure handling | [lifecycle](../architecture/FRAME_LIFECYCLE.md) | A |
| RT-O6 | shared physics stepping, stage-runner ownership and format feedback boundary | [dependencies](../architecture/DEPENDENCIES.md) | before physics integration |
| RT-O7 | C layout/calling convention, strings, allocator ownership, handles, provider lifetime and ABI negotiation | [ABI](../contracts/ABI.md) | A |
| RT-O8 | capture envelope/checkpoint encoding and motion-recording reuse | [recording/replay](../design/RECORDING_AND_REPLAY.md) | E; lifetime prerequisites in A |
| RT-O9 | direct-consumer binding/update costs, later Hydra locators/dirtiness and output parity tolerances | [output paths](../architecture/OUTPUT_PATHS.md) | early C feedback in A/B; full parity in C |
