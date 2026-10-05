---
status: binding
owner: usd-avatar-runtime
---

# Scoped VRM registration adapter

The optional `avatarVrmAdapter` target connects the installed
[`vrmRig` owner library](https://github.com/animu-sphere/usd-vrm-plugins/tree/main/libs/vrmRig)
to the experimental revision-3 runtime. This document owns the implemented
adapter boundary. It does not establish real-avatar or milestone acceptance.

## Build and installation

The default `avatarRuntime` target has no provider dependencies. Enable
`AVATAR_BUILD_VRM_ADAPTER` to resolve `vrmRig` using its installed CMake package;
its transitive dependencies include `motionCore` and OpenUSD value libraries.
No sibling source directory is compiled into this runtime.
The owner install must include the additive `EvaluateDirection` API advertised
by `VRMRIG_LOOKAT_DIRECTION_API`. Older headers fail with an explicit adapter
build diagnostic; rebuild and install the owner before configuring this adapter.
This feature requirement is not a new published owner package version.

In an x64 developer shell with the owner's dependency DLLs on `PATH`:

```sh
cmake -S . -B build/vrm -G Ninja -DCMAKE_BUILD_TYPE=Release -DAVATAR_BUILD_VRM_ADAPTER=ON -DCMAKE_PREFIX_PATH="<vrmRig-install>;<motionCore-install>;<usd-install>"
cmake --build build/vrm
ctest --test-dir build/vrm --output-on-failure
cmake --install build/vrm --prefix install/vrm
```

Consumers request the optional installed component explicitly:

```cmake
find_package(AvatarRuntime 0.1.0 EXACT CONFIG REQUIRED COMPONENTS vrm)
target_link_libraries(host PRIVATE AvatarRuntime::avatarVrmAdapter)
```

Finding the core package without `COMPONENTS vrm` does not resolve owner
dependencies. The adapter has a C++ configuration API above the common C ABI;
this is not a new cross-toolchain C++ ABI guarantee.

## Configuration and lifetime

[`ExpressionAdapterConfig`](../../adapters/vrm/include/avatarVrm/ExpressionAdapter.h)
copies the owner's `ExpressionRig`, optional expression- or bone-type `LookAtRig`,
layout ID/version, evaluator ID, input selections and output bindings.
The host currently supplies these values. The optional
[USD skeleton binding](USD_BINDING.md) supplies authored joint/rest and
placement values. Separate [VRM Humanoid](VRM_USD_BINDING.md) and
[LookAt](VRM_LOOKAT_USD_BINDING.md) binders supply schema-derived configuration.
The separate [Expression binding](VRM_EXPRESSION_USD_BINDING.md) supplies
owner expression definitions and canonical output baseline/identities.
`Descriptor()` supplies the registration table. Keep the adapter alive until
runtime destruction, because registration borrows its immutable `user_data`.
Multiple instances can share that configuration without sharing frame state.
Different avatar bindings require different adapter objects/evaluator IDs.

With the optional [motion clip pose adapter](MOTION_ADAPTER.md), supply the
motion evaluator ID in `after`. The constructed integration test proves that
LookAt reads its retargeted working head and Expression resolves the resulting
contributions once. The motion target's host input assembler supplies explicit
scalar/gaze identities from a selected owner pose; tests cover LookAt
precedence over mapped scalars and host-selected stale gaze. USD Humanoid
discovery and real-avatar conformance remain open.

Each input mapping selects exactly `(source, actor, channel)` and names the
owner expression verbatim. The adapter does not invent a semantic vocabulary.
Duplicate selections or multiple mappings to one expression are rejected at
construction. Absence contributes nothing; an explicit scalar zero reaches
the owner resolver and writes its affected targets at zero. Finite double
weights exceeding float range saturate during marshalling, then the owner
applies its own clamp and diagnostic.

Morph bindings map opaque owner targets to `(mesh_id, target_id)`. Material
bindings use the owner's canonical color-slot table, including separate RGB
and alpha inputs where specified. Authored baseline material values must be
present in `ArMaterialInput.value` even with `overridden=0`; the adapter reads
the current working values as the owner resolver's base. Earlier material
writes therefore participate through explicit scheduler ordering.

Every frame verifies layout identity/version and all declared rig effect
bindings, including material input types, before owner evaluation. Missing
targets and unsupported slots fail publication rather than dropping effects.
The host remains responsible for assigning honest layout identities.

Bone rigs additionally supply `EyeBinding` records mapping each named owner
eye to `(skeleton_id, joint_id)` and an authored parent-local rest quaternion
in x,y,z,w order. Rest rotations must be finite and unit length within the
runtime's squared-norm tolerance of `1e-6`. Each named eye requires exactly
one mapping; duplicate owner or runtime identities, extra mappings, head
targets and cross-skeleton mappings are rejected. At least one eye is required;
one-eye rigs preserve the owner's missing-eye warning. Eye bindings are invalid
without bone LookAt. Every frame validates the head and eye layout even when
gaze is absent. Eyes must be direct children of the head, matching the owner's
head-space rotation boundary; other parent spaces fail visibly.

## Execution and supported gaze

One stateless callback in `AR_PHASE_EXPRESSIONS` executes the scoped sequence:

```text
selected gaze -> owner LookAt -> named contributions
                                      |
selected scalars ----------------------+
                                      |
                         owner ExpressionResolver
                                      |
                      common morph/material writes
```

The callback reads material values and, when LookAt is enabled, pose. It writes
deformation/material domains and, for bone rigs, pose. Bone LookAt returns eye
rotations instead of expression contributions. Each replaces the working eye
rotation with normalized `resolved gaze * bound authored rest`, matching the
owner's bake caller. Translation and scale retain their current working values.
Earlier animated eye rotations are replaced rather than multiplied into gaze;
there is no accumulation or prior-frame hold. Earlier pose phases precede it;
`after` names additional required predecessors. LookAt contributions are resolved
exactly once, with no intermediate state shared across frames. This is scoped RT-O3
evidence with both LookAt types; actual motion/MMD plans remain unvalidated.
Supplied capabilities are `avatar.vrm.expression.effects` version 1 plus
`avatar.vrm.lookAt.expression` or `avatar.vrm.lookAt.bone` version 1 for the
selected rig type. Bone rig registration's pose writes participate in the
runtime's existing writer dependency checks.

LookAt accepts valid selected **points and directions** in runtime-world or
explicitly bound joint-local space. It derives head/reference world transforms
from the current working parent-local rig using OpenUSD value operations, so
earlier pose writes participate. Local points include joint/root translation;
local directions use joint orientation only. The reference may name the head,
an eye or any other baseline rig joint; it names that joint's own frame.
Head and local-reference ancestry must have unit scale within `1e-6`.
Missing head/reference joints, scaled ancestry and positions/targets outside
finite owner float range fail visibly. Bone-type LookAt is rejected when its
eye binding is incomplete. These restrictions do not narrow the input contract.

Point inputs preserve the owner's eye-origin offset and target-distance rules.
Directions call the owner's `EvaluateDirection` entry point with a world unit
vector and head orientation. The owner applies its existing range maps and
bone/expression outputs without positional eye parallax or an inferred target
distance. Head/root translation and avatar/clip eye offsets do not affect a
direction; even placements outside float range remain usable. After core unit
validation, the adapter removes the permitted double norm error before float
marshalling to avoid rejection caused by rounding at the tolerance boundary.

Absent gaze produces no contribution. Stale/unavailable selected gaze is
diagnosed and contributes nothing; the complete snapshot returns unwritten
effects to baseline. It does not hold the previous gaze silently.
The owner's valid target-at-origin result also contributes no eye rotation and
forwards its warning. Unwritten eyes retain the authored baseline or an earlier
pose evaluator's values for this frame.

When valid LookAt reports a name also mapped by a scalar, the LookAt value
replaces that scalar and an informational precedence diagnostic identifies
the collision. The owner resolver subsequently applies the rig's expression
arbitration. Scalar look expressions remain usable when gaze is absent or
unavailable. This is an explicit adapter policy, not a core arbitration rule.

## Diagnostics and evidence limits

Diagnostics identify `usd-vrm-plugins.vrmRig` as origin; the runtime stamps the
evaluator, instance, frame and phase. Adapter codes report layout, target,
head/eye/eye-parent, reference-scale/range and availability failures. Owner unresolved,
clamped, suppressed and warning results are forwarded with their named subjects or
warning text. Provider failure returns `AR_PROVIDER_ERROR` at the runtime
boundary; the diagnostic retains the underlying adapter status.

The tests exercise installed owner algorithms with constructed rigs and test
input, numeric direct-owner parity at `1e-6`, isolation, reset, absence/zero,
rollback/retry and retained output lifetime. A separately configured installed
consumer repeats the boundary checks without source-tree include paths.
Bone tests cover asymmetric inner/outer maps, both yaw signs, rotated head
placement from an earlier evaluator, nonidentity rest rotations, preserved
translation/scale, repeated-frame stability, one-eye diagnostics, stale and
unavailable gaze, target-at-origin, invalid bindings, and rollback after eye
writes when later material marshalling fails. An unordered same-phase pose
writer is rejected as a write conflict at instance creation.
Mapped-gaze tests cover both rig types, world/root/head/rotated-eye references,
points versus directions, present/absent owner eye offsets, earlier head pose
writes, reference-scale failure and same-frame retry. Owner API tests separately
check direction yaw, outputs, invalid vectors and independence from placement,
eye offsets and the point-only distance threshold.
Versions and target evidence are recorded in the
[capability matrix](../reference/CAPABILITY_MATRIX.md).

Real VRM asset binding, Humanoid/motion/connector integration,
actual connector gaze mappings, resolved expression/gaze records and renderer
consumption remain in the [roadmap](../roadmap/current.md). No milestone,
ABI freeze, pixel parity or renderer support follows from these tests.
