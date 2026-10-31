---
status: binding
owner: usd-avatar-runtime
---

# Native VRM clip session

`AVATAR_BUILD_VRM_SESSION` composes one schema-bound VRM and one owned motion
clip through `avatarVrmSession::Session`. Enable the USD, VRM, VRM USD, spring USD,
spring evaluator, motion USD and motion adapter options too. Installed consumers
request `AvatarRuntime` component `vrm_session` and link
`AvatarRuntime::avatarVrmSession`. This imports installed owner packages without
Jolt, Stage Runner or a renderer. The published
`avatarVrmSession` package holds these targets and the core in one prefix; pin it
with its owners from the [release notes](../releases/README.md).

The host snapshots Humanoid/skeleton, spring/collider values and the clip's
coherent source rest. Input stages can be destroyed after construction. Motion
samples at runtime tick time, then the secondary-phase evaluator receives that
tick's animated pose. Owner algorithms remain in `motionSampling`,
`motionRetarget` and `vrmRig`. Clip channels and gaze are selected only as
[clip observations](#clip-expressions-and-gaze).

The core [native fixed-tick session](FRAME_LIFECYCLE.md#native-backend-free-fixed-tick-session)
owns accumulation, seeding and retained output. `Play`/`Pause` affect the next
elapsed request; the caller supplies only time elapsed in the current playback
state. A paused request adds no time and does not drain backlog. `Reset(start)`
pauses, increases generation, clears history/output/backlog, and declares a new
seed time. The next `Advance(0)` initializes the current animated pose. A seek
starts a fresh simulation there; it does not reconstruct earlier velocities.

`SessionConfig::secondaryMotion` explicitly selects spring or motion-only
evaluation at construction. A live enable/disable or binding change closes the
old session and constructs a new one, including a new consumer binding. It is
not an in-place evaluator-plan edit. Motion-only mode does not read spring
declarations. Spring mode retains the existing rigid, unit-scale, metre-stage
admission boundary and refuses unsupported inputs with owner/binding diagnostics.

## Expression and LookAt

`SessionConfig::expressions` and `lookAt` explicitly select the owner evaluators at
construction, as `secondaryMotion` does, and both default to off. A selected capability
the avatar does not author refuses construction with the binding's diagnostic.
Expression-type LookAt requires selecting expressions as well; bone-type LookAt does not.
Expression inputs without selected expressions are refused.

The plan is `vrm.session.motion` (`AR_PHASE_RETARGET`), then `vrm.session.face`
(`AR_PHASE_EXPRESSIONS`), then `vrm.session.spring` (`AR_PHASE_SECONDARY`), each naming its
predecessors. `vrm.session.face` is one [atomic owner sequence](VRM_ADAPTER.md#execution-and-supported-gaze):
LookAt reads the animated pose, and its expression contributions reach the owner resolver
once. Spring receives motion and any replaced eye rotations.

The host maps each owner expression name to an input identity through
`expressionInputs`, names its gaze channel through `gaze`, and passes selected observations
to `Advance(elapsed, observations, sink)`. The session copies the arrays' pointers and the
input revision for that call, and the runtime supplies frame identity and time. Observations
are held across a catch-up request. An observation absent from a request contributes
nothing, and the complete snapshot returns its outputs to baseline.

Selecting expressions adds morph and material outputs to the layout, so a host that changes
the selection for one layout ID must change `HumanoidBindingConfig::layoutVersion`.
Consumers receive resolved blend-shape weights, material values and joint rotations. The
session publishes no expression names or LookAt status records.

## Clip expressions and gaze

A clip's expressions and gaze change between ticks, so one observation set held across a
catch-up request cannot carry them. `SessionConfig::clipExpressions` maps clip channels to
owner expression names, and `clipGaze` selects the clip's gaze, which also requires selecting
`expressions` or `lookAt`. The caller reads these inputs explicitly through
`MotionStageReadOptions`; nothing discovers them. The channel prefix and name are the host's
choice, with no aliases or conversion between VRM versions.

The session then selects observations for each evaluation through the
[per-evaluation form](FRAME_LIFECYCLE.md#native-backend-free-fixed-tick-session) of
`FixedTickSession::Advance`: owner `SampleClip` at the tick's evaluation seconds, then
`MotionPoseInputBridge`. They are ordinary input records, so the runtime validates them and
retains them as [source samples](../contracts/EVALUATED_STATE.md#source-sample-provenance)
under source `vrm.session.motion`, the skeleton as actor, and channels `clip:<clip channel>`
and `motion:lookAtTarget`. Evaluators still receive only `ArInputFrame`, and the clip's pose,
expressions and gaze come from one clock.

| Situation | Result |
| --- | --- |
| Gaze space | clip space, placed once by the avatar's rigid root placement to a runtime-world point; no animated hips, retarget height scaling or source-rig LookAt offset |
| Before the first key of a sparse channel, or at a sample without it | whatever the owner sampler gives, including absence, which contributes nothing |
| Past either end of the clip | scalars keep the owner's held value; gaze is `AR_OBSERVATION_STALE` and contributes nothing; both report the boundary sample's time |
| Input revision | the frame ID, replacing any host revision, because sampled content changes every tick and replays by frame |
| Paused or zero-tick request | no sampling and no evaluation |
| Sampling or validation failure | the tick stays due; retry with `Advance(0)` |

Each expression and the gaze have exactly one source. An expression mapped from both the clip
and `expressionInputs`, or `clipGaze` with a host `gaze` identity, refuses construction, as
does a mapped channel or gaze that no clip sample authors and a clip expression the avatar does
not declare. Different inputs may have different sources: host observations passed to `Advance`
join the clip's for every tick of that request. Unlike held input, they are validated per tick,
so an invalid host record refuses at the first due tick, after elapsed is accepted.

## Clock and lifetime

`Snapshot()` borrows the latest committed tick state. Retain through `Api()`
before advancing, resetting or closing; retained values survive runtime/provider
destruction. Delivery retries read that handle and do not call `Advance`.
`Stats()` exposes completed ticks in the last request, total tick index, fixed
simulation time, pending backlog and initialization. A failed evaluation retains
the latest successful tick and due time; retry `Advance(0)` with no extra elapsed.

The caller can hand the snapshot and `Binding()` to existing pose or renderer
consumers. `SkeletonPoseOutput` is an optional display transport over an anonymous
USD layer; it runs only when the host delivers a snapshot. `Close()` detaches the
clock before destroying runtime instances and then provider resources. Close the
output layer separately. All calls use one exclusive calling thread.

## Check host

`avatarVrmSessionCheck avatar.vrm clip.vrma fixed_seconds ticks [--no-springs] [--face]
[--clip-expression name-attribute value-attribute]... [--clip-gaze attribute]`
uses default-prim avatar scoping and requires exactly one clip skeleton. The
caller configures installed file-format/schema resources and DLL discovery.
For example, `fixed_seconds=0.016666666666666666` and `ticks=120` exercise two
seconds at 60 Hz. The check compares motion with direct owner retargeting,
springs with a separate owner solver, and delivered USD pose identity/local TRS
with the snapshot. Translation/quaternion component tolerance is `1e-6`; USD
half-scale quantization uses relative tolerance `5e-4` (absolute near zero).
It also checks paused output identity, exact reset replay and retained unload.

`--face` selects expressions and LookAt. It sweeps every expression and orbits a world-space
gaze point, leaving each absent at regular ticks. It resolves those observations through a
separate owner `LookAtEvaluator` and `ExpressionResolver` over the motion-only pose, then
compares blend-shape weights, material values and override state, eye rotations and the
spring result at `1e-6`. These observations are generated by the check, not captured.

`--clip-expression` and `--clip-gaze` read those attributes from the clip instead and replace
`--face`. Channels named `vrm:<expression>` map to that expression. The check samples the clip
separately with the owner, applies the placement and hold rules above and makes the same
comparison. It requires morph or eye changes only for the inputs selected, because a clip drives
only what it authors.

`tools/check-vrm-session.ps1` runs an explicitly supplied VRM 0.x, VRM 1.0 and
clip in both modes, with `-Face` for the face check, and writes asset hashes and outcomes to a caller-supplied
evidence file. No avatar or clip is copied into the repository.

Generated source/installed tests cover both spring declaration shapes, both LookAt
types, refused selections and stage-independent session lifetime. Measured real-avatar scope and exclusions
are recorded in the [capability matrix](../reports/evidence-log.md#vrm-fixed-tick-session-and-host-evidence-2026-10-29).
This is a headless pose host: resident mesh rendering, Hydra/direct agreement,
pixels, display interpolation and shared-world VRM composition remain open.
