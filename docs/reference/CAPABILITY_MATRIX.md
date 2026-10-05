# Capability matrix

Checked against this repository on 2026-10-05. This is implementation status,
not a promise about sibling repositories or a release/support declaration.
The experimental direct runtime and CMake installation/export are implemented.
Runtime Phase A remains open; revision 3 is not a frozen ecosystem ABI.

| Surface | Current status | Owning documentation |
| --- | --- | --- |
| implementation direction | adopted documentation; real-provider/fast-path integration prioritized before freeze | [design policy](../design/DESIGN_POLICY.md), [near-term direction](../design/NEAR_TERM_PLAN.md) |
| runtime architecture and layout | one reusable `avatarRuntime` target; further component split proposed | [overview](../architecture/OVERVIEW.md), [layout](../architecture/PROJECT_LAYOUT.md) |
| input/evaluated-state contracts | experimental revision-3 C headers: attributed scalars and typed gaze point/direction observations with explicit world/joint-local space, validity and clock mappings; host input revision and dense snapshots with retained layout ID/version and active capabilities; configured owner clip-to-pose mapping and owned selected-motion scalar/world-gaze assembly implemented; live observation intake, multi-source composition, expression/gaze result records and other deformation channels pending | [input](../contracts/INPUT_FRAME.md), [state](../contracts/EVALUATED_STATE.md) |
| evaluator registration, phase execution and lifecycle | direct serial plan, dependencies/cycles/write validation, instance state, commit/abort/reset implemented; real-provider phase conformance and checkpoint restore pending | [evaluator](../contracts/EVALUATOR.md), [lifecycle](../architecture/FRAME_LIFECYCLE.md) |
| versioned C ABI | revision-3/size validation, revision-1/2 rejection, scoped handles, retain/release and installed C provider-consumer tests; ABI freeze pending | [ABI](../contracts/ABI.md) |
| capability negotiation and diagnostics | exact-version provider/binding intersection, required support checks, ordered bounded diagnostics and provider provenance implemented; semantic vocabulary/output negotiation pending | [capabilities/diagnostics](../contracts/CAPABILITIES_AND_DIAGNOSTICS.md) |
| motion/VRM/MMD/connector integration | optional installed owner clip sampling/retarget-to-pose adapter, host scalar/world-gaze input assembler and `vrmRig` expression + expression/bone LookAt adapter; explicit clock/input/layout/joint/output and eye/rest binding and diagnostics; constructed motion -> assembled input -> VRM and gaze-space numeric parity tested; actual clip/avatar USD/Humanoid discovery, connectors, multi-source selection and MMD pending | [motion adapters](../architecture/MOTION_ADAPTER.md), [VRM adapter](../architecture/VRM_ADAPTER.md), [dependencies](../architecture/DEPENDENCIES.md) |
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

The direction extension was checked with a separately built/installed local
`vrmRig` 0.10.0 source package advertising `VRMRIG_LOOKAT_DIRECTION_API` on the
same date/toolchain. It is an unpublished additive owner API, not support from
older 0.10.0 installs. The owner's `vrmRig_unit` and `vrmRig_boundaries` tests
both pass; the adapter build consumes its installed headers/library.

| Test | Evidence |
| --- | --- |
| `adapters.vrm_expression` | real owner LookAt/expression algorithms against constructed rigs; numeric direct-owner morph/RGB/alpha and bone-eye quaternion parity at `1e-6`; exact source/actor/channel selection; owner binary/override arbitration and clamp diagnostics; gaze precedence; world head placement/rotation including earlier pose writes; world/root/head/rotated-eye point/direction mapping with both LookAt types and present/absent eye offsets; directions remain angular at placements outside float range; asymmetric bone maps, both yaw/pitch signs, authored rest composition, preservation of eye translation/scale, one-eye diagnostics and no accumulation; missing layout/version/target/material/head/eye, invalid eye mapping/rest/parent, unsupported slots/head/reference scale and range failure/retry; absence/zero, stale/unavailable and target-at-origin gaze, type-specific capabilities, same-phase pose writer conflict detection, independent instances, partial eye-write rollback/retry/reset and snapshots retained past runtime destruction |
| `adapters.vrm_installed` | separate configure/build via installed `AvatarRuntime` `vrm` component and adapter headers/static library, repeating owner-boundary checks; the installed C-only core consumer still resolves without provider dependencies |

This is real-library adapter evidence with test input and constructed rig
configuration. It is not real VRM asset/motion/connector evidence, independently
validated owner semantics, renderer output, milestone acceptance or ABI freeze.

The motion/VRM Release build was checked on the same date/toolchain using
separately built/installed local `motionSampling` and `motionRetarget` 0.5.3,
installed `motionCore` 0.5.0, the direction-capable local `vrmRig` 0.10.0 above
and OpenUSD 26.08. `ctest --test-dir build/motion-vrm --output-on-failure`
passes all seven core/VRM/motion tests, adding:

Fresh core-only and motion-only Release configurations also pass their three
and five tests respectively, including installed consumers. Motion-only
configuration resolves no `vrmRig` target.

| Test | Evidence |
| --- | --- |
| `adapters.motion_pose` | independent owner sampling/retarget numeric parity at `1e-6`; explicit clip/runtime clock scale/offset; interpolation and boundary hold; root delta, undriven nonidentity rest rotation and nonunit scale; owner/runtime slot remapping; empty clip absence; required/unbound bone diagnostics; pose-only scalar/gaze diagnostics; invalid configuration/layout/joint/parent/time failure and same-frame retry; downstream failure rollback; reset, independent instances and snapshots retained past runtime destruction; input assembler ownership/copy lifetime, explicit native/runtime mapping, source/actor/sample clocks, frame/USD metadata, absent/zero/unclamped weights, world-point origin/absence and host-selected stale validity, unmapped-field reporting, malformed channels/mappings/context/clock rejection and corrected retry; with VRM enabled, constructed partial-rig motion -> assembled input + working head -> LookAt -> Expression numeric parity with the owner's VRM required-bone set and reversed selection order, scalar/gaze precedence, stale held gaze with usable scalars and absent-input baseline |
| `adapters.motion_installed` | separate configure/build using installed `AvatarRuntime` `motion` component and adapter headers/static library; repeats input assembly and lifetime checks; optional `motion vrm` consumer repeats composition, explicit-zero clearing and reset checks; core-only consumer in the same install does not resolve providers |

This is immutable constructed-clip/rig composition evidence. Actual motion
captures, USD Humanoid discovery, real-avatar bindings, live connector intake,
multi-source selection and retained source metadata remain unvalidated; milestones A/B/C and
Runtime Phase A stay open.
