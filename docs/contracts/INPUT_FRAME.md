---
status: proposed
owner: usd-avatar-runtime
---

# Avatar input frame

`AvatarInputFrame` is the logical, immutable input assembled for one avatar
evaluation. It is renderer- and format-independent. This document develops
[design policy section 5](../design/DESIGN_POLICY.md); it defines no final struct.

The experimental [`input.h`](../../include/avatarRuntime/input.h) implements
frame identity/generation, evaluation seconds, optional USD time mapping and
source-attributed scalar channels. This subset is not a second motion model:
motion pose/clip values and gaze space descriptors are still unimplemented.
The full logical contract and RT-O1/RT-O2 remain open.

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
Exact time/space descriptors are `RT-O1` in the
[roadmap](../roadmap/current.md#open-decisions).

In revision 1, each scalar records its source/actor/channel identity, source
seconds and an explicit positive affine clock mapping:
`runtime_sample_seconds = source_seconds * clock_scale + clock_offset`.
The evaluation instant stays separate. When present, the USD mapping is
`usd_time_code = evaluation_seconds * usd_time_codes_per_second + usd_time_code_offset`.
Both mapped results must be finite. Missing mappings are not guessed. The
runtime validates/forwards this metadata; it does not sample, blend or reject
stale samples on behalf of a connector/motion provider.

## 3. Intake and ownership

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
there is no separate invalid/stale-observation flag in this subset.
