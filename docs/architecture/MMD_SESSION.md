---
status: binding
owner: usd-avatar-runtime
---

# Native MMD application session

The optional `avatarMmdSession` target composes the installed MMD owner,
`avatarStageRunner::PhysicsSession`, USD physics resources and Jolt for a
directly opened single-avatar PMX with optional original VMD input. The
implementation is shared with the usdview Python wrapper. No Python or Qt
API is used by this native target, and no renderer is linked into it.

Enable `AVATAR_BUILD_MMD_SESSION` together with MMD, USD binding, Stage Runner
session and physics USD resources. `AVATAR_BUILD_USDVIEW_MMD_PHYSICS` also
builds it. Consumers use the installed package:

```cmake
find_package(AvatarRuntime 0.3.0 EXACT CONFIG REQUIRED COMPONENTS mmd_session)
target_link_libraries(host PRIVATE AvatarRuntime::avatarMmdSession)
```

`avatarMmdSession::Session` starts paused. `Advance(elapsedSeconds)` submits
host elapsed time to the existing runtime accumulator: 1/120-second ticks,
at most eight catch-up ticks per request, with backlog retained. MMD owns
VMD binding, pre/post controls, bone feedback, vertex/group morph weights
and primary UV/material evaluation; Jolt executes once at each runtime Step barrier.
Both experimental spring options remain OFF by default. Existing owner
startup and unsupported-morph admission rules are preserved. Body properties
explicitly select installed `ReadSourceBodyProperties`: each damping channel
maps its owner finite rate or pre-force prior-velocity clearing response into
the neutral descriptor. The strict owner reader remains separate.

`DampingNormalizations()` returns owned session records with original attribute
paths, original coefficients and effective coefficients for normalized source
values. Copy records before carrying them across a rebuild. Successful Reset
rereads declarations and replaces the records; malformed declarations still
refuse. The Python wrapper exposes copied `dampingNormalizations` dictionaries,
and usdview reports them on construction and successful Reset. This is applied
source policy, separate from `UnappliedMetadata()` spring gaps.

The session selects installed owner `ReadJointAdmission` for enabled MMD
`spring6Dof` joints. Two `followBone` endpoints preserve their USD declaration
but allocate no solver constraint. `InertJoints()` and Python `inertJoints`
expose owned source paths (Python returns copies); usdview reports them on
construction and successful Reset. Copy native records before a rebuild.
Successful Reset replaces admission records. Generic descriptor validation
and owner spring validation still apply to omitted joints. Invalid endpoint
metadata refuses, and repaired Reset admission can be retried. World/non-MMD
joint endpoints remain outside the owner's scoped policy. Source edits that
change the coupling layout still require session reconstruction.

Configuration and installed `mmd_session` lookup require owner
`SOURCE_BODY_PROPERTIES_VERSION=1`, `JOINT_ADMISSION_VERSION=1`, core `DAMPING_RESPONSE_VERSION=1` and
`physicsJolt_PRIOR_VELOCITY_CLEARING_AVAILABLE`. Rebuild the complete physics
core/backend/USD bridge and runtime consumer closure for the descriptor change;
matching product versions alone do not establish compatibility.

`Baseline()` exposes the complete layout for renderer matching. `Snapshot()`
borrows the latest final result; independently retain it through `Api()`
before another successful advance, reset, close or destruction. The supplier
API/library must outlive every retained result. A zero-tick display has no
new result. `publishUsd=false` supplies final snapshots without creating an
output layer or authoring USD. The default `true` preserves usdview's
temporary pose/morph/UV and canonical material publication.

When the model declares material morphs, construction admits stage-contract v2
materials by canonical path, applied `MmdMaterialAPI`, source index and exact
static PMX/stage baseline. Connected, sampled, missing or mistyped canonical
inputs reject. Each material exposes twelve runtime inputs: the six canonical
`inputs:mmd:material:*` fields and independent vec4 tint coefficients
`mmd:{texture,sphere,toon}Tint:{multiply,add}`. The latter are opaque runtime
input identities, not new USD attributes. The adapter calls the owner query on
raw final channels and restores immutable bases/identity coefficients at zero.

USD delivery selects only the six canonical inputs, in the same transactional
anonymous layer as pose/UV. The owner defines no USD inputs for sampled tints;
direct renderer consumption carries those coefficients. The renderer's existing
MMD/full.fx tint switch selects their realization without changing evaluation.

Use one exclusive calling thread. Play/Pause preserve solver state and exclude
paused time. Reset pauses, advances generation, rebuilds frame-zero bodies and
clears display output. Close unregisters the world before solver resources,
is idempotent, and refuses later Play/Advance; construct a new session to start
again. Retained results remain immutable across reset/close. The stage and
supplier runtime remain owned through session destruction. Runtime owns fixed
ticks and evaluation order; the application never steps Jolt separately.

There is no automatic loop, arbitrary seek, multi-avatar participant discovery
or general-purpose material discovery outside the direct-PMX contract.
Source edits require stopping/rebinding.
The renderer's scoped native host is described in its
[viewport guide](https://github.com/animu-sphere/hydra-toon/blob/main/docs/guides/BUILDING.md#live-mmd-physics-in-the-dedicated-viewport).
It consumes snapshots through resident bindings; it does not use a recording.

The opt-in generated `hosts.mmd_session_snapshots` test runs 720 fixed ticks
over three generations, checks exact replay, pause, final joint/morph/UV layout,
tick identity, retained output through reset/close, whole-world teardown and
unchanged root/session USD. Enable it with `AVATAR_MMD_SESSION_TEST_PMX` set
to the MMD owner's generated morph fixture and `AVATAR_MMD_SESSION_TEST_VMD`
set to generated `live-physics-uv-mixed.vmd`. The existing eight usdview native
tests cover the shared implementation's properties, springs, keyed input,
dynamic feedback, startup and morph/UV publication. This scoped host does not
close representative L3, Hydra/direct parity or the broader physics MVP gates.

Setting `AVATAR_MMD_SESSION_TEST_PMX` also enables
`hosts.mmd_session_installed`. This separately configures and compiles an
installed `mmd_session` consumer, checking typed normalization records, Reset,
live snapshots and nonmutation without Python. It can use the generated
pendulum alone without configuring the mixed UV/vertex VMD option.

`hosts.mmd_session_materials` accepts the optional
`AVATAR_MMD_SESSION_TEST_MATERIAL_PMX`/`_VMD` generated pair. Generate it with
`tools/generate-mmd-material-fixture.py --owner <mmd-repository> --out <directory>`;
the script uses the owner's test encoders and creates no private asset copies.
It also writes `combined.vmd`, adding the owner's keyed pendulum and signed
group/direct vertex/UV channels to the material motion without changing the
original material-only pair. The renderer's
[combined comparison](https://github.com/animu-sphere/hydra-toon/blob/main/docs/reports/renderer/114-2026-10-31-mmd-combined-parity.md)
consumes this additional generated motion through the same native session.
The runtime test checks direct owner values, independent tint coefficients, canonical USD
values, zero restoration, pause, three reset generations and retained unload
in both publication modes. Current evidence is in the
[capability matrix](../reports/evidence-log.md#live-mmd-material-evidence-2026-10-29).

## Package boundary

A release member from v0.4.0. The MMD session follows the
[VRM session's packaging](PROJECT_LAYOUT.md#packaging):

- `libs/avatarMmdSession` packages the core with the `mmd`, `usd`, `stage_runner`, `physics_usd`
  and `mmd_session` targets in one prefix, and pins its owners by digest.
- `AvatarMmdSession` is a thin package over the `mmd_session` component, so that a core already found
  in another prefix is refused. Targets keep the `AvatarRuntime::` namespace.
- `avatarMmdSessionCheck` ships in the prefix, and writes state in the `--state-output` format. Jolt owns
  the simulation, so it takes the session's own bodies each tick and evaluates them again through the
  owner's feedback, continuation, morph and UV queries: a difference is one of composition. Material
  values are not compared with the owner, and it reports how many it left out. No renderer is linked.
- A consumer pins one package. The `mmd_session` component gets the same refusal as `vrm_session` in
  a prefix without it, and `scripts/check_package_choice.py` covers the new orders.

Each session's prefix provides `AvatarRuntime`, so one process cannot use both. That is enough for
the trial, because `toon-viewport-vrm` and `toon-viewport-mmd` are separate executables. A host that
needs both formats in one process needs a combined package, to be decided when one exists.

| Owner | Libraries the session needs | State |
| --- | --- | --- |
| `usd-motion-plugins` 0.5.4 | `motionCore`, `motionSampling`, `motionRetarget`, `motionUsd` | published by digest for Windows and Linux |
| `usd-physics-plugins` 0.1.2 | `physicsCore`, `physicsJoltSixDof`, `physicsUsd` | by digest for Windows and Linux |
| `usd-stage-runner` 0.1.0 | `runtimeCore` | published by digest for Windows and Linux |
| `usd-mmd-plugins` 0.3.2 | `mmdPhysicsAdapter`, `mmdControl`, `mmdMotionBinding`, `mmdModel`, `mmdPmx`, `motionVmd` | by digest for Windows and Linux |

This repository does not vendor or rebuild them. A published MMD library names its siblings by version
range, so the descriptor pins all six. Real models also rest on three owner rules:

- **World capacity.** The session selects 65,536 body pairs and contacts and 128 MiB of temporary memory,
  because models with 349 to 571 bodies exhaust `physicsJolt`'s defaults.
- **Self-joints.** A joint from a body to itself constrains nothing. The importer authors it disabled, with
  `MMD_PHYSICS_JOINT_SELF`, and the session creates no constraint for it.
- **Several bodies per bone.** The latest body in source order drives the bone, with
  `MMD_PHYSICS_BONE_MULTIPLY_DRIVEN`, and the others keep simulating. This is the owner's deterministic rule,
  not a claim of native MMD equivalence.

A PMX joint names two bodies and the importer drops one whose body is missing, so world and non-MMD endpoints
do not arise, and `ReadJointAdmission` still refuses them. A model with no rigid bodies is refused, as a session
with nothing to simulate. Two things follow from the owners' packages:

- **A deterministic Jolt SDK is a prerequisite.** MMD joints need six-DOF constraints, which published
  `physicsJolt` does not have: the session refuses any model with an admitted joint on it, with
  `SceneResources requires SixDofConstraints for admitted joints`. `physicsJoltSixDof` has them, and
  [its owner](https://github.com/animu-sphere/usd-physics-plugins/blob/main/docs/architecture/DEPENDENCIES.md#3-jolt-physics)
  leaves building Jolt 5.5.0 to the consumer, with the deterministic options its release workflow names.
  The installed package checks the exported definitions and refuses any other build. The session lanes
  build it that way and pass it through `CMAKE_PREFIX_PATH`, and every consumer of this package has to
  as well. The capability stays experimental at the owner.
- **The package has no `package_contract`.** `ost library verify-consumer` resolves only a declared
  closure, which cannot name that SDK. `scripts/check_session_consumer.py --session mmd` builds and runs
  the repository's installed consumer instead.

### Fixtures

[`tests/assets/mmd-session`](../../tests/assets/mmd-session/README.md) holds generated assets that CI can
run, and `scripts/check_mmd_session_fixture.py` runs them on both platforms:

- Committed PMX and VMD files with a `--check` generator. They are written with the owner's encoders from
  its pinned source archive, so CI needs no owner checkout. `usdMmdFileFormat` and `mmdSchema` are
  published, and pinned by digest as the VRM importers are.
- One model that exercises the whole path. Morph, primary UV, IK, append and physics feedback each change
  committed state, and each input in `broken/` must fail the comparison on the feature it took away. It has
  a self-joint and a bone two bodies drive, so the owners' rules for them run in the lanes.
- Expected state through `scripts/compare_avatar_state.py`, which has a channel for primary vertex UVs.

On the deterministic SDK, this fixture's state has equal values on Windows and Linux, so one expected
file serves both, at the script's stated tolerance. That is [measured](../reports/evidence-log.md#mmd-session-package-2026-11-01)
for this small model and 240 ticks, not assumed for others, and exact replay stays the per-platform check.

Acceptance still needs a real PMX and VMD, which stay local. Generated assets show the path, not
MMD compatibility.
