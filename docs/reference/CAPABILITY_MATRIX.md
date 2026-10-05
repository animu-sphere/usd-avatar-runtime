# Capability matrix

Checked against this repository on 2026-10-05. This is implementation status,
not a promise about sibling repositories or a release/support declaration.
The experimental direct runtime and CMake installation/export are implemented.
Runtime Phase A remains open; revision 3 is not a frozen ecosystem ABI.

| Surface | Current status | Owning documentation |
| --- | --- | --- |
| implementation direction | adopted documentation; real-provider/fast-path integration prioritized before freeze | [design policy](../design/DESIGN_POLICY.md), [near-term direction](../design/NEAR_TERM_PLAN.md) |
| runtime architecture and layout | one reusable `avatarRuntime` target; further component split proposed | [overview](../architecture/OVERVIEW.md), [layout](../architecture/PROJECT_LAYOUT.md) |
| input/evaluated-state contracts | experimental revision-3 C headers: attributed scalars and typed gaze point/direction observations with explicit world/joint-local space, validity and clock mappings; host input revision and dense snapshots with retained layout ID/version and active capabilities; owner motion mapping, expression/gaze result records and other deformation channels pending | [input](../contracts/INPUT_FRAME.md), [state](../contracts/EVALUATED_STATE.md) |
| evaluator registration, phase execution and lifecycle | direct serial plan, dependencies/cycles/write validation, instance state, commit/abort/reset implemented; real-provider phase conformance and checkpoint restore pending | [evaluator](../contracts/EVALUATOR.md), [lifecycle](../architecture/FRAME_LIFECYCLE.md) |
| versioned C ABI | revision-3/size validation, revision-1/2 rejection, scoped handles, retain/release and installed C provider-consumer tests; ABI freeze pending | [ABI](../contracts/ABI.md) |
| capability negotiation and diagnostics | exact-version provider/binding intersection, required support checks, ordered bounded diagnostics and provider provenance implemented; semantic vocabulary/output negotiation pending | [capabilities/diagnostics](../contracts/CAPABILITIES_AND_DIAGNOSTICS.md) |
| motion/VRM/MMD/connector integration | optional installed `vrmRig` expression + expression-type LookAt adapter with explicit input/layout/output binding and diagnostics; constructed-rig numeric parity tested; real-avatar USD binding, Humanoid/motion/connectors, bone LookAt and MMD pending | [VRM adapter](../architecture/VRM_ADAPTER.md), [dependencies](../architecture/DEPENDENCIES.md) |
| Hydra state overlay and direct consumer API | retained direct snapshot API implemented; Hydra/renderer binding and parity pending | [output paths](../architecture/OUTPUT_PATHS.md) |
| OpenExec orchestration adapter | proposed; not implemented | [overview](../architecture/OVERVIEW.md) |
| capture, replay, inspection and benchmarks | proposed; not implemented | [recording/replay](../design/RECORDING_AND_REPLAY.md) |
| multi-avatar execution and parallel scheduling | independent instances tested under serial execution; parallel scheduling pending | [overview](../architecture/OVERVIEW.md) |
| OST composition and published runtime packages | local CMake install/export tested; OST composition and publication not configured | [layout](../architecture/PROJECT_LAYOUT.md) |
| supported native/Web/WASM/XR targets | local Windows x64/MSVC direct prototype evidence only; no other target validation | [roadmap](../roadmap/current.md) |

Update a row only with implementation and validation evidence from this
repository. A sibling's callable library or published package is an integration
input, not proof that this runtime already composes it.

## Validation evidence

The direct Release build was checked on Windows x64 with MSVC 19.51 and CMake
4.4.3 on 2026-10-05. No OpenUSD, Hydra, OpenExec or sibling provider target is
linked. `ctest --test-dir build/direct --output-on-failure` runs:

| Test | Evidence |
| --- | --- |
| `contracts.c_provider_consumer` | separate runtime DLL, C11 provider DLL and C11 consumer; revision-3 table negotiation, revision-2 rejection, typed gaze transport including stale/unavailable and invalid-direction retry; capability discovery and copied layout/input/capability metadata surviving runtime shutdown |
| `contracts.runtime` | C++20 headers; revision-1/2 and undersized-view rejection; deterministic plan order; missing/backward/cyclic dependencies; overlapping writers; capability mismatch; stateful failure/retry/reset; ignored/invalid writes; time/input/layout validation; independent instances; absent/zero and complete snapshot behavior; diagnostic provenance and overflow; retained metadata, stable layout across reset, explicit layout version change, prior input revision on failure and domain-independent evaluator metadata; gaze absence/origin, point/direction, world/bound-joint spaces, unchanged stale/unavailable transport, source/actor identity, invalid arrays/references/numerics/clocks/duplicates and failure before provider callbacks with same-frame retry |
| `contracts.installed_consumer` | install into a build-local prefix, configure/build C provider and consumer separately with `find_package(AvatarRuntime 0.1.0 EXACT)`, run the C boundary against installed headers/library |

These core tests use synthetic providers. They prove runtime boundary behavior,
not VRM/MMD/motion algorithm conformance, renderer parity or an ABI freeze.

The optional Release adapter build was also checked on Windows x64/MSVC 19.51
on 2026-10-05 with installed `vrmRig` 0.10.0, `motionCore` 0.5.0 and OpenUSD
26.08. `ctest --test-dir build/vrm --output-on-failure` passes the same three
core tests plus:

| Test | Evidence |
| --- | --- |
| `adapters.vrm_expression` | real owner LookAt/expression algorithms against constructed rigs; numeric direct-owner morph/RGB/alpha parity at `1e-6`; exact source/actor/channel selection; owner binary/override arbitration and clamp diagnostics; gaze precedence; world head placement/rotation; missing layout/version/target/material/head, unsupported slots/gaze spaces/kinds/head scale; absence/zero, stale gaze, expression-only capability set, independent instances, failure/retry/reset and snapshots retained past runtime destruction |
| `adapters.vrm_installed` | separate configure/build via installed `AvatarRuntime` `vrm` component and adapter headers/static library, repeating owner-boundary checks; the installed C-only core consumer still resolves without provider dependencies |

This is real-library adapter evidence with test input and constructed rig
configuration. It is not real VRM asset/motion/connector evidence, independently
validated owner semantics, renderer output, milestone acceptance or ABI freeze.
