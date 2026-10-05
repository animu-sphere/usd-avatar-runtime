# usd-avatar-runtime

The format-independent avatar evaluation IR, lifecycle and orchestration layer
for the animu-sphere ecosystem.

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
implementation direction. The [near-term direction](docs/design/NEAR_TERM_PLAN.md)
prioritizes real motion -> VRM -> evaluated state -> `hydra-toon` fast-path,
then MMD validation of the shared scheduler/state model before ABI freeze.
Runtime Phases A/B overlap. The [roadmap](docs/roadmap/README.md) gives the
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
The headers are experimental revision 3; Runtime Phase A is still open.
Revision 3 adds typed gaze observations with explicit space, validity and clock
mapping, preserving revision 2's retained layout/input/capability metadata.
Revision-1/2 providers and consumers must rebuild.
An optional [VRM adapter](docs/architecture/VRM_ADAPTER.md) connects installed
owner expression and both LookAt evaluator types to pose/morph/material
snapshots, including world/joint-local point/direction input. Direction support
requires an owner install with `VRMRIG_LOOKAT_DIRECTION_API`.
Its constructed-rig tests do not establish real-avatar acceptance.
An optional [motion clip pose adapter](docs/architecture/MOTION_ADAPTER.md)
connects installed owner sampling/retargeting to runtime pose. Its host-side
input assembler maps selected motion scalar channels and world gaze points
to owned input frames, validating motion -> input -> VRM LookAt/Expression
with constructed bindings. An optional
[USD skeleton binding](docs/architecture/USD_BINDING.md) reads authored
joint/rest layout into owned baseline and motion-owner values with explicit
Humanoid mappings, metre conversion and rigid placement. Real-avatar VRM
expression/output binding extraction, multi-source input selection,
connector adapters, Hydra, OpenExec and OST composition remain unimplemented.
The optional [VRM USD Humanoid binding](docs/architecture/VRM_USD_BINDING.md)
discovers the owner's applied schema, resolves its skeleton relationship and
supplies standard role mappings to the generic binder. One privately supplied
avatar validates skeleton/Humanoid binding and constructed motion-to-state
parity. The optional [USD motion clip binding](docs/architecture/MOTION_USD_BINDING.md)
connects installed `motionUsd` reading and source rest to pose evaluation.
Its opt-in `avatarMotionCheck` tool validates seven real VRMA clips on that
avatar with all-joint numeric parity, reset and retained snapshots. Real-avatar
Expression/output extraction and renderer evidence remain open. The optional
[USD LookAt binding](docs/architecture/VRM_LOOKAT_USD_BINDING.md) now extracts
owner range maps and head/eye/rest configuration into the VRM adapter. One
private avatar's Expression-type LookAt has test-gaze owner-weight parity;
its actual expression/output bindings and full motion-to-LookAt flow remain open.

See [contributing](CONTRIBUTING.md) before adding code or changing a boundary.
