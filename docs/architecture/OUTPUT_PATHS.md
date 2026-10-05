---
status: proposed
owner: usd-avatar-runtime
---

# Evaluated-state publication

This document develops [design policy sections 11–12](../design/DESIGN_POLICY.md).
All paths consume one [evaluated state](../contracts/EVALUATED_STATE.md).

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
implemented and tested. Renderer binding, consumer capability negotiation and
Hydra/direct parity remain unimplemented; this snapshot API alone does not
complete Runtime Phase C.

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

## 3. Recording, baking and export

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

Also test value-only updates, structural rebinding, absent/zero channels and
retained output lifetime. Parity criteria specify numeric tolerances per value
kind and quaternion sign equivalence; they must not hide missing channels.
Reuse motion-owner comparison rules for motion values.

Equivalent state does not by itself prove equivalent pixels or equal latency.
Renderer evidence must state backend, configuration and hardware separately.
