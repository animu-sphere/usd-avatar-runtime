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
find_package(AvatarRuntime 0.1.0 EXACT CONFIG REQUIRED COMPONENTS mmd_session)
target_link_libraries(host PRIVATE AvatarRuntime::avatarMmdSession)
```

`avatarMmdSession::Session` starts paused. `Advance(elapsedSeconds)` submits
host elapsed time to the existing runtime accumulator: 1/120-second ticks,
at most eight catch-up ticks per request, with backlog retained. MMD owns
VMD binding, pre/post controls, bone feedback, vertex/group morph weights
and primary UV evaluation; Jolt executes once at each runtime Step barrier.
Both experimental spring options remain OFF by default. Existing owner
body-property, startup and unsupported-morph admission rules are preserved.

`Baseline()` exposes the complete layout for renderer matching. `Snapshot()`
borrows the latest final result; independently retain it through `Api()`
before another successful advance, reset, close or destruction. The supplier
API/library must outlive every retained result. A zero-tick display has no
new result. `publishUsd=false` supplies final snapshots without creating an
output layer or authoring USD. The default `true` preserves usdview's
temporary pose/morph/UV publication.

Use one exclusive calling thread. Play/Pause preserve solver state and exclude
paused time. Reset pauses, advances generation, rebuilds frame-zero bodies and
clears display output. Close unregisters the world before solver resources,
is idempotent, and refuses later Play/Advance; construct a new session to start
again. Retained results remain immutable across reset/close. The stage and
supplier runtime remain owned through session destruction. Runtime owns fixed
ticks and evaluation order; the application never steps Jolt separately.

There is no automatic loop, arbitrary seek, multi-avatar participant discovery
or material-morph renderer admission. Source edits require stopping/rebinding.
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
