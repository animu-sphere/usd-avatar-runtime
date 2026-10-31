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
its revision-5 C ABI are unchanged.

## Owner reading

`ReadStageClip` calls the installed
`motionUsd::ReadCanonicalMotionStage`, which owns source skeleton reading and
generic units/axis/rate/placement validation. The helper preserves the owner's
`MotionStageRead`, including its owner-built `descriptor` and `sourceRest`;
it contains no descriptor/source-rest rebuilding, USD motion conversion or
private motion result vocabulary. It returns the owner's value directly.

The parity host uses `ReadStageClip` and passes the owner clip
and source rest to motion registration with explicit target bindings. `StageClip`
remains a source-compatible immutable wrapper over the same function for existing
callers; its `SourceRest()` references the rest in `Read()`. It owns no reading logic.

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
transitively. The generic `usd`/`vrm_usd` bindings resolve `motionUsd` for
owner skeleton reading, while importing no source-clip adapter or sampler.
Core-only lookups still resolve no provider package.

[`ReadStageClip`](../../adapters/motion-usd/include/avatarMotionUsd/ReadStageClip.h)
requires a stage and an explicit skeleton prim path. The strict owner reader
validates source skeleton and clip-space metadata and reads the bound animation.
The helper retains the owner-built `SourceRestPose` without runtime avatar/layout
identities for the source or a second rest copy.
The returned owner value and its copies survive stage changes/destruction;
caller mutations of a copy do not affect the original. Legacy `StageClip` copies
continue to share immutable storage.
Contract tests compare the retained descriptor and every source-rest array with
the direct strict owner result, verify the `SourceRest()` alias after stage
destruction, and preserve duplicate-bone refusal code/subject/detail.
The optional third argument accepts owner `MotionStageReadOptions` for selected
scalar name/value paths, prefixes and a gaze path. It forwards these to the
strict owner reader, preserving its refusals and diagnostics. Attribute
discovery remains format-owner/host configuration; no VRMA layout is inferred.
The returned result preserves the owner's clip, skeleton, animation identity,
encoding rate, optional metadata, warnings and typed descriptor/source rest.
No reader warning is suppressed.

Supply **both** `read.clip` and `*read.sourceRest` to `ClipPoseAdapterConfig`:

```cpp
const auto read = avatarMotionUsd::ReadStageClip(stage, skeletonPath, inputs);
config.clip = read.clip;
config.sourceRest = *read.sourceRest;
// Supply target layout/skeleton/map/joint IDs, placement and clock mapping.
avatarMotion::ClipPoseAdapter adapter(config);
```

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

## Evidence

Measured scope is in the [evidence log](../reports/evidence-log.md#usd-motion-clip-binding-summary), and current state is in the
[capability matrix](../reference/CAPABILITY_MATRIX.md).
