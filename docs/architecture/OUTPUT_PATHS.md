---
status: proposed
owner: usd-avatar-runtime
---

# Evaluated-state publication

This document develops [design policy sections 11–12](../design/DESIGN_POLICY.md).
All paths consume one [evaluated state](../contracts/EVALUATED_STATE.md).

The [near-term direction](../design/NEAR_TERM_PLAN.md#6-first-renderer-consumer)
selects `hydra-toon` direct/fast-path as the first renderer consumer. Connect it
before full Hydra publication to validate state semantics and measure latency,
copy cost and allocations without first freezing locators/Scene Index structure.
This direct consumer work feeds Runtime Phase A/B contract correction.

```text
                     +-- fast/direct API -> hydra-toon (first consumer)
EvaluatedAvatarState +-- Hydra adapter --> render delegates (later)
                     +-- recording/bake
```

Hydra and direct consumers share frame/state identity and evaluated values.
Hydra publication is transport, not a second evaluation of avatar semantics.

Under [boundary policy section 13](../design/BOUNDARY_POLICY.md#13-hydra-and-fast-path-boundary),
fast-path transport may use zero/minimal-copy views, dirty masks, late
publication and GPU-friendly packed views with frame/generation identity.
These are allowed optimizations, not current support claims; canonical values
and semantics remain in evaluated state. GPU resource management, shaders,
draw submission and late-latching implementation stay with the renderer.

## 1. Hydra

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
implemented and tested. The optional motion-check host now composes the
installed `Toon::AvatarState` consumer over an explicit probe scene as described
below. Actual resident-avatar binding, full consumer capability negotiation and
Hydra/direct parity remain open; this does not complete Runtime Phase C.

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

The initial retained-snapshot adapter updates resolved pose, morph/deformation,
expression effects, appearance and visibility. Gaze arrives as resolved
pose/expression effects and result metadata under the state contract. Consumers
do not interpret VRM LookAt, expression arbitration, MMD morph semantics, IK
or retargeting. Test layout change versus value-only update explicitly; a reset
generation alone must not imply changed topology. Measure latency, bytes/copies
and allocations with the actual input/state/layout and renderer configuration.
Actual avatar geometry/resource binding and rendering remain planned requirements.

### Scoped Toon transport check

`AVATAR_MOTION_CHECK_TOON` optionally links only `avatarMotionCheck` and its
regression host to an installed `Toon` package's `AvatarState` component.
It requires the VRM mode and the additive `BaseColorRgb` binding; a configure-time
header probe rejects older same-version renderer installations. Core and
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

Equivalent state does not by itself prove equivalent pixels or equal latency.
Renderer evidence must state backend, configuration and hardware separately.
