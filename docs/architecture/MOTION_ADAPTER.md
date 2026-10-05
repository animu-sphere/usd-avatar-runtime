---
status: binding
owner: usd-avatar-runtime
---

# Scoped motion adapters

The optional `avatarMotionAdapter` connects installed `motionSampling` and
`motionRetarget` owner libraries to the revision-3 runtime. It validates a
scoped RT-O3/RT-O4 path: immutable clip -> owner sampling -> explicit humanoid
retarget map -> dense runtime rig pose -> ordered VRM LookAt/Expression.
Constructed bindings do not establish real-avatar or milestone acceptance.
The same target also supplies host-side `InputAssembler` for selected owner
scalar channels and world gaze points, without changing the runtime C ABI.

## Build and installation

Enable `AVATAR_BUILD_MOTION_ADAPTER` and provide installed `motionSampling`,
`motionRetarget` (0.5.3 or compatible later version), `motionCore` and OpenUSD
CMake packages. The adapter links only owner value libraries; core still has
no provider dependency. No sibling sources are compiled into the runtime.
In an x64 developer shell with dependency DLLs on `PATH`:

```sh
cmake -S . -B build/motion -G Ninja -DCMAKE_BUILD_TYPE=Release -DAVATAR_BUILD_MOTION_ADAPTER=ON -DCMAKE_PREFIX_PATH="<motion-install>;<usd-install>"
cmake --build build/motion
ctest --test-dir build/motion --output-on-failure
cmake --install build/motion --prefix install/motion
```

Enable `AVATAR_BUILD_VRM_ADAPTER` as well to run the constructed motion-to-VRM
test. The VRM install requirements remain in [its adapter page](VRM_ADAPTER.md).
Consumers request the installed component explicitly:

```cmake
find_package(AvatarRuntime 0.1.0 EXACT CONFIG REQUIRED COMPONENTS motion)
target_link_libraries(host PRIVATE AvatarRuntime::avatarMotionAdapter)
```

`COMPONENTS motion vrm` imports both adapters. A core-only package lookup
does not resolve either provider family. Adapter configuration is a C++ API
above the unchanged C ABI, without a cross-toolchain C++ ABI guarantee.

## Configuration and lifetime

[`ClipPoseAdapterConfig`](../../adapters/motion/include/avatarMotion/ClipPoseAdapter.h)
copies an owner `MotionClip`, `SkeletonDescriptor`, `RetargetMap`, source/reference
rest and `RetargetOptions`, plus evaluator/layout/skeleton identity, a runtime
joint ID for every owner joint slot, explicit clock mapping and predecessors.
The host supplies the bindings, optionally from the
[USD skeleton binding](USD_BINDING.md); this evaluator does not read a USD stage or
infer humanoid roles from joint names. A VRM host supplies its owner's
`vrmRig::GetRequiredBones()` through `RetargetOptions::requiredBones`; the
motion adapter contains no VRM rule or dependency.

Configuration rejects incomplete/duplicate target identities, invalid parents,
out-of-range/duplicate map targets, invalid/cyclic source-rest ancestry,
non-finite values, non-unit driven/rest rotations and unsorted or overflowing
clip interpolation intervals before constructing the reusable retargeter.
The clip may be empty. Missing required bones remain recoverable owner
diagnostics, preserving the owner's partial-skeleton policy.

Keep the adapter alive until runtime destruction: registration borrows its
immutable `user_data`. Multiple instances share configuration without sharing
frame state. Different clips/rigs require different adapter objects and IDs.
No source polling, file loading, stage authoring or renderer call occurs during
evaluation. Immutable clips make retry/reset independent of a source cursor.

## Evaluation and output mapping

One atomic callback in `AR_PHASE_RETARGET` declares pose reads/writes and calls
`SampleClip`, then `PoseRetargeter::Retarget`. This avoids a runtime-owned
copy of interpolation, root policy or rest correction. Add the motion evaluator
ID to the VRM adapter's `after` list so gaze reads the resolved working head.

The explicit mapping is:

```text
runtime_seconds = clip_seconds * clockScale + clockOffset
clip_seconds = (ArInputFrame.evaluation_seconds - clockOffset) / clockScale
```

Scale must be positive and finite; offset and the mapped request must be finite.
Snapshot evaluation time and input revision continue to describe the host frame,
not the clip's source time. The clip/configuration is a bound source rather than
a new `ArInputFrame` array. Hosts must capture that configuration for replay;
full motion observation/provenance transport remains open under RT-O1/RT-O2.

Every callback verifies layout ID/version, all mapped runtime joints and exact
owner/runtime parent relationships. Owner slot order need not equal runtime
slot order. Root joints must be runtime-world roots; an extra avatar-placement
parent is rejected. `rootPlacement` supplies a finite rigid skeleton-to-world
transform with a unit quaternion and identity scale. After owner retargeting,
each root translation is rotated and translated by that placement, and each
root rotation is premultiplied and normalized. Descendants and owner rest
correction remain skeleton-local. Identity placement is the default. An empty
clip leaves the already placed authored baseline untouched; placement is never
accumulated across frames. USD binding supplies this placement explicitly.

The owner retarget replaces translation/rotation for every mapped joint and
carries the owner's float rest scale into the double runtime transform without
half narrowing. Undriven joints keep owner rest, including nonidentity rotation
and nonunit scale. Unrelated runtime joints are untouched. The common writer
validates the output, and failed frames publish no partial state.

Empty clips contribute no pose and emit `MOTION_ADAPTER_UNAVAILABLE`. Requests
outside the clip range use the owner's boundary hold and emit
`MOTION_ADAPTER_HELD`; this is finished-clip behavior, not a live stale-input
policy. Owner retarget codes, subjects, detail and severity are preserved with
provider origin `usd-motion-plugins.motionRetarget`; the runtime stamps the
instance/frame/evaluator/phase. Descriptor version records both owner package
versions. The scoped capability is `avatar.motion.clipPose` version 1.

The clip callback publishes pose only. Sampled scalar channels and gaze points emit
`MOTION_ADAPTER_CHANNELS_UNSUPPORTED` / `MOTION_ADAPTER_GAZE_UNSUPPORTED` and
require the separate host input mapping below. These diagnostics describe the
pose callback's scope, even when the host has assembled those fields.
Confidence/contact metadata is not
published or used to invent a new gating rule. Motion blending, live connector
intake, USD/Humanoid binding discovery and typed resolved provenance are
still pending.

## Host input assembly

[`InputAssembler`](../../adapters/motion/include/avatarMotion/InputAssembler.h)
maps an already selected owner `MotionPose` into an owned revision-3 input
frame. It does not sample, poll a connector, retarget or perform format
arbitration. Supply explicit source/actor identity, positive affine clock
mapping, native-channel-to-namespaced-runtime-channel bindings and an optional
gaze channel. Channel names are verbatim owner names; there is no automatic
native-to-intent or native-to-VRM conversion. Configure the VRM adapter's
`inputs` and `gaze` selections against those same runtime identities.

`Assemble(pose, context, gazeValidity)` copies frame identity, generation,
evaluation seconds, USD mapping and input revision from `context`. Its input
arrays must be empty; existing inputs are rejected rather than overwritten or
implicitly merged. The host retains responsibility for multi-source selection
and composition. A null pose yields empty arrays. Only reported mapped
channels are emitted, including explicit zero and unclamped weights.
`lookAtTarget` maps to a runtime-world **point**, preserving origin as a valid
target and optional absence. No direction, joint reference, space conversion
or placement adjustment is inferred; the host must supply values in the
runtime's canonical world basis.

Each observation retains `MotionPose.timestamp`, `clockScale` and
`clockOffset`; evaluation time remains independent. This preserves the
timestamp returned by the owner, which may already be restamped by
`SampleClip`/`ClipSource`. It does not reconstruct original key/arrival times
from sample status or metadata. When combined with `ClipPoseAdapter`, use the
same selected clip and clock mapping for host sampling and pose configuration.

Present gaze defaults to `AR_OBSERVATION_VALID`; the host may explicitly
select `AR_OBSERVATION_STALE` to retain a point while suppressing VRM LookAt.
Held/extrapolated status and lag do not implicitly classify validity. A null
pose or missing point is absent, not an invented unavailable observation.
Scalar validity is not representable in revision 3; hosts must explicitly
select/drop scalar inputs according to their source policy.

Configuration rejects duplicate native mappings, scalar/gaze identity
collisions, embedded NULs, missing namespaces and invalid clocks. Assembly
rejects invalid context/clock numerics, malformed owner channel sets and
non-finite channel/gaze payloads before returning any frame. Unmapped reported
channels are available through `UnmappedChannels()` and an unbound point through
`HasUnmappedGaze()` so the host can diagnose its selection. The helper emits no
evaluator diagnostics and registers no capability; it runs before evaluation.

`MotionInputFrame` owns all arrays/strings, and copies share immutable storage.
Its `View()` can be borrowed synchronously by `evaluate_frame` while any frame
copy remains alive, independently of the source pose/configuration/assembler.
Later assemblies do not alter earlier views. Invalid assembly throws
`invalid_argument` and advances no cursor/provider state. There is no
cross-toolchain C++ ABI guarantee or asynchronous runtime retention.

## Evidence and limits

[`tests.cpp`](../../adapters/motion/tests.cpp) compares runtime transforms with
independent owner sampling/retarget calls at `1e-6`, including interpolation,
clock scale/offset, root translation, undriven rest and reordered runtime slots.
It covers held/empty clips, binding failures, source diagnostics, same-frame
retry, downstream failure rollback, reset, instance isolation and retained
snapshots past runtime destruction. The installed-consumer test repeats the
checks using exported targets and installed headers/libraries.
Input assembly tests cover owned/copy lifetime, origin/absence/zero, unclamped
weights, explicit identity/clock/USD metadata, stale gaze, unmapped-field
reporting, malformed bindings/values/context rejection and corrected retry.

When VRM is enabled, the test supplies the owner's required-bone set and orders
motion -> LookAt -> Expression despite reversed evaluator selection. It compares
the resolved morphs with owner sampling/retarget/world-head/LookAt/resolver
calls, using assembled motion scalar/gaze input. Tests cover gaze precedence
over mapped look expressions, host-selected stale held gaze with usable
scalars, absent input restoring the authored expression baseline, and an
explicit zero clearing that baseline after reset.
The partial rig intentionally emits missing-required-bone diagnostics.
This proves owner-library composition with constructed input/bindings; it does
not prove real clip/VRM asset correctness, connector intake/mapping, full Humanoid
conformance, renderer output or ABI freeze. Package/toolchain evidence is in
the [capability matrix](../reference/CAPABILITY_MATRIX.md).

The separate [USD motion clip binding](MOTION_USD_BINDING.md) now connects
actual owner-imported clips and source rest to this adapter. Its opt-in tool
validates seven real clips on one real-avatar schema-derived Humanoid layout;
this extends pose integration evidence without closing real-avatar
LookAt/Expression, connectors, rendering or milestone acceptance.
