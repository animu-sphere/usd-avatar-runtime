# usd-avatar-runtime

The avatar evaluation composition and orchestration layer for the
animu-sphere ecosystem.

## Scope

This repository defines how motion, avatar-format evaluators and external
input participate in one runtime: evaluation order, frame lifecycle,
per-avatar state, capability negotiation and publication of evaluated state.

Format repositories own VRM and MMD meaning; `usd-motion-plugins` owns generic
motion mathematics; `motion-connectors` owns device and protocol intake;
renderers own realization. See the [dependency boundary](docs/architecture/DEPENDENCIES.md).

## Architecture

```text
external sources -> motion-connectors -> AvatarInputFrame
                                             |
                                  usd-avatar-runtime
                             motion / VRM / MMD evaluators
                                             |
                                  EvaluatedAvatarState
                                      /      |      \
                                   Hydra  fast API  recording
```

The composed USD stage supplies authored state. Evaluation produces runtime
state that can be published without writing the stage every frame. Hydra and
the direct consumer API receive the same resolved state. See the
[architecture](docs/architecture/OVERVIEW.md) and
[output paths](docs/architecture/OUTPUT_PATHS.md).

## Components

The first direct implementation is one reusable `avatarRuntime` target with
public C headers. Additional component targets remain a
[layout proposal](docs/architecture/PROJECT_LAYOUT.md).

| Component | Responsibility |
| --- | --- |
| `avatarCore` | common input and evaluated-state contracts |
| `avatarRuntime` | instances, ordered evaluation and frame lifecycle |
| `avatarRegistry` | evaluator registration and capability discovery |
| `avatarDiagnostics` | structured diagnostics, tracing and timing |
| `avatarImaging` | optional Hydra publication adapter |
| `execAvatar` | optional OpenExec execution integration |
| inspect / replay / benchmark tools | observation and reproducible validation |

## Documentation

Start at [docs/](docs/README.md). The
[design policy](docs/design/DESIGN_POLICY.md) preserves the supplied
implementation direction. The [roadmap](docs/roadmap/README.md) gives the
implementation sequence and acceptance criteria; the
[capability matrix](docs/reference/CAPABILITY_MATRIX.md) records what exists.

## Build

The experimental direct runtime builds without OpenUSD, Hydra, OpenExec or
format libraries. It requires CMake 3.22+, a C++17 compiler and a C11 compiler
for tests. On Windows, run in an x64 developer shell with Ninja available:

```sh
cmake -S . -B build/direct -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/direct
ctest --test-dir build/direct --output-on-failure
cmake --install build/direct --prefix install
```

Tests exercise separately compiled C provider/consumer libraries, transaction
rollback, instance isolation, retained snapshots and an external build against
the installed `AvatarRuntime::avatarRuntime` CMake target. See
[the ABI](docs/contracts/ABI.md) and
[capability matrix](docs/reference/CAPABILITY_MATRIX.md) for exact coverage.
The headers are experimental revision 1; Runtime Phase A is still open.
Motion/gaze adapters, Hydra, OpenExec and OST composition are not implemented.

See [contributing](CONTRIBUTING.md) before adding code or changing a boundary.
