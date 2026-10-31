# Roadmap

Adopted 2026-11-01 from v0.2.0. This page holds incomplete work only: what comes
next, in what order and how it is accepted. What exists is in the
[capability matrix](../reference/CAPABILITY_MATRIX.md). Version assignments are
targets; recheck them against the code and released owners when work starts.

The next release is about **completing the VRM session and connecting it to a
renderer**, not about adding evaluators. A later MMD session then shows that nothing
VRM-specific leaked into the runtime.

## Priorities

Order: VRM session integration -> direct rendering end to end -> Hydra/direct agreement
-> MMD session -> external input and bindings.

| Priority | Work | Starting point | Accepted when |
| --- | --- | --- | --- |
| P0 | [VRM session integration](#b-vrm-end-to-end) | the session selects pose, spring, expression and LookAt, with expression and gaze from the host or the clip; CI has only generated fixtures | a real VRM goes from input through evaluation to a committed snapshot with pose, expression, LookAt and spring |
| P0 | [`hydra-toon` direct consumer](#c-renderer-consumer) | `toon-viewport-vrm` builds from the package; the resident check is headless and pose-only | one real VRM renders, updates, resets and tears down soundly |
| P1 | [Hydra/direct agreement](#renderer-paths) | no Hydra publication | the same state gives agreeing pose and expression values within tolerance |
| P1 | [MMD session trial package](#mmd-session) | source-only session | a real PMX/VMD loads, plays, resets and feeds physics back from a package |
| P1 | [Package design](#a-package-quality) | two packages provide the same CMake package and core files | choosing core or session is clear and duplicate installs cannot collide |
| P1 | Diagnosability | bounded diagnostics, an opt-in parity host | trace, capability dump and deterministic replay locate the phase, failure and reproduction conditions |
| P2 | [Connector path](#input-and-recording) | not started | external input reaches `AvatarInputFrame` with latency, loss and timestamps explicit |
| P2 | [Recording and publication](#input-and-recording) | not started | frame sequences reproduce and remain compatible |
| P2 | [Python, JavaScript and Web](#bindings-and-web) | not started | begins only after the native ABI is stable |

## v0.3.0

**Goal:** a real VRM goes into one session, motion, LookAt, expression and spring
evaluate there, and the committed state reaches at least one independent renderer
consumer and appears on screen.

### Definition of done

- [ ] The published session builds and runs from a clean install tree.
- [ ] Windows and Linux CI repeat the same end-to-end test.
- [ ] With a real VRM and motion, pose, LookAt, expression and spring appear in committed state.
- [ ] A renderer consumer receives the values, and drawing and post-reset display have no serious breakage.
- [ ] Owner semantics and session values are compared automatically.
- [ ] Public documents state supported and unsupported capabilities and their limits.
- [ ] Existing core-only consumers keep working.

**Out of scope:** format-specific solvers in core, every rendering engine, a permanent
ABI freeze and bindings for several languages at once.

### A. Package quality

- [ ] Run `verify-consumer` for the session on Linux. `ost` 0.23.17 allows host X11 only for a runtime that declares it, and the pinned runtime declares nothing ([open-strata#289](https://github.com/animu-sphere/open-strata/issues/289), [measurement](../reports/evidence-log.md#ost-02317-and-the-session-consumer-on-linux-2026-11-01)). This needs either an `ost` change or a runtime that declares both packages, with the owners rebuilt against it. Keep the repository's consumer step until then.
- [ ] File the package and target collisions as issues; decide targets and components.

### B. VRM end to end

- [ ] Decide what happens to node constraints: integrate them, or defer them explicitly.
- [ ] Add a redistributable small VRM, motion and expected state to CI. Private assets stay as additional evidence.
- [ ] Test import failure, evaluator failure, reset, pause/resume, destroy/recreate, snapshot retention and several avatars.

### C. Renderer consumer

- [ ] Settle the `hydra-toon` direct input contract, with no evaluation logic reimplemented there.
- [ ] Render one real VRM and confirm motion and expression changes.
- [ ] Build the state comparison that Hydra agreement will use.

### D. Release decision

- [ ] Review the definition of done.
- [ ] Design the MMD session's package boundary and fixtures.
- [ ] Classify ABI change candidates, and decide whether to keep deferring the freeze.

## After v0.3.0

### MMD session

Prove the runtime is not VRM-shaped through outside use of an MMD session: the minimal
vertical path through PMX/VMD, morph, IK, append, physics feedback, shared world and
reset. Publish a supported subset with fixtures and the capability matrix; do not claim
full MMD compatibility. The owner library must be published first.

Also open for this track: complete non-bone morph output, including additional UV
channels; resident UV mapping and coherent tangents; representative private-asset,
spring and contact checks; and a world/non-MMD endpoint policy for joints.

### Renderer paths

Hydra and direct consume the same evaluated result. Compare at least transforms, morph
weights, material overrides and lifetime/reset numerically, and make unsupported
channels explicit. Exact pixel equality is not expected across shaders and rendering;
it needs its own criterion if it is ever required. Also measure consumer latency, copies
and allocations.

### Input and recording

Connect connector input, recording, replay and remote transport in adapters and hosts,
without growing core. Do not carry arrival order or loss implicitly into the runtime:
state timestamps and interpolation policy. Representative captured expression and gaze,
multi-source selection and automatic native attribute discovery belong here. See
[recording and replay](../design/RECORDING_AND_REPLAY.md).

### Bindings and Web

Develop Python and JavaScript/Wasm after the ABI and session use cases settle. The host
owns the browser UI and draw loop. Measure Wasm size, load time and ABI differences.
Native support does not establish browser support, and device adapters and WebGPU
realization stay with their owners.

### Execution and scheduling

OpenExec wrappers, inspect/replay/benchmark hosts and parallel scheduling follow stable
evaluator cores. Direct evaluation must stay functional with OpenExec disabled and give
equivalent state and diagnostics through both. Parallelism needs explicit state
dependencies and shared-resource lifetime evidence.

## Physics orchestration milestones

Remaining gates for runtime-coordinated physics, which the MMD session needs. Rules are
in the [evaluator contract](../contracts/EVALUATOR.md#5-shared-world-physics-orchestration)
and [lifecycle](../architecture/FRAME_LIFECYCLE.md#5-physics-clock-and-discontinuities).

| Gate | Remaining work |
| --- | --- |
| Evaluator dependencies | validate DAGs against MMD pre/post control, morph, append, IK and model-specific post-physics IK before adding any generic phase value |
| World integration | connect the session to application and stage lifecycle with automatic world and participant binding; selective unload with cross-avatar constraints |
| Final-state consumers | Hydra and direct consume the same final joint and morph values, layout and spaces; no Step or IK in rendering |
| Several avatars and seek | two avatars in one world with exactly one Step per tick for zero, one or several ticks per display request; coherent pause, reset, loop-wrap and unload; physics-disabled static and motion-only paths stay valid |

Seek must declare invalidation or a supported checkpoint, not promise complete replay to
any time. Core stays free of mandatory MMD, Jolt and renderer packages. Normal CI covers
core units and headless smoke; heavy real-asset, backend and render checks stay opt-in.

## VRM secondary-motion integration

Spring runs in the session through the owner solver, under the
[owner's policy](https://github.com/animu-sphere/usd-vrm-plugins/blob/main/docs/design/VRM_MOTION_POLICY.md#10-spring-bones-on-a-rig).
Do not write a substitute solver here or in a renderer. Open: scaled spring rigs, changing
spring mode without rebuilding session and consumer bindings, composition with the
shared-world barrier, resident rendering and Hydra/direct agreement. Reset, seek and
loop-wrap initialize from the declared animated pose; a time jump is never passed as one
large or negative step.

## ABI freeze

The ABI stays experimental until all of these hold, and the decision is revisited at each
release. Versions, struct sizes and capabilities carry extension meanwhile, with old
consumers still checked.

- Two formats and two kinds of external consumer use the same scheduler and state.
- Install-tree CI, failure recovery and public examples exist.
- Clock and space mappings, channel identity, absence versus zero, final rig
  representation and snapshot rules are specified: ownership, buffer lifetime, retention,
  absent channels and multi-instance identity.
- Plan order is validated against motion, VRM and MMD dependencies.
- Provider checkpoint restore is implemented, or explicitly deferred with a reason.
- Compatibility is checked with separately versioned real providers across toolchains.
- Capability negotiation makes unsupported channels observable, without forcing
  format-specific elements on another format.
- Core ABI, session API, renderer transport and owner adapters each have a stated
  compatibility policy.
- The [open decisions](#open-decisions) are resolved or explicitly deferred. ABI and
  phase-order questions cannot be deferred past the freeze.

Synthetic tests alone cannot freeze a contract.

## Quality gates

| Gate | Must be checked |
| --- | --- |
| Core | ABI revision, struct sizes, evaluator order, read/write domain conflicts, transactions and snapshot retention |
| Session | clean-install consumer, dependency closure, package metadata, capability negotiation |
| VRM | real-asset pose, expression, LookAt and spring against the owner; reset and replay |
| MMD | morph, IK, physics feedback and shared world against the owner; reset and replay |
| Rendering | same state delivered, resource lifetime, transform and expression updates, device-reset equivalent |
| Cross-platform | Windows and Linux build and run, dependency agreement, reproducible packages |
| Negative | bad input, evaluation failure, partial failure, destroy/recreate, interference between avatars |

Record results separately as `unit`, `adapter integration`, `installed-package consumer`
and `real-asset end-to-end`, and do not state success more broadly than the category.
Record actual assets with usable rights, provider versions, configuration and tolerances.
A successful C call does not show that a provider is correct.

## Risks

| Risk | Response |
| --- | --- |
| The session grows format-specific | keep session composition apart from owner evaluators; no format algorithms in core |
| Adapters and the shipped session drift apart | the matrix tracks owner, session and renderer state per feature |
| Hydra and direct diverge in meaning | committed state is the single truth; only transport differs |
| Partial physics failure leaves bad state | invalidation and reset contract, world ownership, shared-step tests |
| CMake packages collide | namespaces, components and install-tree tests |
| The ABI freezes early | stay experimental until the gates above hold |
| Tests lean on private assets | more redistributable fixtures and golden state comparisons |

## Measures of success

- **Adoption:** a short, repeatable path from installed packages to a drawn avatar.
- **Correctness:** owner values and session values agree within declared tolerances.
- **Independence:** VRM and MMD owners and several consumers work with no core change.
- **Recovery:** no regressions in reset, replay, evaluator failure, destroy/recreate or several avatars.
- **Sustainability:** package CI and the matrix explain what each release can really do.

The lasting value is not absorbing VRM or MMD. It is letting each keep its own meaning
while composing safely under a common time, state and evaluation model.

## Open decisions

IDs are stable. Resolving one updates the owning contract and records the evidence.
These are runtime design questions, not reported upstream bugs.

| ID | Decision | Owner here | Needed by |
| --- | --- | --- | --- |
| RT-O1 | evaluation seconds, source clock, USD time-code mapping and gaze space descriptors | [input](../contracts/INPUT_FRAME.md) | ABI freeze |
| RT-O2 | semantic intent vocabulary, custom and native channel names, mapping precedence; resolved expression identity, arbitration, provenance and availability. The session publishes effects only, under the [consumer contract](../contracts/EVALUATED_STATE.md#4-consumer-contract) | [input](../contracts/INPUT_FRAME.md), [state](../contracts/EVALUATED_STATE.md) | ABI freeze |
| RT-O3 | whether the DAG fits MMD morph, append, control and IK, model-specific pre/post-physics dependencies, gaze and expressions, before any phase extension | [evaluator](../contracts/EVALUATOR.md) | MMD session |
| RT-O4 | typed gaze results, retained diagnostics and snapshot provenance, and real-provider layout conformance. Pose, spaces, layout identity and complete snapshots are in the [consumer contract](../contracts/EVALUATED_STATE.md#4-consumer-contract) | [state](../contracts/EVALUATED_STATE.md) | ABI freeze |
| RT-O5 | reset, checkpoint, commit and abort for stateful evaluators; shared-world discontinuity scope, post-Step recovery, publication failure | [lifecycle](../architecture/FRAME_LIFECYCLE.md) | ABI freeze; world recovery for the MMD session |
| RT-O6 | world handles and callbacks, fixed-tick and catch-up policy, tick/display separation, shared participants and sibling alignment | [evaluator](../contracts/EVALUATOR.md#5-shared-world-physics-orchestration), [lifecycle](../architecture/FRAME_LIFECYCLE.md#5-physics-clock-and-discontinuities), [dependencies](../architecture/DEPENDENCIES.md#3-integration-questions) | MMD session |
| RT-O7 | C layout and calling convention, strings, allocator ownership, handles, provider lifetime and negotiation | [ABI](../contracts/ABI.md) | ABI freeze |
| RT-O8 | capture envelope, checkpoint encoding and reuse of motion recording | [recording and replay](../design/RECORDING_AND_REPLAY.md) | recording; lifetime prerequisites by ABI freeze |
| RT-O9 | direct-consumer binding and update costs, later Hydra locators and dirtiness, parity tolerances | [output paths](../architecture/OUTPUT_PATHS.md) | v0.3.0 for direct; Hydra agreement for the rest |
