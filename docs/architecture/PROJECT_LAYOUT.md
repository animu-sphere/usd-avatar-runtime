---
status: proposed
owner: usd-avatar-runtime
---

# Project layout

This maps [design policy section 18](../design/DESIGN_POLICY.md) to prospective
components. The initial implementation provides public headers, one
`libs/avatarRuntime` library, CMake installation/export support and contract
tests. Registry/validation/diagnostics currently live inside that library;
separate `avatarCore`/`avatarRegistry`/`avatarDiagnostics` targets below are
prospective, as are plugins and tools. Do not create empty targets from this
sketch.

```text
usd-avatar-runtime/
  cmake/                         build support
  docs/
    architecture/ contracts/ design/ reference/ roadmap/ contributing/
  include/avatarRuntime/         public C ABI and optional C++ convenience API
  libs/
    avatarCore/                  common values and contract validation
    avatarRuntime/               instances and frame orchestration
    avatarRegistry/              providers and capability discovery
    avatarDiagnostics/           diagnostics and observation hooks
  plugins/
    avatarImaging/               optional Hydra state overlay
    execAvatar/                  optional OpenExec integration
  tools/
    avatarInspect/ avatarReplay/ avatarBenchmark/
  tests/
    contracts/ parity/ replay/
```

## Dependency placement

Public contracts should expose compact C-compatible views and opaque handles.
C++ wrappers sit above them. The core and registry must not require a renderer
backend, Hydra or OpenExec. The runtime consumes registered evaluator adapters;
it does not own their format algorithms.

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
