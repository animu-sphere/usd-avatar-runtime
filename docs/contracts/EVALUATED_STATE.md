---
status: proposed
owner: usd-avatar-runtime
---

# Evaluated avatar state

`EvaluatedAvatarState` is the logical resolved result for one avatar and frame.
It is the common boundary for Hydra, direct consumers and capture, following
[design policy section 6](../design/DESIGN_POLICY.md).

Experimental [`state.h`](../../include/avatarRuntime/state.h) implements a
complete snapshot subset: parent-local rig transforms, blend-shape weights,
typed material input overrides and visibility. Other typed deformation outputs
and real-provider layout conformance remain open before freeze.

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

Revision 1 binds a dense array of all rig joints, including auxiliary joints.
Each carries `(skeleton_id, joint_id)`, a parent index (`-1` or an earlier joint
in the same skeleton), translation, rotation and scale. Translations are metres;
values use the canonical motion basis, right-handed +Y up/+Z forward. Rotations
are `double` unit quaternions in **x,y,z,w** order, with squared-norm error at
most `1e-6`. Each transform is parent-local; roots are runtime-world. Avatar
placement must be included by the binding adapter in the root transform. The
runtime does not retarget or convert basis/quaternion ordering itself.

Blend shapes use `(mesh_id, target_id)`; material values use
`(material_id, input_id)` with scalar, vec3 or vec4 type fixed by the layout;
visibility uses a target ID and a 0/1 value. All identities are copied opaque
UTF-8 strings, independent of renderer resources. Weights/transforms must be
finite; weights are not silently clamped. Material values use four components,
with unused components zero, and explicit `overridden=0/1`. Material meaning
and units remain with the canonical input's owner. Duplicate identities,
invalid parent order and invalid typed values prevent activation/publication.

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

Validation covers finite transforms/values, channel type/shape, target layout
and version compatibility. Unsupported effects are diagnosed via
[capability negotiation](CAPABILITIES_AND_DIAGNOSTICS.md), rather than dropped
silently by an output adapter. Output view retention is the [ABI](ABI.md)'s.
