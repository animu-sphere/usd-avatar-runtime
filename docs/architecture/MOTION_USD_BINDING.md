---
status: binding
owner: usd-avatar-runtime
---

# Scoped USD motion clip binding

The optional `avatarMotionUsdBinding` owns a host-side connection from an
already composed semantic motion stage to the existing
[clip pose adapter](MOTION_ADAPTER.md). It calls installed `motionUsd` to read
the clip and installed `motionRetarget` to build its source rest. File parsing,
sampling and retarget mathematics remain with their owners. Runtime core and
its revision-3 C ABI are unchanged.

## Build and use

Enable `AVATAR_BUILD_USD_BINDING` and `AVATAR_BUILD_MOTION_USD_BINDING`, with
installed `motionUsd`/`motionRetarget` 0.5.3, `motionCore` and OpenUSD packages.
This binding alone does not require `motionSampling`, `vrmRig` or `vrmSchema`.
Installed consumers request `COMPONENTS motion_usd` and link
`AvatarRuntime::avatarMotionUsdBinding`; the generic `usd` component resolves
transitively. Core-only, `usd` and `vrm_usd` lookups do not resolve `motionUsd`.

[`StageClip`](../../adapters/motion-usd/include/avatarMotionUsd/StageClip.h)
requires a stage and an explicit skeleton prim path. It validates the source
skeleton using the generic USD binder, reads its bound animation through the
owner and constructs `SourceRestPose` from the validated source skeleton.
Copies share immutable storage and survive stage changes/destruction.
`Read()` preserves the owner's clip, skeleton, animation identity, encoding
rate, optional metadata and warnings. No reader warning is suppressed.

Supply **both** `Read().clip` and `SourceRest()` to `ClipPoseAdapterConfig`.
Source rest is required to subtract the source hip height before adding root
movement to target rest, and to apply owner rest-rotation correction. Target
skeleton/map/joint identities/placement, clock mapping, root policy and
required bones remain host configuration. The motion adapter validates the
configured clip before evaluation. Scalar/gaze input assembly remains a
separate host responsibility.

The scoped source boundary accepts Y-up metre stages with identity skeleton
placement, authored rest and a finite positive time-code rate. It rejects
other units and placements rather than silently treating their translations
as canonical world metres. The generic binder checks rest/topology/TRS;
the owner rejects ambiguous semantic rest roles. Missing rest is rejected,
even when the reader could synthesize identity, because that would change
root-height interpretation. Failures throw `invalid_argument` with
`MOTION_USD_*` or delegated `USD_BINDING_*` codes and subjects. There is no
joint-name heuristic for target avatars, source polling, per-frame stage
authoring or new C++ cross-toolchain ABI guarantee.

## Opt-in asset parity tool

Enable `AVATAR_BUILD_MOTION_CHECK` together with motion USD binding, VRM USD
binding and the motion adapter. The installed `avatarMotionCheck` executable
accepts an avatar followed by one or more motion paths:

```sh
avatarMotionCheck <avatar.vrm> <motion.vrma> [other-motion.vrma ...]
```

The host must register matching installed schema/file-format resources and
put their dependency DLLs on its library path. The tool requires an avatar
default prim and exactly one motion skeleton; library callers select their
own explicit skeleton. When the VRM adapter is enabled, the tool also supplies
the owner's `GetRequiredBones()` set. Otherwise no required-bone set is claimed.

It releases both stages before evaluation, checks every imported key and
midpoint plus two boundary holds using an explicit affine clip/runtime clock,
and compares every target joint's TRS with separate owner sampling/retarget
calls at `1e-6`. It verifies snapshot layout/frame/input/capability identity,
observable pose changes, reset and retained snapshots after runtime destruction.
Diagnostics are counted by owner/code with subject names. Any failed comparison,
error diagnostic, malformed binding or unchanged avatar pose returns nonzero.
The tool never copies, installs or authors the input assets, and no local asset
is added to default CTest runs.

## Evidence and remaining scope

`adapters.motion_usd_clip` checks source height/rotation/ancestry, time-code to
seconds mapping, missing optional metadata, immutable copies, rejection of
missing rest, invalid units/axis/rate/placement, absent animation and duplicate
semantic roles. `adapters.motion_usd_installed` repeats these checks through
installed headers/targets with no source-tree include or format evaluator.

On 2026-10-05, seven privately supplied VRMA MotionPack clips were applied to
one private avatar with 128 joints and 51 schema-mapped roles: 4,129 source
samples, 8,272 evaluation frames and maximum TRS component error
`1.403972313e-7`. All seven changed 51 target joints and passed reset/retention.
Each reported unbound driven `upperChest`; the target has no authored binding
for that role, so the owner warning is preserved. Two held diagnostics per
clip correspond to the intentional outside-range checks. No error diagnostics
were reported. Asset hashes, commands and paths remain in ignored local build
evidence; neither clips nor derived motion dumps are included here.

This proves real motion -> real-avatar Humanoid pose -> retained evaluated
state composition, not independent correctness of parsing/retarget semantics.
Real-avatar LookAt/Expression/output binding extraction, live connectors,
source provenance in retained state, renderer output, milestones A/B/C and
ABI freeze remain open. See the [capability matrix](../reference/CAPABILITY_MATRIX.md)
and [roadmap](../roadmap/current.md).
