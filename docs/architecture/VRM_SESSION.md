---
status: binding
owner: usd-avatar-runtime
---

# Native VRM clip session

`AVATAR_BUILD_VRM_SESSION` composes one schema-bound VRM and one owned motion
clip through `avatarVrmSession::Session`. Enable the USD, VRM USD, spring USD,
spring evaluator, motion USD and motion adapter options too. Installed consumers
request `AvatarRuntime` component `vrm_session` and link
`AvatarRuntime::avatarVrmSession`. This imports installed owner packages without
Jolt, Stage Runner, expression/LookAt evaluation or a renderer.

The host snapshots Humanoid/skeleton, spring/collider values and the clip's
coherent source rest. Input stages can be destroyed after construction. Motion
samples at runtime tick time, then the secondary-phase evaluator receives that
tick's animated pose. Owner algorithms remain in `motionSampling`,
`motionRetarget` and `vrmRig`. Motion channels other than pose are not selected.

The core [native fixed-tick session](FRAME_LIFECYCLE.md#native-backend-free-fixed-tick-session)
owns accumulation, seeding and retained output. `Play`/`Pause` affect the next
elapsed request; the caller supplies only time elapsed in the current playback
state. A paused request adds no time and does not drain backlog. `Reset(start)`
pauses, increases generation, clears history/output/backlog, and declares a new
seed time. The next `Advance(0)` initializes the current animated pose. A seek
starts a fresh simulation there; it does not reconstruct earlier velocities.

`SessionConfig::secondaryMotion` explicitly selects spring or motion-only
evaluation at construction. A live enable/disable or binding change closes the
old session and constructs a new one, including a new consumer binding. It is
not an in-place evaluator-plan edit. Motion-only mode does not read spring
declarations. Spring mode retains the existing rigid, unit-scale, metre-stage
admission boundary and refuses unsupported inputs with owner/binding diagnostics.

`Snapshot()` borrows the latest committed tick state. Retain through `Api()`
before advancing, resetting or closing; retained values survive runtime/provider
destruction. Delivery retries read that handle and do not call `Advance`.
`Stats()` exposes completed ticks in the last request, total tick index, fixed
simulation time, pending backlog and initialization. A failed evaluation retains
the latest successful tick and due time; retry `Advance(0)` with no extra elapsed.

The caller can hand the snapshot and `Binding()` to existing pose or renderer
consumers. `SkeletonPoseOutput` is an optional display transport over an anonymous
USD layer; it runs only when the host delivers a snapshot. `Close()` detaches the
clock before destroying runtime instances and then provider resources. Close the
output layer separately. All calls use one exclusive calling thread.

## Check host

`avatarVrmSessionCheck avatar.vrm clip.vrma fixed_seconds ticks [--no-springs]`
uses default-prim avatar scoping and requires exactly one clip skeleton. The
caller configures installed file-format/schema resources and DLL discovery.
For example, `fixed_seconds=0.016666666666666666` and `ticks=120` exercise two
seconds at 60 Hz. The check compares motion with direct owner retargeting,
springs with a separate owner solver, and delivered USD pose identity/local TRS
with the snapshot. Translation/quaternion component tolerance is `1e-6`; USD
half-scale quantization uses relative tolerance `5e-4` (absolute near zero).
It also checks paused output identity, exact reset replay and retained unload.

`tools/check-vrm-session.ps1` runs an explicitly supplied VRM 0.x, VRM 1.0 and
clip in both modes and writes asset hashes and outcomes to a caller-supplied
evidence file. No avatar or clip is copied into the repository.

Generated source/installed tests cover both spring declaration shapes and
stage-independent session lifetime. Measured real-avatar scope and exclusions
are recorded in the [capability matrix](../reference/CAPABILITY_MATRIX.md#vrm-fixed-tick-session-and-host-evidence-2026-10-29).
This is a headless pose host: resident mesh rendering, Hydra/direct agreement,
pixels, display interpolation and shared-world VRM composition remain open.
