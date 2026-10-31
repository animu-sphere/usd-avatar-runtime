# OpenUSD Avatar Runtime

[![License: Apache-2.0](https://img.shields.io/github/license/animu-sphere/usd-avatar-runtime?label=license)](LICENSE)

`usd-avatar-runtime` is the format-independent avatar evaluation layer for OpenUSD. It
decides which evaluator runs for which avatar, in what order, and publishes one resolved
state that renderers and other consumers read.

It coordinates evaluation order, frame lifecycle, per-avatar state, capability negotiation,
physics barriers and clocks. It does not interpret file formats or render. VRM and MMD
meaning stay in their format repositories, generic motion mathematics in
`usd-motion-plugins`, device intake in `motion-connectors`, solving in `usd-physics-plugins`
and realization in renderers such as `hydra-toon`. See the
[dependency boundary](docs/architecture/DEPENDENCIES.md).

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

The composed USD stage supplies authored state. Evaluated state is published without
writing the stage every frame.

## Status

The runtime is experimental: the C ABI is at revision 5 and is not frozen.

| Package | Contents | Dependencies |
| --- | --- | --- |
| `avatarRuntime` | the core: C contracts, instances, ordered evaluation, transactions, retained snapshots, physics barrier and fixed-tick session | none |
| `avatarVrmSession` | the core and a native VRM and motion clip session with spring secondary motion | motion, VRM and OpenUSD packages, pinned by digest |

Both target `cy2026` / `usd` on Windows x86_64 and Linux x86_64. The VRM session evaluates
pose and spring. Expression, LookAt and MMD adapters are source-built, and Hydra publication,
recording and connector intake are not implemented. The
[capability matrix](docs/reference/CAPABILITY_MATRIX.md) has the details, and the
[roadmap](docs/roadmap/current.md) has what comes next.

## Use

Pin a package from the [release notes](docs/releases/README.md). A consumer pins one of the
two, since both provide the `AvatarRuntime` CMake package:

```cmake
find_package(AvatarRuntime 0.2 REQUIRED)                         # the core
find_package(AvatarRuntime 0.2 REQUIRED COMPONENTS vrm_session)  # the VRM session
```

The session's API is described in the [native VRM session](docs/architecture/VRM_SESSION.md).

## Build

The core needs CMake 3.22+, a C++17 compiler and, for tests, a C11 compiler. It needs no
OpenUSD, Hydra or format library. On Windows, use an x64 developer shell with Ninja:

```sh
cmake -S . -B build/direct -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/direct
ctest --test-dir build/direct --output-on-failure
cmake --install build/direct --prefix install
```

Optional adapters are separate `AVATAR_BUILD_*` options built against installed owners. The
[project layout](docs/architecture/PROJECT_LAYOUT.md) lists them.

## Documentation

Start at the [documentation index](docs/README.md). [Contributing](CONTRIBUTING.md) explains
ownership before adding code or changing a boundary, and the [changelog](CHANGELOG.md) lists
released changes.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md), the [Code of Conduct](CODE_OF_CONDUCT.md) and the
[Security Policy](SECURITY.md). Bug reports and feature requests have
[issue templates](.github/ISSUE_TEMPLATE/).

## License

Apache-2.0 — see [LICENSE](LICENSE).

Third-party dependencies and avatar, texture, motion and capture assets retain their own
licenses. Use generated fixtures when possible; third-party assets require explicit
redistribution permission before inclusion.
