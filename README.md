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

These are proposed responsibilities; directory names are a
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

The repository currently contains documentation only. No runtime libraries,
build configuration, OST composition metadata or executable API are provided.
Build and installation instructions will be added with executable evidence
when the first implementation lands.

See [contributing](CONTRIBUTING.md) before adding code or changing a boundary.
