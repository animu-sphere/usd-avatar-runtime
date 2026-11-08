---
status: proposed
owner: usd-avatar-runtime
---

# Evaluated-state publication

All paths consume one [evaluated state](../contracts/EVALUATED_STATE.md).

`hydra-toon` direct/fast-path is the first renderer consumer. It comes before full
Hydra publication, to validate state semantics and measure latency, copy cost and
allocations without first freezing locators or Scene Index structure.

```text
                     +-- fast/direct API -> hydra-toon (first consumer)
EvaluatedAvatarState +-- Hydra adapter --> render delegates (later)
                     +-- recording/bake
```

Hydra and direct consumers share frame/state identity and evaluated values.
Hydra publication is transport, not a second evaluation of avatar semantics.

Both renderer paths consume the same final snapshot after the runtime's
[pre/Step/post orchestration](../contracts/EVALUATOR.md#5-shared-world-physics-orchestration).
Neither renderer adapter runs Physics Step, IK or owner bone/body feedback.
Published display state is separate from mutable solver state; renderer retries
reuse retained CPU buffers without advancing simulation. Copy/packing
optimizations must preserve that boundary.

Fast-path transport may use zero/minimal-copy views, dirty masks, late
publication and GPU-friendly packed views with frame/generation identity.
These are allowed optimizations, not current support claims; canonical values
and semantics remain in evaluated state. GPU resource management, shaders,
draw submission and late-latching implementation stay with the renderer.

## 1. Hydra

The scoped [live UsdSkel output](USD_POSE_OUTPUT.md) publishes final joints
through an anonymous session sublayer for usdview. This pose-only application
path supplies no material/morph/visibility overlay or complete Scene Index and
does not close the Hydra/direct parity gate. Its first consumer is the optional
[live MMD host](../guides/live-mmd-physics.md).

```text
UsdImagingStageSceneIndex
           |
AvatarEvaluationSceneIndex <- published EvaluatedAvatarState
           |
       renderer
```

The optional evaluation Scene Index overlays resolved skeleton transforms,
blend-shape weights, material input values and visibility over the authored
scene. Static topology and canonical material meaning still come from the
authored stage and its format imaging adapters.

The adapter owns mapping runtime target identities to prim/data-source paths,
dirty notifications and snapshot lifetime. Exact data sources and locators
need an agreed OpenUSD/consumer integration before this becomes binding.
They are not defined by the conceptual Scene Index name above.

No per-frame writeback to the stage is required. A structural stage edit
requires binding invalidation; it is distinct from a value-only runtime update.

## 2. Direct consumer API

Experimental `evaluate_frame`/`get_snapshot`/`retain_snapshot`/`release_snapshot`
expose the complete resolved state through
[`api.h`](../../include/avatarRuntime/api.h). Snapshot transport/lifetime are
implemented and tested. The optional motion-check host composes the
installed `Toon::AvatarState` consumer over an explicit probe scene as described
below. Actual resident-avatar binding, full consumer capability negotiation and
Hydra/direct parity remain open.

```text
EvaluatedAvatarState -> direct consumer adapter -> renderer resources
```

The direct path avoids Hydra transport while preserving all evaluator semantics.
The consumer maps resolved channels to its own skeleton/morph/material buffers.
It owns backend resource updates and rendering. The runtime exposes no Vulkan,
WebGPU or renderer-private material objects.

The binding/configuration generation, frame identity and channel layout must
be observable so a late consumer cannot combine a pose with stale topology or
another frame's appearance. Supported output types are negotiated explicitly;
an unsupported material override is diagnosed rather than silently dropped.

An initial adapter to `hydra-toon` must be agreed with that repository's
[scope policy](https://github.com/animu-sphere/hydra-toon/blob/main/docs/design/INTEGRATION_SCOPE_POLICY.md).
Existing fast-input support is not evidence of this common-state API.

The renderer-owned consumer contract is the public
[`avatar_state.hpp`](https://github.com/animu-sphere/hydra-toon/blob/main/include/toon/fast/avatar_state.hpp)
and its [adapter/package boundary](https://github.com/animu-sphere/hydra-toon/blob/main/docs/architecture/PROJECT_LAYOUT.md).
The canonical state representation remains the runtime's
[evaluated-state contract](../contracts/EVALUATED_STATE.md); do not create a
second physics-specific state implementation for the fast path.

### Resident target matching

Matching runtime target identities to a renderer's resident targets belongs to
a host on the renderer's side, not to this repository. For `hydra-toon`, that
host pairs skeleton paths, joint tokens, mesh paths, `skel:blendShapes` tokens,
material paths and canonical inputs with the resident targets its Hydra
delegate describes, then builds the bindings that the renderer's installed
`Toon::AvatarState` adapter consumes. The adapter reads retained snapshots
directly through the [direct API](#2-direct-consumer-api), so no runtime-side
bridge is needed for transport.

This follows from ownership: the consumer maps resolved channels to its own
resources, and the runtime manages no renderer resources
([design policy](../design/DESIGN_POLICY.md#publication)).
Resident-target description is also renderer-internal: its delegate API is not
installed, so only code built with the renderer can call it. The planned
`avatarHydraToonBridge` target is therefore withdrawn. The runtime still
publishes stable target identities and layout ID/version, and the
[scoped transport check](#scoped-toon-transport-check) remains a probe over
generated resources, not real-avatar matching.

The initial retained-snapshot adapter updates resolved pose, morph/deformation,
expression effects, appearance and visibility. Gaze arrives as resolved
pose/expression effects under the
[consumer contract](../contracts/EVALUATED_STATE.md#4-consumer-contract). Consumers
do not interpret VRM LookAt, expression arbitration, MMD morph semantics, IK
or retargeting. Test layout change versus value-only update explicitly; a reset
generation alone must not imply changed topology. Measure latency, bytes/copies
and allocations with the actual input/state/layout and renderer configuration.
Actual avatar geometry/resource binding and rendering remain planned requirements.

### Scoped Toon transport check

`AVATAR_MOTION_CHECK_TOON` optionally links only `avatarMotionCheck` and its
regression host to an installed `Toon` package's `AvatarState` component.
It requires the VRM mode and the additive `BaseColorRgb` binding; configure-time
header probes reject older same-version renderer installations and installs
built for a different runtime ABI revision. Core and
all reusable runtime/provider targets retain their existing dependencies.

`avatarMotionCheck --vrm --toon <avatar> <motion> [motion ...]` evaluates the
existing motion/LookAt/Expression plan and compares the owner's results to
state before handing the completed snapshot to the renderer-owned adapter.
The host explicitly constructs a triangle probe with a reversed joint palette,
reversed resolved morph slots and canonical material bindings from the VRM
owner's slot table. All six colour slots map explicitly, including separate
base RGB/alpha. Unknown material inputs and visibility layouts are refused,
rather than silently omitted. This is a transport probe, not extraction or
rendering of the avatar's actual meshes, inverse binds or material resources.

The probe uses centimetre scene units, identity inverse binds and explicit
renderer-space transforms. An independent double-precision USD matrix oracle
compares parent-composed joint values, while all weights and material RGB/alpha
are checked. Matrix/weight comparisons use
`abs(actual-expected)/max(1,abs(expected)) <= 2e-6`; matrix translations are
converted back to metres before comparison. Material values must exactly match
the expected float conversion. Owner/state comparisons keep their existing
`1e-6` tolerance. These measure transport parity, not provider mathematics.

Every result also reaches `ApplyFastSnapshot` without structural changes.
Frame/instance/generation/layout/input revision and binding epoch are checked;
duplicate results reuse dynamic arrays/revisions, reset preserves binding,
older generations are refused atomically and a changed layout requires rebinding.
The host releases all producer snapshot references after runtime destruction,
then checks independently retained old/current consumer snapshots and release
restoration. The regression oracle rejects deliberately perturbed palettes,
weights and materials. No GPU, per-frame USD authoring or consumer-cost claim
is implied. Validation evidence and its asset limits belong in the
[capability matrix](../reference/CAPABILITY_MATRIX.md).

### Scoped MMD transport check

`AVATAR_MMD_TOON_SMOKE` adds a separate test host above the optional MMD/Jolt
smoke. It passes the final retained post-physics snapshots directly to the
installed renderer's `AvatarStateAdapter`; no renderer call evaluates MMD,
steps the world or writes USD. Bindings explicitly map a generated triangle's
reversed joint palette, one scalar morph and one visibility target. Unknown
material output or a different channel layout is refused by the probe.

The primary UV slice additionally maps owner-evaluated absolute UV slots onto
explicit probe vertices through the installed consumer's `AvatarUvBinding`.
This host checks source identity reordering, simultaneous pose/morph transport,
duplicate arrays/revisions, two instances and retained snapshots after reset
and producer destruction. The installed header probe requires UV bindings as
well as the matching runtime ABI. UV changes use ordinary draw extraction;
resident target discovery and rendered/runtime/Hydra comparisons remain
renderer-host work.

An independent USD matrix oracle composes parent-local snapshot transforms,
converts metre translations into centimetre scene units and checks the renderer
palette. The host also compares resolved morph weight, visibility, frame/layout
identity and extracted draw values, then exercises duplicate publication and
retained consumer lifetime after reset and producer destruction. Visibility
membership changes use ordinary draw extraction, consistent with the renderer's
fast-update boundary. This is explicit resource transport, not resident-avatar
discovery, GPU rendering, Hydra parity or the complete physics MVP. Current
measurements belong to the
[capability matrix](../reports/evidence-log.md#scoped-mmdtoon-evidence-2026-10-21).

## 3. Recording, baking and export

Recording is a publication adapter to the motion-owner recorder. The runtime
must not implement generic resampling, compression, clip construction or a
replacement motion storage format. Avatar-specific non-motion state recording
needs a separate design; no recording adapter is implemented yet.

Capture observes the same resolved snapshot plus evaluation provenance;
[recording and replay](../design/RECORDING_AND_REPLAY.md) owns its format proposal.
Baking explicitly maps values to authored USD samples through the applicable
motion/format contracts. Source-format export stays with format owners.

The runtime does not define a replacement VRM/MMD export representation or
require bake/writeback for live rendering.

## 4. Parity evidence

Feed one input sequence, initial state and configuration through one evaluation
and deliver each resulting snapshot to both adapters. Compare the resulting
joint transforms, deformation values, material overrides, visibility and
target identity before judging rendered output. Require consistent diagnostics
for unsupported effects.

For the first slice, compare evaluator reference results with the retained
snapshot and fast-path mapped values for pose, expression and gaze, including
their resolved deformation/appearance contributions. Later feed those same
snapshots to the Hydra adapter. Shared state semantics must be established
before transport parity; pixel parity stays in renderer tests.

Also test value-only updates, structural rebinding, absent/zero channels and
retained output lifetime. Parity criteria specify numeric tolerances per value
kind and quaternion sign equivalence; they must not hide missing channels.
Reuse motion-owner comparison rules for motion values.

`scripts/compare_avatar_state.py` applies these rules to two files in the
`--state-output` format, one state or a list in the order the consumer received them. A consumer
writes what it received in that format, and the script matches frames in order and records by
identity. Each state carries `instance`, `layout`, `layout_version`, `generation`, `frame`,
`input_revision`, `seconds`, `capabilities` and the channels `joints`, `morphs`, `uvs`,
`materials`, `visibility` and `samples`. An empty channel is not an output of the layout; a missing
one is a channel the writer did not carry. Identities are opaque UTF-8 with JSON escaping only.

| Value | Agrees when |
| --- | --- |
| layout, layout version, generation, frame, input revision, joint parent, material type and override flag, visibility, sample evaluator, kind, validity and resolution | equal |
| instance | one to one: handles differ between runs, but two instances never cross |
| translation, rotation, scale, morph weight, primary vertex UV, evaluation/source/runtime seconds, clock scale and offset | `abs(actual-expected)/max(1,abs(expected)) <= 2e-6`; a quaternion also agrees with its negation |
| material value with `overridden = 1` | equal after conversion to `float`; with `overridden = 0` the value is ignored |
| capabilities | the same set of `(id, version)` pairs |

`capabilities` contains objects with a nonempty `id` and nonzero uint32 `version`.
`samples` contains `source`, `actor`, `channel`, `evaluator`, `kind`, `validity`,
`resolution`, `source_seconds`, `clock_scale`, `clock_offset` and `runtime_seconds`.
Samples match by `(source, actor, channel)`, the runtime's unique sample identity;
different sources or actors may report the same channel. `evaluator` compares exactly
but does not make a repeated sample identity distinct. Selected inputs use an empty
evaluator string. Identities are opaque and may be reordered by the consumer.

Every sample's four clock numbers must be finite, scale must be positive, and runtime
seconds must equal `source_seconds * clock_scale + clock_offset` within `1e-12` absolute
or relative error. This internal consistency check is independent of the comparison
tolerance. It validates supplied mappings; the comparison reads no clock.
Legacy exports with capability strings or incomplete sample provenance fail validation;
regenerate them with current writers, or omit the channel and declare it unsupported.

A missing or unexpected target, a repeated identity and a missing channel are differences.
`--unsupported <channel>` excludes a channel the consumer does not carry, and the report names it.
`--tolerance` replaces each between-file tolerance, for a comparison across platforms such as
`scripts/check_session_fixture.py`, which uses `1e-5`.

Tolerances must be finite and nonnegative. JSON nonfinite values, including exponent
overflow, are refused. Compared nonfinite values and material float32 overflow are
differences rather than agreement or an uncaught conversion error. The headless plain
CMake CI lane runs the comparison's Python tests independently of provider SDK builds.

Each sequence is also checked on its own against the
[consumer contract](../contracts/EVALUATED_STATE.md#4-consumer-contract), so a consumer cannot
agree with a producer that breaks it:

- per instance, `(generation, frame)` increases strictly: a reset restarts frames in a later
  generation, and an older generation is a difference
- per instance, the layout tuple, the identities of each layout channel, joint parents, material types
  and capabilities stay fixed; a change of binding needs a new instance
- instance, frame and generation are nonzero and the layout ID is not empty
- rotations are unit quaternions within a squared-norm error of `1e-6`, override and visibility
  flags are 0 or 1, material components past the value type are zero, and capabilities do not repeat
- in the expected file, which keeps the producer's order, each parent is -1 or an earlier joint of
  the same skeleton. A consumer may reorder records, so its parents are compared by joint identity
  only

`scripts/test_compare_avatar_state.py` checks that each value kind is compared and that identity,
absence, reset and shape violations are caught; `contracts.state_json` checks the writer the check
hosts share. The generated Direct consumer export below feeds this format. Resident VRM/MMD
host exports and Hydra publication/export remain open; no Direct/Hydra comparison is established.

A consumer declares an unsupported channel only on the command line, not in its file.
Typed expression/gaze results and retained diagnostics remain outside the state ABI and
this diagnostic format.

### Direct consumer export

The optional diagnostic overload of `hydra-toon`'s
[`AvatarStateAdapter::Apply`](https://github.com/animu-sphere/hydra-toon/blob/main/include/toon/fast/avatar_state.hpp)
fills a host-supplied `std::string*` with one JSON object after all binding and conversion
checks succeed. It reports the input values the adapter accepted, including parent-local
joints before palette mapping. Failure leaves the prior capture unchanged. The ordinary
overload does no serialization and keeps no capture buffers. The host owns file I/O and
collects these objects in a JSON array. ABI-4 consumers omit `uvs` and must declare that
channel unsupported; the complete probe below requires ABI 5.

`toon-avatar-state-test --state-output <file>` is the renderer-owned headless diagnostic
host. `scripts/check_consumer_state.py <consumer-executable>` compares six accepted states
with an independent Python oracle, including input revision, reset, recreated instance,
visibility, UVs, baseline restoration, escaped identities and handles larger than 2^53.
The probe also carries versioned capabilities and three samples sharing a channel across
different sources/actors, with nonidentity clocks and an empty selected-input evaluator.
Instance handles use the canonical JSON integer representation and are read without loss
by Python; readers using double-precision numbers must preserve those integers separately.
Seventeen deliberate delivery omissions and identity/visibility/provenance changes must fail comparison.

The runtime owns the common-format oracle; the renderer owns conversion, rejection and
capture tests. Setting `AVATAR_STATE_CONSUMER_EXECUTABLE` when configuring the runtime adds
the opt-in `consumers.direct_state_export` CTest. Core builds without that option need no
Python or renderer dependency. This establishes generated Direct consumer receipt;
resident VRM/MMD viewport export, Hydra and pixel comparison remain outside this probe.

Equivalent state does not by itself prove equivalent pixels or equal latency.
Renderer evidence must state backend, configuration and hardware separately.
