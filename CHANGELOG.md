# Changelog

Notable changes to the released runtime. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/). While the version is
0.x, a minor version may change the public API or ABI. The package version and
the C ABI revision are separate numbers.

## [Unreleased]

## [0.4.0] - 2026-11-01

The MMD session ships as a package and runs real models from it, as the VRM session did in v0.3.0. The C ABI stays
at experimental revision 5 and is not frozen. The `AvatarRuntime` CMake package moves to 0.4, and `SameMinorVersion`
refuses a request for 0.3.

### Added

- `avatarMmdSession` as a package, pinning its owners by digest, with `avatarMmdSessionCheck` and a
  generated PMX and VMD in the session lane. Consumers supply the deterministic Jolt SDK that
  `physicsJoltSixDof` requires.
- The MMD session selects a larger Jolt world capacity, and runs models with self-joints and bones several
  bodies drive, on `usd-physics-plugins` 0.1.2 and `usd-mmd-plugins` 0.3.2.
- A `uvs` channel in the `--state-output` format and `scripts/compare_avatar_state.py`.
- `avatarMmdSession` is a release member. The release lane builds the deterministic Jolt SDK, packages the
  session twice, and runs its installed consumer and generated PMX and VMD on Windows and Linux.

### Changed

- The VRM session's fixture moves to `tests/assets/vrm-session`.

## [0.3.0] - 2026-11-01

Completes the VRM session and connects it to a renderer: motion, LookAt, expression and spring
evaluate in one session, and `hydra-toon` draws the result. The C ABI stays at experimental
revision 5 and is not frozen. The `AvatarRuntime` CMake package moves to 0.3, and `SameMinorVersion`
refuses a request for 0.2.

### Added

- The VRM session selects expressions and LookAt through `SessionConfig`, with host
  observations passed to a new `Session::Advance` overload. `avatarVrmSessionCheck --face`
  compares the results with the owner.
- The VRM session reads expressions and gaze from the clip through
  `SessionConfig::clipExpressions` and `clipGaze`, sampled at each tick time.
  `avatarVrmSessionCheck --clip-expression` and `--clip-gaze` compare them with the owner.
- `FixedTickSession::Advance` accepts a `FixedTickObservations` source that selects
  observations for each evaluation, as catch-up cannot share one held set of tick-dependent
  values. The C ABI is unchanged.
- The evaluated-state contract states what a consumer can rely on: identity, units, spaces,
  absent values, gaze and snapshot lifetime. `hosts.vrm_session` checks it.
- `scripts/check_session_fixture.py` runs a generated VRM and VRMA through the owner's published
  importers and the session in the session and release lanes, and compares the committed state with
  `tests/assets/session/expected.json`. `avatarVrmSessionCheck --state-output` writes that state.
- `hosts.vrm_session` covers import and evaluator failure, recovery, destroy/recreate and two
  avatars in one thread.
- `scripts/compare_avatar_state.py` compares two sequences of evaluated state by identity, with
  tolerances per value kind and explicitly declared unsupported channels. The fixture check uses it.

### Changed

- The `avatarVrmSession` package and `vrm_session` component now include the VRM
  expression and LookAt adapters.
- A session pull request lane builds, tests and packages `avatarVrmSession` on Windows
  and Linux. It and the release lane build and run the repository's installed consumer
  on both.
- The release lane and source CI move to `ost` 0.23.18, and `verify-consumer` now checks
  the session on Linux as well, declaring X11 and Xt as host packages of the consumer.
- A prefix without the session now refuses the `vrm_session` component, and
  `AvatarVrmSession` refuses a core already found in such a prefix. Both messages name
  the choice between the two packages, which `scripts/check_package_choice.py` checks.
- A change touching only documentation no longer runs the pull request lanes or the
  `plain cmake` push lane.

### Known limitations

- `avatarRuntime` and `avatarVrmSession` still provide the same CMake package and core files, so
  a consumer pins one. Combining them is refused.
- Node constraints are not evaluated, and scaled spring rigs are refused.
- Real avatars and the renderer are measured on Windows only, with generated expression and gaze
  input and no real-avatar material bind. No consumer feeds the state comparison yet.

## [0.2.0] - 2026-11-01

Publishes the native VRM session for `cy2026` / `usd` on Windows x86_64 and Linux
x86_64. The C ABI stays at experimental revision 5 and is not frozen.

### Added

- The `avatarVrmSession` package: the core and the session's motion, USD, VRM USD
  and spring adapters in one prefix, with `avatarVrmSessionCheck`. It pins motion
  0.5.4 and VRM 0.12.0 by digest. `find_package(AvatarVrmSession)` is equivalent to
  requesting the `vrm_session` component of `AvatarRuntime`.

### Changed

- The release lane and source CI move to `ost` 0.23.16. The lane checks a consumer of
  the session with `verify-consumer` on Windows and with the repository's installed
  consumer on Linux, where `verify-consumer` cannot find OpenUSD's X11 dependency.
- The `AvatarRuntime` CMake package version moves to 0.2. `SameMinorVersion`
  refuses a `find_package(AvatarRuntime 0.1)` request.
- Installed-consumer tests register only when this repository is the top-level
  project, because `ost library test` runs without a compiler environment.
- Tests that open a VRM stage register the installed schema themselves.

### Known limitations

- `avatarRuntime` and `avatarVrmSession` provide the same CMake package and the same
  files for the core. A consumer pins one of them.
- The session selects pose only. Expressions, gaze and node constraints are not
  evaluated, and scaled spring rigs are refused.
- The session has no pull request lane; only the release workflow builds it.

## [0.1.0] - 2026-10-31

The first versioned release. It publishes the dependency-free `avatarRuntime`
core for `cy2026` / `usd` on Windows x86_64 and Linux x86_64, with the C ABI at
experimental revision 5. **The ABI is not frozen:** Runtime Phase A remains open,
and any other revision is refused at the boundary. What exists and how far it was
measured is in the [capability matrix](docs/reference/CAPABILITY_MATRIX.md).

### Added

- Common C contracts for input frames, evaluators, evaluated state, capabilities
  and diagnostics, with versioned and size-checked descriptors.
- Instances, ordered evaluation with transactional provider state, immutable
  retained snapshots, reset and capability/plan validation.
- An experimental physics table: fixed ticks, a shared-world barrier, group
  reset, post-Step failure invalidation and selective participant removal.
- The backend-free `FixedTickSession` for instances without a rigid-body world.
- An installed `AvatarRuntime::avatarRuntime` CMake package with
  `SameMinorVersion` compatibility.
- A release workflow publishing the core by digest, with a generated consumer pin
  table.

### Source only

The optional motion, USD, VRM, MMD, Stage Runner and physics-USD adapters and the
host tools are in the source archive, not the package. They build against
installed owners: motion 0.5.4, `vrmRig` and `vrmSchema` 0.10 or newer,
`mmdPhysicsAdapter` 0.3, and the physics and Stage Runner libraries 0.1.

### Known limitations

- Hydra publication, recording publication, OpenExec, inspect/replay tools and
  connector intake are not implemented.
- Full PMX/VMD and representative avatar acceptance remain open; see the
  [roadmap](docs/roadmap/current.md).
- macOS is built by continuous integration but is not a release target. Web and
  XR are not targets.
