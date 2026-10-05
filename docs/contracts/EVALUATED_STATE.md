---
status: proposed
owner: usd-avatar-runtime
---

# Evaluated avatar state

`EvaluatedAvatarState` is the logical resolved result for one avatar and frame.
It is the common boundary for Hydra, direct consumers and capture, following
[design policy section 6](../design/DESIGN_POLICY.md).

## 1. State groups

| Group | Resolved effects |
| --- | --- |
| pose | evaluated transforms/root placement in an explicitly bound skeleton layout |
| deformation | blend-shape weights and typed additional deformation channels |
| appearance | typed material input overrides and visibility |
| identity | instance, frame, evaluation instant, binding generation and channel layouts |

The conceptual `AvatarPoseState { MotionPose pose; }` in the policy is not a
decision that semantic humanoid pose alone can represent every final rig.
The output must retain non-humanoid/twist/auxiliary joints, translations and
scales required by a bound avatar. Reuse motion-owner rig/pose results where
they fit; specify an explicit final-layout view where they do not (`RT-O4`).

Target identifiers must distinguish skeletons, joints, meshes, morph targets,
materials and visibility targets without exposing renderer resource handles.
Binding changes invalidate the layout explicitly. Sparse/dense representation,
units, transform space and update semantics must be chosen before ABI freeze.

## 2. Semantic convergence

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

Validation covers finite transforms/values, channel type/shape, target layout
and version compatibility. Unsupported effects are diagnosed via
[capability negotiation](CAPABILITIES_AND_DIAGNOSTICS.md), rather than dropped
silently by an output adapter. Output view retention is the [ABI](ABI.md)'s.
