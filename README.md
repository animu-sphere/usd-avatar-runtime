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

The reusable `avatarRuntime` target exposes common C contracts and owns
lifecycle, state storage, scheduling, registration and diagnostics. Provider
and USD integrations are separately optional targets:

| Component | Responsibility |
| --- | --- |
| `avatarRuntime` | common contracts, instances, ordered evaluation, frame lifecycle, registry and diagnostics |
| `avatarMotionAdapter` | owner sampling/retarget calls and selected-motion input bridge |
| `avatarUsdBinding` / `avatarMotionUsdBinding` | runtime skeleton/baseline binding and direct owner motion-stage results; legacy StageClip compatibility |
| `avatarVrmAdapter` / VRM USD binding targets | owner LookAt/expression evaluation and authored configuration |
| `avatarMotionCheck` | opt-in real-asset adapter parity host |

MMD, Hydra, reusable `hydra-toon` publication, recording publication, OpenExec and
inspect/replay/benchmark components remain planned. The optional motion-check
host can validate resolved values through installed `Toon::AvatarState` probe
resources. See
[project layout](docs/architecture/PROJECT_LAYOUT.md) for current targets and
the intended responsibility split.

## Documentation

Start at [docs/](docs/README.md). The
[design policy](docs/design/DESIGN_POLICY.md) preserves the supplied
implementation direction. The adopted
[boundary cleanup policy](docs/design/BOUNDARY_POLICY.md) adds motion validation
and USD skeleton/rest ownership cleanup, thin adapters and recording publication
rules. Motion validation delegation and input-bridge naming are implemented;
USD skeleton/rest reading and direct owner-result host integration are implemented;
publication work remains planned. The [near-term direction](docs/design/NEAR_TERM_PLAN.md)
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
The headers are experimental revision 4; Runtime Phase A is still open.
Revision 4 retains per-sample source time with each snapshot, including the
pose sample a provider actually used, and adds a two-component material value
type. It preserves revision 3's typed gaze observations and revision 2's
retained layout/input/capability metadata. Revision-1–3 providers and
consumers, including a renderer adapter that pins revision 3, must rebuild.
An optional [VRM adapter](docs/architecture/VRM_ADAPTER.md) connects installed
owner expression and both LookAt evaluator types to pose/morph/material
snapshots, including world/joint-local point/direction input. Direction support
requires an owner install with `VRMRIG_LOOKAT_DIRECTION_API`.
Its constructed-rig tests do not establish real-avatar acceptance.
An optional [motion clip pose adapter](docs/architecture/MOTION_ADAPTER.md)
connects installed owner sampling/retargeting to runtime pose. Its host-side
`MotionPoseInputBridge` (`InputAssembler` source alias) maps selected motion
scalar channels and world gaze points
to owned input frames, validating motion -> input -> VRM LookAt/Expression
with constructed bindings. The motion component requires installed owner
validation APIs and preserves owner reports through `MotionValidationError`
and synchronous diagnostic sinks. An optional
[USD skeleton binding](docs/architecture/USD_BINDING.md) reads authored
joint/rest layout into owned baseline and motion-owner values with explicit
Humanoid mappings, metre conversion and rigid placement. Multi-source input selection,
connector adapters, Hydra, OpenExec and OST composition remain unimplemented.
The optional [VRM USD Humanoid binding](docs/architecture/VRM_USD_BINDING.md)
discovers the owner's applied schema, resolves its skeleton relationship and
supplies standard role mappings to the generic binder. One privately supplied
avatar validates skeleton/Humanoid binding and constructed motion-to-state
parity. The optional [USD motion clip binding](docs/architecture/MOTION_USD_BINDING.md)
connects installed `motionUsd` reading and source rest to pose evaluation.
Its opt-in `avatarMotionCheck` tool validates seven real VRMA clips on that
avatar with all-joint numeric parity, reset and retained snapshots. Real-avatar
Representative captured VRMA expression/gaze and renderer evidence remain open. The optional
[USD LookAt binding](docs/architecture/VRM_LOOKAT_USD_BINDING.md) now extracts
owner range maps and head/eye/rest configuration into the VRM adapter. One
private avatar's Expression-type LookAt has test-gaze owner-weight parity;
the separate [Expression binding](docs/architecture/VRM_EXPRESSION_USD_BINDING.md)
now extracts actual morph/material targets and supplies a complete baseline.
That avatar's 18 expressions and 48 morph slots have test scalar/gaze owner
parity; material targets have constructed-USD evidence. `avatarMotionCheck --vrm`
now composes seven real motion clips with those actual bindings and explicit
host gaze/expression probes, checking all pose/morph output and retained state.
The source-clip adapter now accepts explicit owner-selected expression/gaze
attributes, and the parity tool places clip gaze in runtime-world space.
Generated native VRMA fixtures drive actual-avatar outputs without probes;
representative captured expression/gaze and automatic native discovery remain
open. See the [USD motion binding](docs/architecture/MOTION_USD_BINDING.md).

See [contributing](CONTRIBUTING.md) before adding code or changing a boundary.

With the motion-check tool and VRM adapter enabled, `AVATAR_MOTION_CHECK_TOON=ON`
adds an installed `Toon` `AvatarState` dependency only to that host. Use a renderer
install containing the additive `BaseColorRgb` binding. Run
`avatarMotionCheck --vrm --toon <avatar> <motion> [motion ...]` to check retained
state-to-palette/morph/material transport. The renderer install must be built
for this runtime's ABI revision; the configure probe refuses other revisions. This constructs probe resources;
actual avatar rendering and Hydra/direct parity remain open. See
[output paths](docs/architecture/OUTPUT_PATHS.md#scoped-toon-transport-check).
