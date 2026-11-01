---
status: proposed
owner: usd-avatar-runtime
---

# Project layout

The targets in this tree and how they ship. Registry and diagnostics live inside the
core library. Do not create empty targets or rename working directories to match a sketch.

## Current targets

| Target | Location / responsibility |
| --- | --- |
| `avatarRuntime` | `libs/avatarRuntime`: common C contracts, lifecycle, storage, registration, scheduling and diagnostics; no external provider dependency |
| `avatarMmdSession` | `adapters/mmd-session`: optional native single-avatar PMX/VMD/Jolt application composition; shared by usdview and external snapshot consumers; [session contract](MMD_SESSION.md) |
| `avatarMotionAdapter` | `adapters/motion`: owner sampling/retarget/validation invocation, owned diagnostic reports and MotionPoseInputBridge with legacy source aliases |
| `avatarUsdBinding` | `adapters/usd`: typed owner skeleton/placement read to runtime identity/baseline/placement binding and live pose, selected morph/UV/material publication |
| `avatarMotionUsdBinding` | `adapters/motion-usd`: throwing owner-result read used directly by the host; compatible immutable StageClip wrapper delegates to it |
| `avatarVrmAdapter` | `adapters/vrm`: owner LookAt/expression registration and common-state mapping |
| `avatarMmdAdapter` | `adapters/mmd`: owner MMD pre/post control/coupling registration and host world bridge; [boundary](MMD_ADAPTER.md) |
| `avatarStageRunnerSession` | `adapters/stage-runner`: optional installed clock/playback session with borrowed or owned runtime world registration; [boundary](STAGE_RUNNER_SESSION.md) |
| `avatarPhysicsUsdResources` | `adapters/physics-usd`: optional installed resource sets over physics-owner snapshots and whole-stage world ownership with a caller-selected factory; [boundary](PHYSICS_USD_RESOURCES.md) |
| `avatarVrmUsdBinding`, `avatarVrmLookAtUsdBinding`, `avatarVrmExpressionUsdBinding` | `adapters/vrm-usd`: separately optional owner schema/configuration bindings |
| `avatarVrmSpringUsdBinding` | `adapters/vrm-spring-usd`: optional canonical spring/collider stage snapshot into installed owner values; [boundary](VRM_SPRING_USD_BINDING.md) |
| `avatarVrmSpringAdapter` | `adapters/vrm-spring`: optional transactional owner spring registration and final rotation mapping; [boundary](VRM_SPRING_ADAPTER.md) |
| `avatarMotionCheck` | `tools/avatarMotionCheck`: opt-in asset parity host, optionally with installed Toon probe transport checks |
| `avatarVrmSession` / `avatarVrmSessionCheck` | `adapters/vrm-session`: opt-in native clip/spring/expression/LookAt host and headless owner/pose-output check over core `FixedTickSession` |
| `usdviewMmdPhysicsPython` | `tools/usdviewMmdPhysics`: opt-in live single-PMX usdview application host, installed-owner control/coupling/Jolt and anonymous pose output |
| `mmdToonContracts` | `adapters/mmd/tests`: separately opt-in final MMD/Jolt snapshot transport host using installed Toon and explicit generated resources |

All adapter targets are optional; linking/finding only core must not resolve
OpenUSD, VRM, MMD, Hydra or renderer packages.

Planned, with no target yet: `avatarImaging` for Hydra publication, `execAvatar` for OpenExec
and inspect, replay and benchmark hosts. Direct consumers need no runtime-side bridge target: a
renderer-side host matches runtime targets to resident resources and its adapter reads retained
snapshots ([resident target matching](OUTPUT_PATHS.md#resident-target-matching)).

## Dependency placement

Public contracts should expose compact C-compatible views and opaque handles.
C++ wrappers sit above them. The core and registry must not require a renderer
backend, OpenUSD, format libraries, Hydra or OpenExec. The runtime consumes
registered evaluator adapters; it does not own their format algorithms.

`avatarImaging` owns OpenUSD imaging/Scene Index integration. `execAvatar`
owns the execution-mechanism adapter. Both are optional. A direct runtime build
must avoid loading either merely to call an evaluator.

Existing motion/format packages may use OpenUSD value types internally.
Dependency isolation must be demonstrated per target; the public C ABI does
not erase a library's actual dependencies.

## Packaging

`openstrata.toml` names the workspace members and the release members. `libs/avatarRuntime` packages
the core, and `libs/avatarVrmSession` and `libs/avatarMmdSession` each package the core with their
session's adapters and pin their owners by digest. Other adapters stay source-built. See
[how a release is cut](../releases/README.md).

Pin one of these:

| A consumer that needs | Pins | Finds |
| --- | --- | --- |
| the core, with its own providers | `avatarRuntime` | `find_package(AvatarRuntime)` |
| the VRM session | `avatarVrmSession` | `find_package(AvatarVrmSession)`, or the `vrm_session` component of `AvatarRuntime` |
| the MMD session | `avatarMmdSession` | `find_package(AvatarMmdSession)`, or the `mmd_session` component of `AvatarRuntime`, with [a Jolt SDK](MMD_SESSION.md#package-boundary) |

A session's prefix includes the core, so each provides the `AvatarRuntime` CMake package and the
same core files, and the core's targets keep the `AvatarRuntime::` namespace in all of them. A session
does not depend on a separately published core: it would have to pin a digest made by the same
release, and its component would have to span two prefixes.

They do not combine. A session's package uses the core in its own prefix, whatever the search order.
A prefix without a session refuses its component, and a session's package refuses a core already
found in such a prefix, each naming this choice. `scripts/check_package_choice.py` checks these
orders, including the other session's package found first, in the session and release lanes. Composing them with `ost runtime compose` fails on their shared install paths.

## Tools and tests

`tests/contracts` compiles a separate C11 provider DLL, C11 consumer and C++20
runtime-boundary test executable. `tests/installed_consumer` builds the C
provider/consumer in a separate CMake configuration against installed headers
and the exported `AvatarRuntime::avatarRuntime` shared-library target. No
renderer/OpenExec/provider package is needed for these tests. Build/install
commands are in the [root README](../../README.md#build).

Inspection reports contracts, bindings and capabilities. Replay executes a
captured evaluation deterministically. Benchmarking measures phase/evaluator
costs and publication separately. These tools share runtime code rather than
implementing another evaluator loop.

Contract tests validate boundary behavior; parity compares execution/output
paths; replay verifies reproducibility. Renderer image checks and source-format
algorithm tests remain with their owners.
