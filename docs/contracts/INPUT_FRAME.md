---
status: proposed
owner: usd-avatar-runtime
---

# Avatar input frame

`AvatarInputFrame` is the logical, immutable input assembled for one avatar
evaluation. It is renderer- and format-independent. This document develops
[design policy section 5](../design/DESIGN_POLICY.md); it defines no final struct.

The experimental [`input.h`](../../include/avatarRuntime/input.h) implements
frame identity/generation, evaluation seconds, optional USD time mapping,
source-attributed scalar channels, typed gaze observations and host input
revision. This subset is not a second motion model:
motion pose/clip values are still unimplemented; gaze descriptors have scoped
revision-3 rules below, awaiting real-provider integration.
The full logical contract and RT-O1/RT-O2 remain open.

The optional [VRM adapter](../architecture/VRM_ADAPTER.md) now selects scalar
and gaze identities explicitly and marshals named expression weights to the
owner's `MotionChannelSet`. Both LookAt types accept valid world/joint-local
points and directions using current working rig transforms and the owner's
point/direction entry points; scaled ancestry fails visibly, and stale/unavailable
gaze contributes nothing. This does not establish connector/motion mappings
or narrow the revision-3 input transport contract.

## 1. Logical contents

| Part | Meaning |
| --- | --- |
| frame context | instance, frame identity, configuration generation and evaluation instant |
| motion | semantic pose/root input, clip playback or locomotion requests |
| gaze | a target point or direction in an explicitly named space, optionally eye observations |
| expressions | semantic intents and explicitly namespaced custom channels |
| parameters | typed application controls with stable identifiers |
| provenance | source/actor identity, observation timestamps, validity and mapping configuration |

Motion values consume
[`usd-motion-plugins`' contract](https://github.com/animu-sphere/usd-motion-plugins/blob/main/docs/design/MOTION_CONTRACT.md).
Connector frames/observations consume
[`motion-connectors`' contract](https://github.com/animu-sphere/motion-connectors/blob/main/docs/design/CONNECTOR_CONTRACT.md).
This wrapper adds composition context, not another pose or tracker taxonomy.

Revision 2 introduced `input_revision`, preserved in revision 3: a host-assigned
revision for the selected source observations and mapping configuration.
Zero means unspecified. It is
independent of frame ID and evaluation time; evaluating the same selected source
snapshot at a later instant may retain the revision. The host must change it
when selected observation/mapping content changes and interpret it within the
instance/configuration generation. The runtime echoes it into working and
published views and preserves each prior successful frame's revision. It does
not infer revision from arrival order, enforce monotonicity, cache evaluation
by revision or treat revision equality as full frame equality.

## 2. Time and coordinate boundaries

Canonical motion timestamps are seconds under the motion owner's contract.
USD time codes are a separate integration representation. The policy's
`double timeCode` example does not authorize mixing the two.

The host supplies an explicit source-clock-to-runtime mapping and, when a
stage is involved, a runtime-seconds-to-USD-time-code mapping using the stage's
time scale and declared offset. Preserve sample timestamps separately from
the instant being evaluated. Evaluators read no wall clock implicitly.

Reuse canonical motion units/basis without renormalizing already normalized
connector data. Gaze points/directions identify their reference space; a
direction is not treated as a point. Mapping a head-relative observation
requires the bound head transform, not a guessed VRM range-map result.
Real-provider validation of time/space descriptors remains `RT-O1` in the
[roadmap](../roadmap/current.md#open-decisions).

Near-term gaze input must distinguish target position, target direction,
head-relative and eye-relative observations, with explicit source clock and
validity. Missing/unavailable data is not an identity rotation or a valid zero
target. Revision 3 implements the scoped gaze descriptors below. Resolved
eye/head/expression contributions and clamped/rejected/unavailable results belong to the
[evaluated-state contract](EVALUATED_STATE.md), not this input intent.

In revision 3, each scalar and gaze records its source/actor/channel identity, source
seconds and an explicit positive affine clock mapping:
`runtime_sample_seconds = source_seconds * clock_scale + clock_offset`.
The evaluation instant stays separate. When present, the USD mapping is
`usd_time_code = evaluation_seconds * usd_time_codes_per_second + usd_time_code_offset`.
Both mapped results must be finite. Missing mappings are not guessed. The
runtime validates/forwards this metadata; it does not sample, blend or reject
stale samples on behalf of a connector/motion provider.

### Revision-3 gaze observations

`ArInputFrame.gazes`/`gaze_count` is a synchronously borrowed array of
`ArGazeInput`. Each record has the same source/actor/namespaced-channel identity
and affine clock metadata as a scalar. Identity is unique across both arrays:
the same triple cannot be reported as both scalar and gaze. Different sources
or actors may report the same gaze channel; explicit binding policy selects or
combines them. Array order is not arbitration.

| Field | Scoped meaning |
| --- | --- |
| `kind` | `AR_GAZE_POINT`: target position in metres; `AR_GAZE_DIRECTION`: unit vector with squared-norm error at most `1e-6` |
| `space` | `AR_GAZE_RUNTIME_WORLD`: canonical runtime world, including avatar placement; `AR_GAZE_JOINT_LOCAL`: the named bound joint's coordinate frame |
| `skeleton_id`, `joint_id` | both null in runtime-world space; both valid identities naming an existing baseline rig joint in joint-local space |
| `validity` | `AR_OBSERVATION_VALID`, `AR_OBSERVATION_UNAVAILABLE` or `AR_OBSERVATION_STALE`; zero/unknown values are rejected |
| `value[3]` | finite point/direction components; unavailable records require all-zero payload; stale records preserve their old finite point/unit direction |

All vectors use the motion owner's canonical right-handed +Y-up/+Z-forward
basis. Joint-local names identify the joint's own frame, rather than its parent
frame. A head-relative or eye-relative observation names the bound head/eye
joint using opaque rig identities, without creating another humanoid vocabulary.
The adapter constructs that joint's transform from the evaluated rig chain and
root placement. Points include translation; directions do not. Scaling and
direction normalization during space conversion belong to the owner adapter,
not the runtime. A provider needing that conversion declares pose reads and
the relevant ordering dependencies.

No record means absent. A valid point `(0,0,0)` is a real target; a valid zero
direction is rejected. Unavailable is an explicit report without a value;
stale retains an observation the assembler has classified as old. Validity
does not follow automatically from comparing timestamps. Providers receive
these statuses unchanged and must define their drop/hold/default policy. Even
unavailable/stale records must carry valid identity, kind, space/reference and
clock metadata. Validation does not imply that a provider supports every kind
or space; adapters diagnose unsupported requests and negotiate their outputs.

The runtime validates the complete array before any provider callback. Unknown
kind/space/validity, missing joint references, non-finite values, invalid source
clocks and duplicate identities fail the frame without advancing provider state
or successful-frame history; the host can correct and retry that frame ID.
Gaze-specific validation diagnostics identify the input channel with runtime instance
and frame context. Arrays/strings are never retained in published state.

This is an input composition boundary, not a new motion value: an owner
`MotionPose::lookAtTarget` maps to a valid world-space **point**, preserving its
timestamp and optional absence. A direction must not be assigned to that point
field without an explicit owner-supported conversion using the bound avatar.
The VRM adapter uses the additive owner `EvaluateDirection` API for directions;
it does not assign them to `MotionPose::lookAtTarget` or invent a target distance.
Direction evaluation uses the head orientation without positional eye parallax.
Real motion/connector/VRM mapping evidence, tracking-specific validity and
resolved gaze result/provenance records remain open before freeze.

## 3. Intake and ownership

The optional [clip pose adapter](../architecture/MOTION_ADAPTER.md) binds an
immutable owner clip in configuration and samples it using frame evaluation
seconds with an explicit affine clip/runtime clock mapping. It publishes rig
pose without adding owner types or a motion array to the C input ABI. Sampled
face channels/gaze points are diagnosed as unsupported by that pose-only
callback; host input assembly must map them separately. Actual motion/connector
observation transport, actor selection and retained source provenance remain
open. This scoped path does not complete input composition.

The same optional target now supplies host-side `InputAssembler`: explicit
owner channel mappings produce attributed scalar inputs, and the owner's
optional gaze point produces a world-point observation. It owns frame arrays
and strings, preserves the selected pose timestamp and supplied clocks, and
reports unmapped fields to the host. Null/missing fields remain absent; zero
remains present. Gaze validity is explicitly valid or host-selected stale,
without deriving source policy from owner sample status. The context must have
empty arrays; multi-source merging/arbitration, live connector intake and
retained owner metadata remain open. See the
[assembly boundary](../architecture/MOTION_ADAPTER.md#host-input-assembly).

Under the adopted [boundary policy section 6](../design/BOUNDARY_POLICY.md#6-inputassembler-cleanup),
this bridge remains runtime-owned, with a clearer name to be selected. It maps
owner fields to runtime channels and source/actor attribution, owns input
lifetime and reports unmapped fields. External device/network clock and actor
normalization, protocol decoding and generic channel normalization stay with
their owners. Supplying runtime clock metadata is distinct from normalizing
an external source clock.

The runtime bridge binds source actors to avatar instances, selects a declared
input snapshot, and delegates generic sampling/blending to motion evaluators.
Device decoding, source normalization and connector buffering remain upstream.
Non-blocking polling does not grant a connector its own avatar update loop.

If inputs overlap, source priority/blend policy and clock alignment must be
configuration data. Never use arrival order as an undeclared arbitration rule.
Any chosen stale-input/drop policy records the source, affected channels and
original timestamp in diagnostics. Capture must preserve the selection.

Input views stay valid through evaluation. Borrowing/retention rules are the
[ABI proposal](ABI.md)'s, including asynchronous intake handoff.

## 4. Semantic intent and absence

Examples such as `blink`, `smile`, `mouthOpen`, `jawOpen`, `angry`, `happy`,
`sad` and `surprised` are candidate semantic intents, not a frozen vocabulary
or VRM expression/MMD morph names. The bound format adapter resolves an intent
using explicit mapping data; unmapped intents produce diagnostics.

Preserve native/custom channels with namespaced identity where needed, and
make any conversion into common intent explicit. Do not silently equate a
similarly named native channel with a common intent. Promotion into a shared
vocabulary needs evidence across formats (`RT-O2`).

Absent data differs from a reported zero or identity. Validity information
must survive assembly. Generic motion hold/interpolation behavior remains its
owner's; clearing, holding or defaulting other channels must be a declared
binding policy. Validate numeric inputs without silently inventing observations.

The prototype requires an explicit nonempty namespace on scalar channel IDs
(for example `intent:smile`, `vrm:customName` or `app:speed`). These names do not
freeze a semantic vocabulary or imply native/intent conversion. Missing entries
mean absent; a zero-valued entry remains present. Duplicate source/actor/channel
triples and non-finite values are rejected. Different sources may report the
same channel: arbitration belongs in explicit provider binding configuration,
not array/arrival order. Inputs are already-selected immutable observations;
there is no separate invalid/stale-observation flag for scalars. Gaze validity
is explicit under the revision-3 rules above.
