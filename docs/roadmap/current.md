# Roadmap

Adopted 2026-11-03 after v0.4.0. This page holds incomplete work only: what comes next, in
what order and how it is accepted. What exists is in the
[capability matrix](../reference/CAPABILITY_MATRIX.md). Version assignments are targets, not
commitments: recheck them against the code and released owners when work starts, and do not
tie them to owner releases.

v0.4.0 runs two formats on one runtime, and the [VRM](../releases/v0.3.0.md) and
[MMD](../releases/v0.4.0.md) sessions ship as packages. The next value is not more
features. It is delivering one evaluated state, without contradiction, to several
consumers, several avatars and several environments.

## Releases

Order: renderer contract and parity -> diagnostics, replay and performance -> several avatars
in a shared world -> connector input and recording -> bindings, after the
[ABI freeze](#abi-freeze).

| Target | Theme | Release gate |
| --- | --- | --- |
| v0.5.0 | [renderer contract and parity](#renderer-contract-and-parity) | consumers write what they received, and Direct and Hydra agree numerically |
| v0.6.0 | [diagnostics, replay and performance](#diagnostics-replay-and-performance) | deterministic reproduction, phase tracing and a measured baseline |
| v0.7.0 | [several avatars and a shared world](#several-avatars-and-a-shared-world) | two avatars, one Step per tick, negative scenarios |
| later | [connector input and recording](#connector-input-and-recording), the ABI freeze review, [bindings](#bindings-and-web) | the gates below, in order |

v0.5.0's scope is a candidate. Small fixes may release in between.

## Rules for this plan

- **The committed snapshot is the single truth.** Consumers do not recompute IK, physics
  Step, LookAt resolution or morph semantics. Direct and Hydra never evaluate separately.
- **Clocks and lifecycle are explicit.** Evaluation, source, tick and display time are
  distinct, and so are reset, seek, loop wrap and discontinuity.
- **Nothing unsupported is dropped silently.** Absent, unsupported, invalid and zero stay
  distinguishable through [negotiation and diagnostics](../contracts/CAPABILITIES_AND_DIAGNOSTICS.md).
- **Hosts keep the choice.** Direct, Hydra and headless handle the same state through
  different paths.

Not goals: VRM or MMD algorithms in core; Jolt, OpenUSD, Hydra, a browser or a device as a
core dependency; pixel equality as the first criterion; freezing the ABI before its use
cases and different kinds of consumer are checked; claiming full MMD compatibility, Web or
XR support.

Generated fixtures in CI, local real-model runs, numeric comparison of what a renderer
received and pixel agreement are different evidence. Claim only the one that was measured,
under the [quality gates](#quality-gates).

## Renderer contract and parity

Target v0.5.0. Starting point: a [state comparison](../architecture/OUTPUT_PATHS.md#4-parity-evidence)
that no consumer feeds; `hydra-toon` draws through its resident direct path; the
[live UsdSkel output](../architecture/USD_POSE_OUTPUT.md) carries pose only, for usdview; no
Hydra publication.

### Consumer state contract

The [evaluated state](../contracts/EVALUATED_STATE.md#4-consumer-contract) and
[output paths](../architecture/OUTPUT_PATHS.md) state at least:

- avatar and instance identity, layout identity and when it changes
- the space and units of transforms, and the order and identity of joints
- morph target identity and what a weight means
- scalar and vector material overrides, and the targets and identity of UVs
- absent, unsupported, zero and invalid values, kept apart
- snapshot immutability, retain and release, and what survives a reset
- publication failure, a late consumer, destroy and recreate
- negotiation of supported channels, and how a consumer declares one unsupported

Each gap becomes an issue and a negative test. Hydra copies and locators belong to the
publication adapter: no Hydra concept enters the core state schema.

### Consumer-side export and parity

Each consumer writes what it **received** in the `--state-output` format that
`scripts/compare_avatar_state.py` reads. Comparing values the runtime produced with each
other cannot detect a loss in delivery. Joint values reach `hydra-toon` as palettes, so it
reports them before mapping.

| Case | Compared |
| --- | --- |
| VRM clip, expression, gaze and spring | pose, morph, material, layout, lifetime |
| MMD PMX, VMD and physics | joints, morph, primary UV, material, reset |
| reset, destroy and recreate | old and new snapshots stay apart, with their identities |
| missing or unsupported channel | both paths declare the same failure or absence |
| several instances on one thread | no crossing; identities are unique |

Tolerances are declared per value kind, and channels that cannot be compared are listed.
Pixel differences are a separate lane, with their own criterion if one is ever required.

### Hydra publication

- An opt-in adapter outside core, converting the committed snapshot to Hydra's
  representation without repeating any evaluation.
- Contracts for locators, dirtiness, resource lifetime and incremental update, agreed with
  `hydra-toon` under its
  [scope policy](https://github.com/animu-sphere/hydra-toon/blob/main/docs/design/INTEGRATION_SCOPE_POLICY.md)
  (RT-O9).
- A demonstration consumer in which `hydra-toon`'s direct path and Hydra path take the same
  input.
- Core and the direct sessions still build and run without Hydra.

### Accepted when

1. From the same committed state, pose, morph, material and UV values delivered through
   Direct and Hydra compare numerically within the declared tolerances.
2. Unsupported channels are declared, never ignored silently.
3. Reset, recreate, retain and release break no lifetime.
4. A minimal parity test runs automatically and headless on Windows and Linux; tests that
   need a GPU run in a separate lane.
5. `hydra-toon`'s tests and this repository's tests each show what they own.

## Diagnostics, replay and performance

Target v0.6.0, so that parity failures stay cheap to investigate. Starting point: bounded
diagnostics delivered synchronously and the state comparison; no trace, inspect, replay or
benchmark host.

### Trace events

- frame and tick ID, evaluation seconds, source timestamp, instance and world ID
- evaluator ID, phase, start, end and duration, dependencies, commit or abort
- capability, unsupported channels, missing input, provider errors with their diagnostic codes
- before and after physics Step, world invalidation, reset
- publication start, end and failure, and the output layout version

Tracing is bounded and opt-in: normal runs do no steady allocation or logging for it.

### Reproduction

- Record input frames, clock policy, session configuration, provider versions and a fixture
  fingerprint.
- Define the canonical input envelope that replay reads (RT-O8,
  [recording and replay](../design/RECORDING_AND_REPLAY.md)).
- State the scope of determinism: the same OS, solver and configuration, apart from across
  platforms.
- Replay compares frame state and locates the failing phase.
- Format-specific binary codecs stay with their owners; core manages the envelope and lifecycle.

### Benchmarks

| Measure | Of |
| --- | --- |
| evaluator time | per phase and provider |
| session frame time | one avatar and several |
| publish time | Direct and Hydra |
| allocations and copies | per frame and updated channel |
| update traffic | morph, material, transforms, UV |
| reset and rebuild cost | session and physics world |
| catch-up budget | missed ticks and backlog |

### Accepted when

On fixed fixtures, trace -> replay -> state diff reproduces, and the performance report
states its environment and method.

## Several avatars and a shared world

Target v0.7.0. Rules are in the
[evaluator contract](../contracts/EVALUATOR.md#5-shared-world-physics-orchestration) and
[lifecycle](../architecture/FRAME_LIFECYCLE.md#5-physics-clock-and-discontinuities). One world
per session is [decided](../architecture/FRAME_LIFECYCLE.md#rt-o6-one-world-one-session);
shared participants wait for this gate.

### Ownership to settle

- who creates and destroys the world, and who runs Step
- how an avatar or session joins and leaves, and how long its references live
- the Step barrier and synchronization points in a shared world
- the unload policy when cross-avatar constraints exist
- where Stage Runner and this runtime meet, with automatic world and participant binding

The host owns the world and an avatar owns its participant. Core embeds no Jolt SDK and keeps
the solver-independent table.

### Tests

- two avatars in one world, with exactly one Step per tick
- display requests with zero, one and several catch-up ticks
- pause, reset and unload of one avatar
- failure before Step, and failure after Step, which invalidates and resets
- physics-disabled static and motion-only paths stay valid
- a declared refusal or handling of a cross-avatar constraint in the same world
- seek and loop wrap declare invalidation or a supported checkpoint, not complete replay to
  any time

### Accepted when

Exactly one Step per tick is measured, and selective removal keeps state, lifetime and
constraints consistent. Core stays free of mandatory MMD, Jolt and renderer packages. Normal
CI covers core units and headless smoke; heavy real-asset, backend and render checks stay
opt-in.

## Connector input and recording

After v0.7.0. Ownership:

- `motion-connectors` owns camera, motion-capture and network intake, transport and source time.
- `usd-motion-plugins` owns motion representation and semantics, retargeting and recording
  semantics.
- This runtime receives normalized `AvatarInputFrame`s and owns evaluation time, the contract
  of selection and merge policy, and state publication.

The capture and replay envelope makes sequence, timestamps, validity, provenance and the drop
and interpolation policy explicit; arrival order and loss are never carried implicitly into the
runtime. Live expression and gaze, priority, conflict and loss between several sources, and
automatic native attribute discovery are checked here.

Accepted when external input -> `AvatarInputFrame` -> session -> state -> renderer runs, and
missing input and clock skew are recorded so that they reproduce.

## Bindings and Web

Python and JavaScript/Wasm start after the [ABI freeze](#abi-freeze). Web UI, WebXR device
adapters and WebGPU renderers do not enter core; the host owns the browser UI and draw loop.
Measure Wasm size, load latency, memory and ABI differences separately. Native support does
not establish browser support.

## Execution and scheduling

OpenExec wrappers and parallel scheduling follow stable evaluator cores. Direct evaluation
must stay functional with OpenExec disabled and give equivalent state and diagnostics
through both. Parallelism needs explicit state dependencies and shared-resource lifetime
evidence.

## Owner-led work

When an owner's feature is missing, it is recorded as a requirement to that owner. No
substitute solver is written here or in a renderer.

| Area | Owner | This runtime's part |
| --- | --- | --- |
| VRM node constraints, LookAt, expression, spring solving | `usd-vrm-plugins` | evaluator order and composing results |
| MMD IK, append, morph, format control | `usd-mmd-plugins` | phases, snapshot and physics feedback |
| motion clips, retargeting, recording semantics | `usd-motion-plugins` | the boundary between input and evaluated state |
| Jolt and physics solving | `usd-physics-plugins` | the shared-world barrier, time and failure contract |
| scene and world lifecycle | `usd-stage-runner` and the host | avatar participants and their contract |
| rendering, shaders, resident GPU resources | `hydra-toon` | the output adapter and state semantics |
| transport and device intake | `motion-connectors` | receiving frames with timestamps and validity |

### VRM node constraints

[Deferred past v0.3.0](../architecture/VRM_SESSION.md#node-constraints): the owner has no
evaluator to integrate. Accepted when the owner publishes one, the session selects it
explicitly, its phase order against motion, LookAt and spring is agreed with the owner, and
real-avatar results match the owner within tolerance.

### VRM secondary-motion integration

Spring runs in the session through the owner solver, under the
[owner's policy](https://github.com/animu-sphere/usd-vrm-plugins/blob/main/docs/design/VRM_MOTION_POLICY.md#10-spring-bones-on-a-rig).
Open: scaled spring rigs, changing spring mode without rebuilding session and consumer
bindings, composition with the shared-world barrier, resident rendering and Hydra/direct
agreement. Reset, seek and loop wrap initialize from the declared animated pose; a time jump
is never passed as one large or negative step.

### MMD session

After the [package](../releases/v0.4.0.md): complete non-bone morph output, including
additional UV channels and flip and impulse morphs; PMX 2.1's other joint types; resident UV
mapping and coherent tangents; representative private-asset, spring and contact checks.
Several avatars wait for the [shared world](#several-avatars-and-a-shared-world).

## Planned slices

Each slice pairs documentation, code, automated tests and measured evidence. The capability
matrix does not say `yes` before the code does.

| Order | Slice | Expected change |
| --- | --- | --- |
| 1 | consumer state contract gaps | identity, space, absence, lifetime and capability rules, with negative tests |
| 2 | consumer state export | Direct and Hydra write the values they received in one format |
| 3 | Direct and Hydra parity harness | tolerances, a table of unsupported channels, a CI lane |
| 4 | minimal Hydra publication | opt-in adapter, diff and dirtiness, lifetime tests |
| 5 | trace and inspect host | a bounded event contract, failure localization |
| 6 | replay fixtures and deterministic state diff | source metadata, captured inputs, repeatability |
| 7 | consumer performance baseline | latency, copies, allocations, curves over several avatars |
| 8 | two avatars in a shared world | exactly one Step, removal, reset and failure cases |
| 9 | external input adapter prototype | explicit clock, validity and packet loss |
| 10 | ABI freeze readiness review | evidence for the open decisions, and the freeze decision |

Before slice 1: open issues for the consumer contract's undefined points, separate headless CI
from GPU and real-asset checks, and choose the performance measures and first environment.
The state comparison already carries identity, absence and reset negative cases
([parity evidence](../architecture/OUTPUT_PATHS.md#4-parity-evidence)); its remaining gaps are
listed there. Each slice
touching `hydra-toon` states in its pull request which repository owns which test.

## ABI freeze

The ABI stays experimental until all of these hold, and the decision is revisited at each
release. Versions, struct sizes and capabilities carry extension meanwhile, with old
consumers still checked.

- VRM and MMD, and several kinds of external consumer, use the same scheduler and state
  contract.
- C layout, size, calling convention, allocator, handles and lifetime are checked across
  toolchains, with separately versioned real providers.
- Clock and space mappings, absence versus zero, capabilities, provenance, channel identity,
  final rig representation and snapshot rules are specified: ownership, buffer lifetime,
  retention, absent channels and multi-instance identity.
- Plan order is validated against motion, VRM and MMD dependencies, and negative tests pass
  for abort, retry, reset and physics invalidation.
- Provider checkpoint restore is implemented, or explicitly deferred with its limits.
- Capability negotiation makes unsupported channels observable, without forcing
  format-specific elements on another format.
- Core ABI, session API, adapters and renderer transport each have a stated compatibility
  policy.
- Install-tree CI, failure recovery, public consumers of installed packages and
  compatibility tests exist.
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
| Rendering | the values each consumer received, resource lifetime, transform and expression updates, device-reset equivalent |
| Cross-platform | Windows and Linux build and run, dependency agreement, reproducible packages |
| Negative | bad input, evaluation failure, partial failure, destroy/recreate, interference between avatars |

Record results separately as `unit`, `adapter integration`, `installed-package consumer`
and `real-asset end-to-end`, and do not state success more broadly than the category.
Record the OS, SDK, assets with usable rights, provider versions, configuration and
tolerances. A successful C call does not show that a provider is correct.

## Risks

| Risk | Response |
| --- | --- |
| Rendering paths diverge in meaning | the values each consumer received are compared automatically |
| The runtime grows format-specific | evaluators and solvers stay with their owners; no format algorithms in core |
| Adapters and the shipped session drift apart | the matrix tracks owner, session and renderer state per feature |
| The public API freezes early | the ABI stays experimental until the freeze gates hold |
| Shared physics leaves bad state | barrier, world invalidation, removal and reset have negative tests |
| Tests lean on real assets | redistributable generated fixtures and local real-model runs are separate lanes |
| Linux and Windows results are confused | evidence states OS, SDK, assets, providers and tolerances |
| Hydra support slows the direct path | opt-in adapter, feature flags, allocation measurements |
| Packages collide | requirements and ownership for coexistence in one process are written down first |

## Open decisions

IDs are stable. Resolving one updates the owning contract and records the evidence.
These are runtime design questions, not reported upstream bugs.

| ID | Decision | Owner here | Needed by |
| --- | --- | --- | --- |
| RT-O1 | evaluation seconds, source clock, USD time-code mapping and gaze space descriptors | [input](../contracts/INPUT_FRAME.md) | ABI freeze |
| RT-O2 | semantic intent vocabulary, custom and native channel names, mapping precedence; resolved expression identity, arbitration, provenance and availability. The session publishes effects only, under the [consumer contract](../contracts/EVALUATED_STATE.md#4-consumer-contract) | [input](../contracts/INPUT_FRAME.md), [state](../contracts/EVALUATED_STATE.md) | ABI freeze |
| RT-O3 | **Decided for v0.4.0:** the [existing phases fit](../contracts/EVALUATOR.md#6-rt-o3-the-existing-phases-fit) MMD morph, append, control, IK and post-physics dependencies, and VRM gaze and expressions, with no phase extension. A third format reopens it | [evaluator](../contracts/EVALUATOR.md) | done; ABI freeze for a third format |
| RT-O4 | typed gaze results, retained diagnostics and snapshot provenance, and real-provider layout conformance. Pose, spaces, layout identity and complete snapshots are in the [consumer contract](../contracts/EVALUATED_STATE.md#4-consumer-contract) | [state](../contracts/EVALUATED_STATE.md) | absence and unsupported channels by v0.5.0; the rest by ABI freeze |
| RT-O5 | reset, checkpoint, commit and abort for stateful evaluators; shared-world discontinuity scope, post-Step recovery, publication failure. The MMD session recovers by Reset, which rebuilds its world | [lifecycle](../architecture/FRAME_LIFECYCLE.md) | publication failure by v0.5.0; shared-world scope by v0.7.0; ABI freeze |
| RT-O6 | **[Decided for v0.4.0](../architecture/FRAME_LIFECYCLE.md#rt-o6-one-world-one-session)** for one world per session: handles, fixed ticks and catch-up, tick/display and alignment. Interpolation and provenance are deferred to the freeze, and shared participants to v0.7.0 | [evaluator](../contracts/EVALUATOR.md#5-shared-world-physics-orchestration), [lifecycle](../architecture/FRAME_LIFECYCLE.md#5-physics-clock-and-discontinuities), [dependencies](../architecture/DEPENDENCIES.md#3-integration-questions) | done for one session; shared participants by v0.7.0 |
| RT-O7 | C layout and calling convention, strings, allocator ownership, handles, provider lifetime and negotiation | [ABI](../contracts/ABI.md) | ABI freeze |
| RT-O8 | capture envelope, checkpoint encoding and reuse of motion recording | [recording and replay](../design/RECORDING_AND_REPLAY.md) | the replay envelope by v0.6.0; recording; lifetime prerequisites by ABI freeze |
| RT-O9 | direct-consumer binding and update costs, Hydra locators and dirtiness, parity tolerances | [output paths](../architecture/OUTPUT_PATHS.md) | v0.5.0; costs by v0.6.0 |
