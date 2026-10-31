# Changelog

Notable changes to the released runtime. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/). While the version is
0.x, a minor version may change the public API or ABI. The package version and
the C ABI revision are separate numbers.

## [Unreleased]

### Changed

- A session pull request lane builds, tests and packages `avatarVrmSession` on Windows
  and Linux. It and the release lane build and run the repository's installed consumer
  on both, and `verify-consumer` still runs on Windows only.
- A change touching only documentation no longer runs the pull request lanes or the
  `plain cmake` push lane.

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
