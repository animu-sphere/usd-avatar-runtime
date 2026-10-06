---
status: proposed
owner: usd-avatar-runtime
---

# Project layout

This maps [design policy section 18](../design/DESIGN_POLICY.md) and
[boundary policy sections 9 and 17](../design/BOUNDARY_POLICY.md) to components.
The current implementation provides public headers, one
`libs/avatarRuntime` library, CMake installation/export support and contract
tests. Registry/validation/diagnostics currently live inside that library.
`adapters/vrm` now provides the optional installed `avatarVrmAdapter` target
and owner-boundary tests; see the [scoped adapter](VRM_ADAPTER.md).
`adapters/vrm-usd` provides the separate optional installed
`avatarVrmUsdBinding` target for [schema Humanoid discovery](VRM_USD_BINDING.md).
The same directory adds separate `avatarVrmLookAtUsdBinding` and
`avatarVrmExpressionUsdBinding` targets for authored gaze and expression/output
configuration, enabled with the VRM evaluator adapter.
Registry/diagnostics library separation remains prospective. The opt-in
`tools/avatarMotionCheck` host is implemented; inspect/replay/benchmark hosts,
MMD and publication adapters remain prospective. Do not create empty targets
or rename working directories merely to match a responsibility sketch.

## Current targets

| Target | Location / responsibility |
| --- | --- |
| `avatarRuntime` | `libs/avatarRuntime`: common C contracts, lifecycle, storage, registration, scheduling and diagnostics; no external provider dependency |
| `avatarMotionAdapter` | `adapters/motion`: owner sampling/retarget invocation and selected-motion input bridge |
| `avatarUsdBinding` | `adapters/usd`: current skeleton/baseline/placement binding; owner conversion split pending |
| `avatarMotionUsdBinding` | `adapters/motion-usd`: current StageClip integration; motion-domain cleanup pending |
| `avatarVrmAdapter` | `adapters/vrm`: owner LookAt/expression registration and common-state mapping |
| `avatarVrmUsdBinding`, `avatarVrmLookAtUsdBinding`, `avatarVrmExpressionUsdBinding` | `adapters/vrm-usd`: separately optional owner schema/configuration bindings |
| `avatarMotionCheck` | `tools/avatarMotionCheck`: opt-in asset parity host |

All adapter targets are optional; linking/finding only core must not resolve
OpenUSD, VRM, MMD, Hydra or renderer packages.

## Target responsibility layout

```text
usd-avatar-runtime/
  cmake/                         build support
  docs/
    architecture/ contracts/ design/ reference/ roadmap/ contributing/
  include/avatarRuntime/         public C ABI and optional C++ convenience API
  libs/
    runtime/                     instances, contracts and frame orchestration
    registry/                    providers and capability discovery
    diagnostics/                 diagnostics and observation hooks
  adapters/
    motion/                      owner invocation and runtime input bridge
    usd/                         runtime identity/layout/baseline binding
    motion-usd/                  thin owner motion read/registration helper
    vrm/ vrm-usd/ mmd/            owner semantics/configuration adapters
    hydra/                       optional avatarImaging publication
    hydra-toon/                  optional avatarHydraToonBridge publication
    openexec/                    optional execAvatar execution integration
  tools/
    inspect/ replay/ benchmark/   runtime hosts
    avatarMotionCheck/            existing opt-in parity host
  tests/
    contracts/ parity/ replay/
```

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

## OST and packaging

Future OST workspace/composition declarations belong to the composition layer,
separate from reusable libraries and headers. Published package identities,
profiles, pins and formation names are decided with actual adoption/build
evidence. The roadmap does not prescribe unverified OST configuration syntax.

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
