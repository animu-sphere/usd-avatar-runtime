# Current implementation roadmap

Runtime Phases A–F remain open. The adopted
[near-term direction](../design/NEAR_TERM_PLAN.md) refines the original phase
sequence: A/B overlap, early direct consumer work from C validates the state,
and full Hydra publication follows real-provider evidence. No release names,
dates or package pins have been assigned.
Related provider work is tracked by its owner; this page tracks runtime
integration and the evidence needed here.

The adopted 2026-10-06 [boundary cleanup policy](../design/BOUNDARY_POLICY.md)
adds ownership cleanup ahead of further integration. Current APIs and existing
evidence remain in place until owner replacements and runtime conformance are
validated; this roadmap does not claim the migrations are complete.

## Boundary cleanup workstreams

These preserve the supplied cleanup sequence A–F, separately from the original
Runtime Phases A–F and evidence milestones A/B/C. Start with motion and
USD/motion ownership cleanup, then validate state with VRM and MMD, complete
publication and stabilize the ABI. Early fast-path work still supplies feedback
before freeze; owner API work stays in its owning repository.

| Workstream | Remaining work | Acceptance gate | Original Runtime Phase |
| --- | --- | --- | --- |
| B: USD/motion ownership | move StageClip motion-domain preparation to motionUsd; split SkeletonBinding into owner skeleton/rest conversion and runtime binding; delete duplicate conversion | owner supplies clip/source-rest and skeleton results with diagnostics; runtime retains avatar/layout/skeleton/joint identity, baseline and root placement; existing stage/copy lifetime, units, rejection and parity behavior remains verified | A/B |
| C: state validation | complete real-motion VRM LookAt/Expression intake, real material/morph evidence, retained snapshots and fast-path parity | actual input rather than host probes drives real bindings; owner results, retained state and fast-path values agree; layout/value changes and snapshot lifetime pass | A/B and early C |
| D: MMD second provider | register owner MMD evaluator; validate bone/morph/control dependencies and shared pre/post-physics handoff | real MMD frames use the same scheduler/state/lifecycle without core format branches or duplicated IK/control; physics boundary order is explicit | A/B |
| E: publication | finish fast-path and Hydra adapters, recording publication adapter and inspect/replay hosts | Hydra/direct consume identical resolved values and identity; recording delegates formats/resampling/compression/clip construction to motion owner; replay host composes owner algorithms; no renderer resource logic in runtime | C/E |
| F: ABI stabilization | review C/provider/consumer ABI revisions and capability versions; broaden installed-consumer and OST composition validation | real VRM/MMD and consumer evidence supports freeze; dependency-free core installation/import still passes; actual package composition is reproducible | A with C/E and packaging evidence |

Completion requires a provider/OpenUSD/renderer-independent core, no generic
motion algorithms or motionUsd domain logic here, one shared VRM/MMD scheduler
and state, identical Hydra/fast-path source state, recording through adapters,
and preserved owner diagnostic identity/version/subject. Adapter validation
tests cover invocation/mapping/lifetime; generic numerical correctness tests
remain with the owner. Code changes must update the capability matrix with
evidence rather than marking work complete from this documentation alone.

Workstream A's scoped motion registration/input bridge gate is validated:
generic clip/rig and selected-observation checks call installed owner APIs,
runtime binding checks remain, and `MotionPoseInputBridge` supplies compatible
`InputAssembler` source aliases. The
[capability matrix](../reference/CAPABILITY_MATRIX.md) records report forwarding,
owned version/lifetime and installed-consumer evidence. The remaining
workstreams above and the retained owner-version C ABI review under F remain
open; this does not close Runtime Phase A or an evidence milestone.

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
A scoped first [VRM registration adapter](../architecture/VRM_ADAPTER.md)
now executes owner LookAt -> Expression using test input and
constructed bindings. It validates bone-type eye pose and expression-type
morph/material effects and the retained snapshot boundary;
complete real-avatar LookAt/Expression integration remains step 2/3
work, and the evidence milestones below are still open.
World/joint-local points and directions now reach both LookAt types, including
ordered working-pose transforms and the owner's additive direction entry point.
This is constructed-binding evidence, not actual connector/asset mapping evidence.
The optional [motion clip pose adapter](../architecture/MOTION_ADAPTER.md) now
samples and retargets immutable owner clips into dense rig pose, with explicit
clock/joint/parent bindings and constructed motion -> VRM LookAt/Expression
numeric parity. Complete avatar binding and
connectors remain step 2/3 work; this does not close
milestone A.
Host-side motion input assembly now explicitly maps selected owner scalar
channels and world gaze points into owned `AvatarInputFrame` arrays. Constructed
motion -> assembled input -> VRM composition validates clocks, absence/zero,
LookAt precedence and host-selected stale gaze; real capture/avatar bindings,
connector intake, multi-source selection and retained provenance remain open.
A USD binding adapter builds instance configuration for avatar root,
skeleton/joint mapping, format identity, expression bindings, LookAt
configuration and material/deformation targets. Integration diagnostics must
identify missing joints, unsupported expressions, invalid gaze spaces, stale
input, capability/layout mismatch and unsupported outputs with provider
provenance. These are adapter/state tasks, not renderer semantics.
The optional [scoped USD skeleton binding](../architecture/USD_BINDING.md)
now reads authored joint/rest layout, converts translations to metres and
provides explicit owner Humanoid mappings and rigid skeleton placement.
Constructed USD -> motion -> VRM bone LookAt parity and installed-consumer
ownership checks are implemented. The separate
[VRM USD Humanoid binding](../architecture/VRM_USD_BINDING.md) now discovers
the applied owner schema, resolves its skeleton relationship and maps standard
roles without joint-name inference. One private avatar validates 128 joints,
51 roles and constructed motion-to-state parity; affine matrix roundoff from
that asset is covered by generic binder regression tests. Expression and
material/deformation discovery now have the scoped evidence below; this
does not establish complete real-avatar evaluation evidence.

The optional [USD motion clip binding](../architecture/MOTION_USD_BINDING.md)
now connects owner-imported semantic clips and source rest to motion evaluation.
An opt-in parity tool validates seven real VRMA clips on the same private
avatar: all target joint TRS, source/runtime clock mapping, boundary holds,
reset and retained snapshots. The target's missing `upperChest` mapping is
reported by the owner. This closes the scoped real-clip-to-Humanoid pose gap;
native VRMA expression/gaze intake, connectors and rendering
remain open, and milestone A is not yet complete.

The optional [VRM USD LookAt binding](../architecture/VRM_LOOKAT_USD_BINDING.md)
now extracts both owner rig types, raw range maps and explicit head/eye/rest
configuration. Constructed USD tests cover owner quaternion/weight parity,
stage lifetime and installed consumers. The same private avatar's
Expression-type LookAt has test-gaze owner-weight parity with constructed
output sinks. Real-avatar bone-eye
conformance and native VRMA gaze intake remain step 2/3 work; host-test gaze
against a real motion pose has the scoped evidence below.

The optional [VRM USD Expression binding](../architecture/VRM_EXPRESSION_USD_BINDING.md)
now extracts owner definitions, mesh blend-shape identities and canonical material
baseline inputs. Constructed USD tests cover all six colour slots and indexed
binds; the same private avatar supplies 18 expressions and 48 morph slots with
test scalar/gaze -> actual LookAt/Expression -> retained state owner parity.
The opt-in `avatarMotionCheck --vrm` now checks those actual bindings while
seven real clips drive the working pose, with explicit host scalar/world-gaze
probes and separate owner pose/head/LookAt/Expression comparisons. Zero/absence,
stale holds, reset and active/held snapshot retention pass. This closes the
scoped real-motion-pose + host-test-input composition gap.

The installed common motion reader does not convert the VRMA owner's native
expression/gaze attributes; all seven clips report zero reader-provided fields.
Resolve native expression/gaze intake and coordinate mapping with the owners,
then validate captured/clip input without probes. Material binds on real avatars,
additional rigs/shared-mesh targets, connector intake and renderer consumption
remain open; milestone A is not closed by probe evidence.
The shared motion USD gaze boundary and VRMA owner handoff are tracked in
[usd-motion-plugins issue #37](https://github.com/animu-sphere/usd-motion-plugins/issues/37).

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

- Validate the scoped owner clip-to-pose and channel/gaze input assembly with
  real bindings; extend explicit multi-source selection and connector intake;
  validate the revision-3 gaze spaces and
  observation validity against actual connector/motion/VRM mappings;
  establish semantic intent/mapping precedence using real bindings (RT-O1/RT-O2).
- Validate actual motion/VRM/MMD adapters against phase dependencies, particularly
  atomic MMD control and full motion-to-VRM flow (RT-O3). The optional VRM
  adapter's atomic expression-driven gaze-to-expression sequence is tested
  with the real owner library, constructed rigs and one actual avatar's
  expression/morph bindings with test scalar/gaze input; complete real-motion
  composition remains unvalidated. Bone-type LookAt now has constructed-rig
  pose parity and ordered working-head evidence with explicit eye/rest bindings;
  actual avatar joint/rest conformance remains open. Owner motion clip sampling
  and retargeting now precede VRM in constructed-rig tests; this is not real
  clip/LookAt/Expression asset evidence. Schema-derived Humanoid mapping and
  constructed motion-to-state parity now have one private-avatar result.
- Extend scoped USD skeleton/rest/placement and owner schema/Humanoid
  conformance beyond the single local avatar; validate expression/output
  discovery on additional rigs, including material binds and shared-mesh targets.
  The generic binder still uses explicit role
  mappings, with the optional VRM schema adapter supplying them (RT-O4).
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

First complete the motion and USD/motion ownership cleanup above. Runtime
input assembly remains a bridge; generic skeleton conversion and motion
invariants do not become runtime responsibilities merely because existing
optional adapters currently validate them locally.

Begin this work during Phase A. VRM Humanoid/LookAt/Expression is the first
family; motion completes that slice, then MMD challenges scheduler generality.

The optional adapters implement owner clip sampling/retarget-to-pose and
expression effects/both LookAt types through installed owner libraries.
Constructed motion-to-VRM composition is tested, including owned USD skeleton
baseline and rigid placement from explicit bindings. Remaining VRM work includes
additional expression/material asset conformance, native VRMA expression/gaze
intake beyond host-probe composition, real-avatar eye/rest conformance,
actual connector gaze mapping and typed resolved expression/gaze state.
Do not count constructed rigs as the representative real-avatar evidence required here.

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
