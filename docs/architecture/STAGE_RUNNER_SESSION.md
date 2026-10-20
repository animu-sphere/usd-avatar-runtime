---
status: binding
owner: usd-avatar-runtime
---

# Stage Runner physics host session

The optional `avatarStageRunnerSession` target supplies
[`avatarStageRunner::PhysicsSession`](../../adapters/stage-runner/include/avatarStageRunner/PhysicsSession.h)
over an already configured runtime world. It consumes the installed Stage
Runner `runtimeCore::FrameClock`. Avatar-runtime remains the sole owner of
fixed ticks, backlog, evaluation order and the shared-world barrier under the
[frame lifecycle](FRAME_LIFECYCLE.md#5-physics-clock-and-discontinuities).
The adapter adds no format, physics backend, renderer or OpenUSD dependency.

## Build and installation

Enable `AVATAR_BUILD_STAGE_RUNNER_SESSION=ON` with installed `runtimeCore` 0.1
available through `CMAKE_PREFIX_PATH` or `runtimeCore_DIR`. This optional C++20
target sits above the unchanged revision-4 core and experimental physics table.
It does not freeze a cross-toolchain C++ ABI or publish a dependency pin.

```cmake
find_package(AvatarRuntime 0.1.0 EXACT CONFIG REQUIRED COMPONENTS stage_runner)
target_link_libraries(host PRIVATE AvatarRuntime::avatarStageRunnerSession)
```

Core-only lookup does not find Stage Runner. Session-only lookup finds neither
MMD, OpenUSD, a physics backend nor Toon. The optional MMD host smoke also
requires this build option; its ordinary MMD registration component stays
independent of the session target.

## Lifecycle and ownership

The caller creates the runtime, instances, owner adapters and world binding,
then constructs one session with that runtime/world pair. The session borrows
those handles; the caller keeps them and provider code/resources alive through
session destruction. Use a single thread and one exclusive driver per world.
Do not also advance the world through `StageSession` or another accumulator.
The clock can be injected for deterministic host tests.

- Construction starts paused and anchors the clock. `Play` and `Pause` anchor
  transitions, excluding paused wall time; repeated calls are idempotent and
  do not discard elapsed playing time.
- `Display` measures host elapsed time and sends one `advance_world` request.
  The runtime decides zero/one/multiple fixed ticks and retains excess backlog.
  Input templates are borrowed only during the call, in caller-specified order.
  Input selection, freshness and binding discovery remain caller policy.
- `RetryAdvance` anchors the clock and submits a new request with zero elapsed
  after a pre-Step failure. Time spent waiting for this explicit retry is
  discarded. It does not bypass post-Step invalidation or group reset.
  Rejected input validation does not accept time; callers must correct input
  before submitting further display time. No automatic retry is performed.
- `Snapshots()` borrows handles owned by the session in its last successful
  input order. A successful request replaces them, including zero-tick results;
  a failed request preserves the entire previous displayable set. Inspect the
  returned status before treating retained output as a new frame.
- `LastAdvance()` describes the latest attempted runtime advance, including
  partial tick progress on failure as exposed by the C table. It is distinct
  from the last successful displayable snapshot set.
- `Reset` always pauses and reanchors the clock after the group callback.
  Success clears session output, request history and result counters. A refusal
  preserves displayable snapshots and propagates the runtime status; group
  validity follows the runtime's own reset contract. The caller supplies an
  increasing generation and chooses when to resume.

Renderer delivery retries read retained output without another simulation
request. Independently retain a handle before the next successful advance,
reset or session destruction. Session destruction releases only its snapshot
references; it does not destroy or reset the caller's world or runtime.

## Scope

This is a reusable host session for explicitly prepared worlds, validated by
the [capability matrix](../reference/CAPABILITY_MATRIX.md#scoped-stage-runner-session-evidence-2026-10-21).
Automatic stage/world construction, `StageSession`/usdview/standalone application
wiring, stop/single-step UI semantics, avatar unload, resident renderer resource
matching and Hydra/direct agreement remain open. Stop/reset policy must be
selected by the application rather than silently translating stop into seek or
a second fixed-step loop.
