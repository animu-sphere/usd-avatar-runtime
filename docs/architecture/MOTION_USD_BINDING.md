---
status: binding
owner: usd-avatar-runtime
---

# Scoped USD motion clip binding

The optional `avatarMotionUsdBinding` owns a host-side connection from an
already composed semantic motion stage to the existing
[clip pose adapter](MOTION_ADAPTER.md). It calls installed `motionUsd` to read
the canonical clip, descriptor and source rest as one coherent owner result.
File parsing, sampling and retarget mathematics remain with their owners. Runtime core and
its revision-3 C ABI are unchanged.

## Scoped owner reading split

The adopted [boundary policy section 7](../design/BOUNDARY_POLICY.md#7-stageclip-cleanup)
phases out the current `StageClip` form. `StageClip` now calls the installed
`motionUsd::ReadCanonicalMotionStage`, which owns source skeleton reading and
generic units/axis/rate/placement validation. It no longer creates a runtime
`SkeletonBinding` for the source. The helper preserves the owner's
`MotionStageRead`, including its owner-built `descriptor` and `sourceRest`;
it contains no descriptor/source-rest rebuilding, USD motion conversion or
private motion result vocabulary. `SourceRest()` references the rest in `Read()`.

The owner's WS-O4 dependency decision is resolved and its typed results are
consumed here. This migration keeps `StageClip` source compatibility while
final wrapper absorption is settled in
[cleanup workstream B](../roadmap/current.md#boundary-cleanup-workstreams).

## Build and use

Enable `AVATAR_BUILD_USD_BINDING` and `AVATAR_BUILD_MOTION_USD_BINDING`, with
installed `motionUsd`/`motionRetarget` 0.5.4 or compatible later, `motionCore`
and OpenUSD packages. The typed strict reader ships in the owner's 0.5.4 release;
configuration requires that version and also verifies installed headers/linked symbols.
The `motion_usd` component also checks the strict reader's options overload;
the generic `usd` component does not require that additive input API.
This binding alone does not require `motionSampling`, `vrmRig` or `vrmSchema`.
Installed consumers request `COMPONENTS motion_usd` and link
`AvatarRuntime::avatarMotionUsdBinding`; the generic `usd` component resolves
transitively. The generic `usd`/`vrm_usd` bindings now resolve `motionUsd` for
owner skeleton reading, while importing no source-clip adapter or sampler.
Core-only lookups still resolve no provider package.

[`StageClip`](../../adapters/motion-usd/include/avatarMotionUsd/StageClip.h)
requires a stage and an explicit skeleton prim path. The strict owner reader
validates source skeleton and clip-space metadata and reads the bound animation.
The helper retains the owner-built `SourceRestPose` without runtime avatar/layout
identities for the source or a second rest copy.
Copies share immutable storage and survive stage changes/destruction.
Contract tests compare the retained descriptor and every source-rest array with
the direct strict owner result, verify the `SourceRest()` alias after stage
destruction, and preserve duplicate-bone refusal code/subject/detail.
The additive constructor accepts owner `MotionStageReadOptions` for selected
scalar name/value paths, prefixes and a gaze path. It forwards these to the
strict owner reader, preserving its refusals and diagnostics. Attribute
discovery remains format-owner/host configuration; no VRMA layout is inferred.
`Read()` preserves the owner's clip, skeleton, animation identity, encoding
rate, optional metadata, warnings and typed descriptor/source rest. No reader
warning is suppressed.

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
as canonical world metres. The owner reader checks rest/topology/TRS;
the owner rejects ambiguous semantic rest roles. Missing rest is rejected,
even when the reader could synthesize identity, because that would change
root-height interpretation. Owner reader refusals throw the shared
`avatarUsd::MotionUsdReadError`, retaining unmodified code/subject/detail and
installed owner version; existing `invalid_argument` handlers remain valid.
Semantic source-rest failures preserve owner diagnostics such as
`MOTION_USD_SOURCE_REST_DUPLICATE_BONE` and
`MOTION_USD_SOURCE_REST_NO_HUMAN_BONE`. There is no
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

With `AVATAR_BUILD_VRM_ADAPTER`, the tool also links the LookAt and Expression
USD bindings. `--vrm` extracts the complete output baseline, registers motion
and the atomic LookAt/Expression adapter, and deliberately selects the latter
first to verify dependency ordering. The host maps common motion channels
named `vrm:<verbatim avatar expression name>` to that expression; other channels
are reported as unmapped. This is a scoped explicit host policy, without aliases
or automatic VRM-version name conversion. Reader gaze points are in canonical
clip space. This host chooses the avatar's rigid root placement as clip
placement and applies it once before world-gaze assembly, without animated
hips, retarget height scaling or a source-rig LookAt offset. The same affine
sample clock applies. Explicit probe gaze points are already in runtime-world
space and are not transformed.

Optional `--gaze-point X Y Z` and repeated `--weight expression=value` override
selected input with **host test probes**. They require `--vrm`; weights must name
declared expressions. Inputs must be finite; owner clamping/arbitration remains
unchanged. Options precede the avatar and motion paths:

```sh
avatarMotionCheck --vrm --gaze-point 1 1.5 3 --weight happy=0.4 <avatar.vrm> <motion.vrma>
```

Every frame compares all joints, morph identities/weights, material identities,
types, override flags and RGB/alpha with separate owner calls at `1e-6`.
The oracle derives the head from separately sampled/retargeted pose, including
root placement, rather than runtime output. Bone eye rotations include authored
rest; quaternion signs compare equivalently. Held gaze is explicitly stale while
scalar values remain usable. Additional frames check explicit zero, total
expression/gaze absence and reset. Both held and active snapshots survive
runtime destruction. Counters distinguish reader-provided fields from selected
probe input; coverage counters exclude the additional lifecycle checks.
Motion-driven joint changes are counted before LookAt so probe eye rotations
cannot hide a clip that produces no pose change from target rest.

The reader carries common `motion:channelName` / `motion:channelValue` and
`motion:lookAtTarget` attributes. Repeated `--channel-input name-attribute
value-attribute prefix` and one `--gaze-input attribute` select additional
owner fields explicitly, through `MotionStageReadOptions`. These options
apply to every supplied motion; select compatible paths or invoke separately.
Missing or declared-only values stay absent, and the selected gaze replaces
the common gaze. The host does not automatically discover native attributes.
For a VRMA owner-selected expression and gaze, an example is:

```sh
avatarMotionCheck --vrm --channel-input /Animation/Expressions/happy.vrm:expressionName /Animation/Expressions/happy.vrm:expressionWeight vrm: --gaze-input /Animation/LookAt.vrm:lookAtTarget <avatar.vrm> <motion.vrma>
```

Names come from the authored attribute rather than the prim path. Common and
selected duplicate channel identities and malformed values retain owner
refusals. This consumes the handoff from
[usd-motion-plugins issue #37](https://github.com/animu-sphere/usd-motion-plugins/issues/37).

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
The separate [LookAt binding](VRM_LOOKAT_USD_BINDING.md) extracts gaze
configuration with test-input evidence. The separate
[Expression binding](VRM_EXPRESSION_USD_BINDING.md) adds actual-avatar morph
output discovery and test scalar/gaze composition.

On 2026-10-06, `--vrm` checked all seven clips again with those actual bindings,
both without probes and with a world-point gaze plus `happy=0.4` / `blink=0.8`
host probes. Each run evaluated 8,286 frames, including zero/absence/reset;
maximum pose/morph component error was `1.403972313e-7`. The probe run changed
morph output on 8,265 frames. All 128 joints, 48 morph slots, snapshot
capabilities and retained active/held state passed; this avatar has no material
binds. Reader-provided expression/gaze counters were zero for every clip.
Owner warnings retain unbound `upperChest` and intentional held/stale checks.
No error diagnostics occurred. Package versions/toolchain are unchanged from
the evidence above; asset hashes and commands stay in ignored local evidence.

`tools.motion_vrm_check` adds constructed USD regression coverage for both
LookAt types, nonidentity eye rest, changing root/head pose, a common semantic
expression key between body keys, all six material slots and alpha, unmapped
channels, explicit zero/absence, stale holds, reset and retained snapshots.
It also rejects malformed probe options and deliberately perturbed oracle
morph/material output. The optional combined build now passes sixteen tests.

On 2026-10-07, the options handoff passed all sixteen runtime tests, including
the installed source-clip consumer. Constructed regressions cover selected
native-style expression/gaze, between-body keys, origin/absence, malformed
owner input, stage/options lifetime and translated/rotated avatar placement.
Separate origin and axis checks validate placement independently of the output
oracle; world probes remain unchanged.

Three generated VRMA format-owner fixtures were opened by the installed file
plugin and explicitly selected on the same actual avatar, without probes.
The expression fixture drove four morph frames, and keyed/default gaze
fixtures each drove three, over 26 evaluation frames in total. All pose/morph
outputs matched the independent owner oracle within `7.058422424e-8`; reset
and retained state passed. These are generated-format-input/real-avatar
integration evidence, not representative captured expression/gaze evidence.
The seven private MotionPack clips still pass 8,286 frames within
`1.403972313e-7`; inspecting their imported stages found no native expression
or gaze attributes, so their zero counters cannot validate those inputs.
Direct inspection of all seven source GLB JSON chunks also found only
`specVersion` and `humanoid` in `VRMC_vrm_animation`, with neither `expressions`
nor `lookAt`. Their missing native input is therefore a source-asset fact,
not evidence that the importer discarded declared expression/gaze data.

This establishes explicit native-attribute intake and clip placement through
actual-avatar LookAt/Expression. Representative captured expression/gaze,
format-owner automatic discovery, live connectors,
source provenance in retained state, renderer output, milestones A/B/C and
ABI freeze remain open. See the [capability matrix](../reference/CAPABILITY_MATRIX.md)
and [roadmap](../roadmap/current.md).
