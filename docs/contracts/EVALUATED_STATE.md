---
status: proposed
owner: usd-avatar-runtime
---

# Evaluated avatar state

`EvaluatedAvatarState` is the logical resolved result for one avatar and frame.
It is the common boundary for Hydra, direct consumers and capture, following
[design policy section 6](../design/DESIGN_POLICY.md).
The adopted [near-term direction](../design/NEAR_TERM_PLAN.md#4-central-evaluated-state-ir)
positions it as the central runtime IR, validated first by real VRM evaluators
and the `hydra-toon` fast-path, then by MMD before freeze.

Experimental [`state.h`](../../include/avatarRuntime/state.h) implements a
complete snapshot subset: parent-local rig transforms, blend-shape weights,
typed material input overrides and visibility. Other typed deformation outputs
and real-provider layout conformance remain open before freeze. Revision 2 introduced
explicit layout/version identity, host input revision and retained active
capabilities, preserved in revision 4. Revision 4 adds a two-component material
value type and retained [source-sample provenance](#source-sample-provenance).
Typed gaze input does not add resolved gaze output fields. Expression/gaze
result records and snapshot-retained diagnostics remain target requirements
below, not fields already present in `ArStateView`.

The optional [VRM adapter](../architecture/VRM_ADAPTER.md) now writes owner
expression/LookAt effects into the existing morph and canonical material
channels, and bone LookAt into parent-local eye rotations using explicit
owner-to-runtime joint/rest bindings. Constructed-rig tests establish direct-owner numeric parity and
snapshot behavior; real-avatar layout conformance and typed resolved
expression/gaze records remain open.

## 1. State groups

| Group | Resolved effects |
| --- | --- |
| pose | evaluated transforms/root placement in an explicitly bound skeleton layout |
| deformation | blend-shape weights and typed additional deformation channels |
| expression | resolved semantic/native/custom identity, weight, arbitration result and availability with source/provenance |
| gaze / LookAt | resolved eye rotation, head contribution, expression contribution and clamped/rejected/unavailable status |
| appearance | typed material input overrides and visibility |
| identity | instance, frame, evaluation instant, binding generation, layout/version, source/input revision and active capability set |
| diagnostics / provenance | evaluation status and provider/source context associated with the same frame |

The conceptual `AvatarPoseState { MotionPose pose; }` in the policy is not a
decision that semantic humanoid pose alone can represent every final rig.
The output must retain non-humanoid/twist/auxiliary joints, translations and
scales required by a bound avatar. Reuse motion-owner rig/pose results where
they fit; specify an explicit final-layout view where they do not (`RT-O4`).

Target identifiers must distinguish skeletons, joints, meshes, morph targets,
materials and visibility targets without exposing renderer resource handles.
Binding changes invalidate the layout explicitly. Sparse/dense representation,
units, transform space and update semantics must be chosen before ABI freeze.
Contract review must explicitly distinguish local/model/world transforms,
missing joints from identity transforms, and skeleton layout/version changes
from configuration/reset generations. The first ABI prioritizes clear
semantics over internal efficiency; the scoped prototype choice below remains
subject to real-provider conformance.

Revision 4 binds a dense array of all rig joints, including auxiliary joints.
Each carries `(skeleton_id, joint_id)`, a parent index (`-1` or an earlier joint
in the same skeleton), translation, rotation and scale. Translations are metres;
values use the canonical motion basis, right-handed +Y up/+Z forward. Rotations
are `double` unit quaternions in **x,y,z,w** order, with squared-norm error at
most `1e-6`. Each transform is parent-local; roots are runtime-world. Avatar
placement must be included by the binding adapter in the root transform. The
runtime does not retarget or convert basis/quaternion ordering itself.

The optional [motion adapter](../architecture/MOTION_ADAPTER.md) marshals owner
retarget translations/rotations and float rest scale into this dense layout.
Explicit owner-slot/runtime-joint and parent matching is tested with constructed
rigs, including undriven nonidentity rest. The optional
[USD skeleton binding](../architecture/USD_BINDING.md) preserves authored
joint layout, converts rest translations to metres and folds rigid skeleton
placement into baseline roots. Motion applies the same explicit placement
once after owner retargeting. Constructed USD composition is tested; actual
The optional [VRM USD binding](../architecture/VRM_USD_BINDING.md) supplies
schema-derived Humanoid mappings with one local real-avatar skeleton result.
Complete real-avatar expression/LookAt/output conformance remains open under RT-O4.

Blend shapes use `(mesh_id, target_id)`; material values use
`(material_id, input_id)` with scalar, vec2, vec3 or vec4 type fixed by the layout;
visibility uses a target ID and a 0/1 value. All identities are copied opaque
UTF-8 strings, independent of renderer resources. Weights/transforms must be
finite; weights are not silently clamped. Material values use four components,
with unused components zero, and explicit `overridden=0/1`. Material meaning
and units remain with the canonical input's owner. A two-component canonical
input, such as a texture offset or scale, is published as `AR_VALUE_VEC2`;
vec3 with zero `z` is not an alternative encoding, so producers and consumers
cannot validate different shapes for the same input. Duplicate identities,
invalid parent order and invalid typed values prevent activation/publication.

## 2. Semantic convergence

The adopted [boundary policy section 14](../design/BOUNDARY_POLICY.md#14-common-runtime-state)
requires format-independent resolved channels. Do not add core fields such as
`vrmExpressionHappy`, `mmdBoneMorph` or `pmxMaterialMorph`; adapters convert
those semantics into common pose, morph and material values. Namespaced format
identity may survive as provenance without requiring consumers to evaluate it.
Custom extensible named values are a target concept, not a revision-4 field.

```text
VRM expressions -> resolved blend-shape weights + material overrides
VRM LookAt      -> eye transforms and/or expression contributions
MMD morphs      -> resolved deformation + bone effects + material overrides
                     |
             common consolidated state
```

Format evaluators own interpretation, including expression arbitration and
MMD control rules. The runtime manages ordering and aggregation. Consumers
receive effects, not unresolved expression/morph names that require them to
evaluate the format again.

Bone effects must be consolidated into final pose once. An expression-based
LookAt contribution goes through the format's expression resolver once;
publication must not add it a second time.

Expression output cannot be only an unattributed float array. Its target
contract distinguishes common semantic identity from native/custom identity,
weight, source/provenance, arbitration outcome and availability (RT-O2/RT-O4).
It should accommodate VRM presets, MMD morphs and application facial channels
through owner mappings, without copying format semantics into core. Resolved
expression records expose evaluation results; renderers consume their resolved
deformation/appearance effects without applying arbitration again.

Gaze input is owned by the [input contract](INPUT_FRAME.md). Output describes
resolved eye rotations, head and expression contributions, and whether the
request was clamped, rejected or unavailable. VRM LookAt remains an owner
evaluator; the runtime supplies input, order, state and publication. A status
record must not instruct a consumer to perform LookAt itself. The representation
and relation to consolidated pose/expression channels remain RT-O4 work.

Material overrides reference canonical, typed material input identities;
the renderer realizes those values in its own material system. An output
channel is not a common shading model or a new USD toon schema. Additional
deformation channels require a typed contract and consumer negotiation, not
opaque backend-specific payloads.

## 3. Snapshot rules

Publication exposes an immutable, internally consistent frame snapshot.
Pose, deformation and appearance share the same frame and binding generation.
Do not hand a consumer a working state halfway through the phase plan.

Absence, unchanged values, explicit zero and removal of an override need
distinct documented behavior. An initial implementation must choose whether
published views are complete snapshots or deltas with a base-frame identity;
consumers cannot infer that choice from array length (`RT-O4`).

The prototype publishes **complete snapshots**, never deltas. Every frame's
working values start from the copied authored baseline, not the last output.
A provider can explicitly read its prior successful state to implement a
declared hold/filter policy. An unwritten channel therefore returns to baseline;
it does not accidentally hold the previous frame. Layout entries never vanish
mid-frame. For materials, `overridden=0` removes an override; a present override
whose value is zero is distinct. Untouched material entries return to the
baseline's override state. Unsupported material kinds/deformation extensions
are rejected rather than carried as opaque payloads.

Snapshots carry instance/frame/generation/time for all state groups. Reset
increases the generation without changing layout; structural rebinding currently
requires a new instance. Old retained snapshots keep their original identity
and values through subsequent frames/reset/destruction. Snapshot lifetime is
defined in [ABI section 2](ABI.md#2-lifetime-and-calls).

Revision 4 requires the binding host to supply a nonempty opaque UTF-8
`ArInstanceDesc.layout_id` and nonzero `layout_version`. The runtime copies these
and exposes them on every working/prior/published view, even when the evaluator
has no state domains. The tuple identifies channel identities/order, joint
parents, value types and structural binding mappings; changing those
requires a new version or ID and currently a new instance. The host owns that
identity assignment. Runtime validation does not detect a host reusing the same
tuple for different layouts. Consumers must also bind the instance identity;
matching tuples across instances are not permission to mix their values.

Reset changes configuration generation while preserving layout identity/version
and active capabilities. Value-only updates preserve the tuple. Initial-state
metadata is ignored: instance descriptor metadata and negotiated capabilities
are authoritative. A zero input revision means the host supplied no revision;
otherwise snapshots echo the selected input's revision as defined by the
[input contract](INPUT_FRAME.md#1-logical-contents).

Active capabilities are sorted by ID and immutable. Their array and strings,
and the layout ID string, remain valid until the last snapshot reference is
released, including through reset and instance/runtime destruction. Metadata
storage is shared immutably across frames; it holds no provider-private pointers.
Diagnostic records still use callback-scoped delivery and are not retained in
the snapshot. Real-provider layout conformance, expression/gaze results and
mapping/arbitration provenance remain RT-O4/RT-O7 work before freeze.

### Source-sample provenance

Revision 4 retains source time with each published snapshot, so a consumer can
report the age of each contributing sample without keeping per-input times
keyed by `input_revision`. The runtime reads no clock and measures no latency;
it copies host- or provider-supplied times and applies their declared affine
mapping. `ArStateView.samples` holds one `ArSourceSample` per record:

- Every selected scalar and gaze input is copied in input order, scalars first,
  with `resolution = AR_SAMPLE_SELECTED`, its source/actor/channel identity,
  validity (`AR_OBSERVATION_VALID` for scalars), source seconds and clock
  mapping. Absent inputs leave no record. A record states that the host
  selected the input for the frame, not that a provider consumed it.
- A provider that resolves its own sample, such as the
  [clip pose adapter](../architecture/MOTION_ADAPTER.md), reports it through
  `ArStateWriter.report_sample` as `AR_SOURCE_POSE` with
  `AR_SAMPLE_INTERPOLATED`, `AR_SAMPLE_HELD` or `AR_SAMPLE_EXTRAPOLATED`. A
  report needs the pose write domain, valid identities with a namespaced
  channel that is unique among the snapshot's records, `AR_OBSERVATION_VALID`
  and a valid clock mapping; otherwise the frame fails. The runtime stamps the
  reporting evaluator and runtime seconds. Reports follow the inputs in plan
  order, and a failed frame discards them with the rest of the working state.
- `source_seconds` is the time of the sample content, and
  `runtime_seconds = source_seconds * clock_scale + clock_offset`. A held value
  keeps its original time: a host holding a scalar or marking a gaze stale
  supplies the observation's original source seconds, and a provider holding a
  boundary sample reports that sample's time rather than the request. An
  extrapolating provider reports the newest observed sample. Consumers derive
  age from `runtime_seconds`; `evaluation_seconds` remains the evaluated
  instant, not a production timestamp.
- Records and their strings share the snapshot's lifetime. Published and prior
  views carry them; working views passed to providers do not, because reports
  may append during a callback. Initial-state sample fields are ignored, and
  array order is not arbitration.

Mapping a producer's own clock to runtime seconds stays with the host and its
connectors; a source without its own clock is stamped at receipt by its owner.
`input_revision` still identifies the selected observation set.

Validation covers finite transforms/values, channel type/shape, target layout
and version compatibility. Unsupported effects are diagnosed via
[capability negotiation](CAPABILITIES_AND_DIAGNOSTICS.md), rather than dropped
silently by an output adapter. Output view retention is the [ABI](ABI.md)'s.
