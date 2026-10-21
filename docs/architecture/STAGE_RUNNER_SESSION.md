---
status: binding
owner: usd-avatar-runtime
---

# Stage Runner physics host session

The optional `avatarStageRunnerSession` target supplies
[`avatarStageRunner::PhysicsSession`](../../adapters/stage-runner/include/avatarStageRunner/PhysicsSession.h)
over an explicitly configured runtime world. It consumes the installed Stage
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

The caller creates the runtime, instances, owner adapters and solver resources.
Construct with a runtime/world handle pair to borrow an existing registration,
or with a runtime and `ArPhysicsWorldDesc` to create a registration owned by the
session. The descriptor constructor forwards diagnostics and throws
`std::runtime_error` on registration failure, preserving the runtime's status in
the exception message. It copies no solver resources and performs no solver
reset. Descriptor arrays/strings are consumed during construction; callback
code and data remain borrowed until the registration is removed.

The caller keeps the runtime alive through session destruction. With a borrowed
registration it also keeps the world and its callback resources alive that long;
with an owned registration world callback resources may be released after
successful `Close()`. Close does not destroy instances or unregister evaluators;
their provider code/resources still follow the core and provider lifetime
contracts, including reset and destruction requirements. Use a single thread
and one exclusive driver per world.
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
- `Close` pauses, removes an owned registration, releases session output and
  clears request/result history. It neither steps nor resets the external
  solver. A borrowed registration stays registered. Close is idempotent;
  `IsOpen()` becomes false on success, and Display/RetryAdvance/Reset subsequently
  return `AR_INVALID_STATE` without reading the clock or invoking callbacks.
  Play/Pause do nothing while closed. Failed removal leaves the session paused
  and open, preserves output/history and returns the runtime status.
- `RemoveParticipant` forwards the independently negotiated membership extension
  at a synchronous boundary, for either owned or borrowed registrations. On
  success it releases only that member's session snapshot reference; survivors
  keep their previous input order, handles and `LastAdvance` metadata. It reads
  no clock and changes neither playback nor world time/backlog. Refusal keeps
  all output intact; closed calls return `AR_INVALID_STATE`. Subsequent requests
  must cover exactly the remaining participants, including zero inputs when
  only non-avatar bodies remain. The exclusive driver must finish external
  body/constraint/command cleanup before advancing or resetting again, following
  the [membership contract](FRAME_LIFECYCLE.md#experimental-fixed-tick-requests).
  Instance destruction is then permitted; reuse requires explicit reset.

Destruction closes the session automatically and releases its snapshot
references. Destroy a session before its runtime. Closing an owned registration
is a whole-world discontinuity: all of its members require explicit increasing-
generation instance reset before reuse, or may be destroyed. It leaves other
worlds in the same runtime alone. A new session may register the same world ID
after removal and instance reset; the host must separately restore solver/body
state and bindings before resuming. The session cannot be reopened in place.

Renderer delivery retries read retained output without another simulation
request. Independently retain a handle before the next successful advance,
reset, member removal, close or session destruction. The session never destroys
the caller's solver, body resources or runtime.

## Scope

This is a reusable host session for explicitly prepared worlds, validated by
the [session evidence](../reference/CAPABILITY_MATRIX.md#scoped-stage-runner-session-evidence-2026-10-21)
and [world-lifetime evidence](../reference/CAPABILITY_MATRIX.md#scoped-session-world-lifetime-evidence-2026-10-21).
The [selective-unload evidence](../reference/CAPABILITY_MATRIX.md#scoped-selective-unload-evidence-2026-10-21)
adds member removal with explicit host-owned body teardown.
Automatic stage/world construction, `StageSession`/usdview/standalone application
wiring, stop/single-step UI semantics, automatic body/constraint teardown,
resident renderer resource matching and Hydra/direct agreement remain
open. Stop/reset policy must be selected by the application rather than silently
translating stop into seek or
a second fixed-step loop.
