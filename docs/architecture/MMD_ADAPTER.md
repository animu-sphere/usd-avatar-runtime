---
status: binding
owner: usd-avatar-runtime
---

# Scoped MMD control and coupling adapter

The optional `avatarMmdAdapter` target registers the installed MMD owner's
control continuation and bone/body coupling with the existing
[shared-world barrier](../contracts/EVALUATOR.md#5-shared-world-physics-orchestration).
MMD semantics remain in
[`mmdControl` and `mmdPhysicsAdapter`](https://github.com/animu-sphere/usd-mmd-plugins/blob/main/docs/design/PHYSICS_INTEGRATION.md).
This document owns registration, configuration and lifetime. Support and
evidence belong to the [capability matrix](../reference/CAPABILITY_MATRIX.md).

## Build and installation

Enable `AVATAR_BUILD_MMD_ADAPTER` with installed `mmdPhysicsAdapter` 0.2 and
OpenUSD 26.08 available through `CMAKE_PREFIX_PATH`. The owner package must
provide `Evaluator::BeginPhysics`, `ResumePhysics` and `physics::Adapter`;
the local 0.2 build used for validation is not a published dependency claim.
No sibling source is compiled into the runtime.

```powershell
cmake -S . -B build/mmd -A x64 -DAVATAR_BUILD_MMD_ADAPTER=ON "-DCMAKE_PREFIX_PATH=<mmd-install>;<usd-install>"
cmake --build build/mmd --config Release
ctest --test-dir build/mmd -C Release --output-on-failure
```

Put owner dependency DLL directories on `PATH` before running tests.
The headless backend smoke is separately opt-in: `AVATAR_MMD_JOLT_SMOKE=ON`
requires installed `physicsUsd`/available `physicsJolt`, generated
`AVATAR_MMD_PMX` (`coupling-modes.pmx`) and `AVATAR_MMD_VMD` (`sample.vmd`),
and the installed MMD importer/schema on `PXR_PLUGINPATH_NAME`.
The backend is linked only into the test host, never the registration adapter.

`AVATAR_MMD_TOON_SMOKE=ON` additionally requires that Jolt smoke and an installed
`Toon` package with the `AvatarState` component built for the current runtime
ABI. A configure-time header probe refuses incompatible installations. Only
the separate `mmdToonContracts` test host links the renderer; ordinary MMD and
installed adapter consumers retain their existing dependencies. Explicit
generated resources exercise final snapshot transport under the
[output boundary](OUTPUT_PATHS.md#scoped-mmd-transport-check).

`AVATAR_MMD_STAGE_RUNNER_SMOKE=ON` additionally requires the Toon smoke and
`AVATAR_BUILD_STAGE_RUNNER_SESSION=ON` with installed Stage Runner `runtimeCore`.
The separate `mmdStageRunnerContracts` host uses the reusable
[physics host session](STAGE_RUNNER_SESSION.md) for display time, play/pause,
group reset, explicit zero-elapsed retry and retained display output. It
explicitly holds selected input templates across catch-up ticks. The generated
scene still uses explicit renderer probe resources and caller-built world
resources; application and resident-resource integration remain open.
Ordinary installed MMD consumers import no host target.

```cmake
find_package(AvatarRuntime 0.1.0 EXACT CONFIG REQUIRED COMPONENTS mmd)
target_link_libraries(host PRIVATE AvatarRuntime::avatarMmdAdapter)
```

Core-only lookup imports no MMD, physics or OpenUSD target. The optional MMD
component imports no physics/backend target. This is a C++20 configuration
boundary above the revision-4 C ABI, not a frozen cross-toolchain C++ ABI.

## Configuration and ownership

[`PhysicsAdapterConfig`](../../adapters/mmd/include/avatarMmd/PhysicsAdapter.h)
owns copies of the prepared evaluator, bound VMD motion, stateless coupling
binding, layout identity/version, pre/post IDs and predecessors. Joint bindings
must use the exact canonical joint order, parent topology and metre-valued rest
space shared by the evaluator and coupling. Runtime slots are resolved by
skeleton/joint identity, allowing unrelated joints in the runtime layout.
The supplied rigid `placement` must match the composed skeleton placement used
by the coupling binding; it converts owner root-local poses to runtime-world
root transforms once. The host supplies coherent owner bindings and placement;
automatic PMX/avatar configuration discovery is outside this slice.

Model frames are `evaluation_seconds * framesPerSecond + frameOffset`.
The owner handles VMD interpolation, bone morphs, append, IK and visibility.
Explicit morph bindings map already resolved non-bone scalar channels to
runtime blend-shape slots; they do not interpret UV/material/impulse morphs.
The host selects compatible deformation outputs. Visibility maps to named
targets. Unmapped non-bone channels or hidden state without a visibility target
fail. Material morph realization and retargeted-input continuation remain open.

Register both `PreDescriptor()` and `PostDescriptor()`, select both for each
instance, and use the post ID as `ArPhysicsParticipant.post_physics_evaluator`.
Both callbacks occupy the existing constraints phase with an explicit pre ->
post dependency. No phase enum or core ABI change is needed. Later evaluators
run in the suffix with their usual dependency/write declarations.

The adapter and callback captures outlive registration. Per-instance
continuations are keyed by instance **and generation** because reset prepares
new provider states before destroying old ones. Begin/commit/abort discard
tick-local continuations; failed frames never publish partial output. Owner
diagnostics retain code, origin, location and severity; runtime stamps evaluator,
instance and tick. Descriptors carry the installed MMD package version.

## World bridge

The host supplies two borrowed callbacks:

- `submit` replaces pending path-keyed targets for the given instance/tick.
  It must not mutate the solver yet. If another prefix fails, a retry replaces
  those commands; reset clears all pending commands.
- `snapshot` returns owned metre-valued rigid body transforms for the matching
  completed instance/tick. The caller must reject stale or cross-world results.

The host resolves composed paths to caller-owned body handles. Inside the
runtime's single world Step callback it applies all pending kinematic targets,
calls the physics owner's Step once, then captures body states for all
participants. Identical paths in different instances need distinct handle maps.
The post callback invokes owner feedback and `ResumePhysics`, then writes the
final pose/morph/visibility once. It never advances time.

World reset reconstructs the entire solver and handle maps, clears pending
commands/results, and composes with runtime group reset. A failure after Step
requires that reset under the existing lifecycle contract. Direct evaluation
without a matching host snapshot is not a physics-disabled fallback; a separate
motion-only path uses the owner's offline evaluator.

This interface supplies no solver factory, checkpoint restore, arbitrary seek,
automatic shared-world asset construction or renderer binding. The generated
test host demonstrates installed Jolt handles; broader host integration and
rendered acceptance remain in the roadmap.
