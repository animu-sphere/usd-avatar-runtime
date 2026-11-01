# Roadmap

Adopted 2026-11-01 from v0.2.0, and rescoped after v0.3.0. This page holds incomplete work
only: what comes next, in what order and how it is accepted. What exists is in the
[capability matrix](../reference/CAPABILITY_MATRIX.md). Version assignments are
targets; recheck them against the code and released owners when work starts.

The next release is about **showing that the runtime is not VRM-shaped**: an MMD session
ships as a package and runs from it, as the VRM session did in v0.3.0.

## Priorities

Order: MMD session package -> Hydra/direct agreement -> external input and bindings. The
MMD session comes first because its boundary is designed and only an owner release stands
in its way, while Hydra publication still needs an agreed integration with its consumer.

| Priority | Work | Starting point | Accepted when |
| --- | --- | --- | --- |
| P0 | [MMD session package](#v040) | source-only session; [package](../architecture/MMD_SESSION.md#package-boundary) checked on Windows against the owners' pushed digests, with a generated PMX and VMD | the v0.4.0 definition of done is reviewed against its evidence |
| P1 | [Hydra/direct agreement](#renderer-paths) | a [state comparison](../architecture/OUTPUT_PATHS.md#4-parity-evidence) that no consumer feeds yet; no Hydra publication | the same state gives agreeing pose and expression values within tolerance |
| P1 | Diagnosability | bounded diagnostics, an opt-in parity host | trace, capability dump and deterministic replay locate the phase, failure and reproduction conditions |
| P2 | [Connector path](#input-and-recording) | not started | external input reaches `AvatarInputFrame` with latency, loss and timestamps explicit |
| P2 | [Recording and publication](#input-and-recording) | not started | frame sequences reproduce and remain compatible |
| P2 | [Python, JavaScript and Web](#bindings-and-web) | not started | begins only after the native ABI is stable |

## v0.4.0

**Goal:** a real PMX and VMD load, play, reset and feed physics back from an installed
package, compared with the owner, with no format-specific change to core.

### Definition of done

- [x] `usd-mmd-plugins` 0.3.1 publishes `mmdPhysicsAdapter` and its closure.
- [ ] `usd-physics-plugins` 0.1.1 publishes `physicsJoltSixDof`. `physicsJolt` has no six-DOF, so it
  refuses any model with an admitted joint. Its draft is published after the session lanes pass on its
  digests.
- [ ] `avatarMmdSession` builds, tests and packages from those pins on Windows and Linux, and its
  installed consumer runs from the package with the Jolt SDK the
  [package boundary](../architecture/MMD_SESSION.md#package-boundary) requires.
- [ ] `avatarMmdSessionCheck` compares the session with the owner and writes state in the
  `--state-output` format, which gains primary vertex UVs. It does not compare material values with
  the owner, and reports how many it left out.
- [ ] A generated PMX and VMD in the session lanes show morph, IK, append and physics feedback each
  changing committed state, and each failing the comparison when broken.
- [x] Jolt agreement across platforms is [measured](../reports/evidence-log.md#mmd-session-package-2026-11-01):
  this fixture's state has equal values on the deterministic SDK, so there is one expected file.
- [ ] A real PMX and VMD run locally from the package, and `toon-viewport-mmd` draws them from it.
- [ ] [RT-O3 and RT-O6](#open-decisions) are resolved or deferred with a stated reason.
- [ ] The capability matrix states the supported subset. Full MMD compatibility is not claimed.
- [ ] The VRM package and core-only consumers keep working, and choosing between packages is enforced.
- [ ] `hydra-toon`'s VRM host consumes the published `avatarVrmSession` by its pin. The v0.3.0 check
  used the dry run's package, and `hydra-toon` does not pin the published one yet.

**Out of scope:** Hydra publication, several avatars in one world, seek, both formats in one
process, and an ABI freeze.

## After v0.4.0

### VRM node constraints

[Deferred past v0.3.0](../architecture/VRM_SESSION.md#node-constraints): the owner has no
evaluator to integrate. Accepted when the owner publishes one, the session selects it
explicitly, its phase order against motion, LookAt and spring is agreed with the owner, and
real-avatar results match the owner within tolerance. No substitute solver is written here or
in a renderer.

### MMD session

After the [package](#v040): complete non-bone morph output, including additional UV channels;
resident UV mapping and coherent tangents; representative private-asset, spring and contact
checks; and a world/non-MMD endpoint policy for joints.

### Renderer paths

Hydra and direct consume the same evaluated result. Compare at least transforms, morph
weights, material overrides and lifetime/reset numerically, and make unsupported
channels explicit. `scripts/compare_avatar_state.py` does this for files in the state format, so
each consumer first has to write what it received in that format. Exact pixel equality is not expected across shaders and rendering;
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
| RT-O9 | direct-consumer binding and update costs, later Hydra locators and dirtiness, parity tolerances | [output paths](../architecture/OUTPUT_PATHS.md) | Hydra agreement |
