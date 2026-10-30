# Capability matrix

Implementation evidence below includes the 2026-10-21 MMD/Jolt callback
slice. This is implementation status,
not a promise about sibling repositories or a release/support declaration.
The experimental direct runtime and CMake installation/export are implemented.
Runtime Phase A remains open; revision 5 is not a frozen ecosystem ABI.

The [boundary cleanup policy](../design/BOUNDARY_POLICY.md) is accepted intent.
The motion-boundary slice is implemented: `ClipPoseAdapter` delegates generic
invariants to installed owner validation APIs, and `MotionPoseInputBridge`
retains `InputAssembler` source aliases. Owned owner reports preserve
code/subject/detail and package identity and can be emitted to C sinks.
The scoped USD reader split is also implemented: `SkeletonBinding` delegates
generic matrix/token/topology/unit/placement checks to installed
`motionUsd::ReadMotionSkeleton` with the explicit `Generic` role, and `ReadStageClip`
retains the coherent clip/descriptor/source-rest result of `ReadCanonicalMotionStage`.
Neither binding rebuilds the owner descriptor or source rest. Reading refusals
retain owner code/subject/detail and package identity/version in `MotionUsdReadError`.
WS-O4 is resolved upstream. The host consumes the owner result directly;
`StageClip` delegates to the same helper for source compatibility. Scoped
workstream B validation is recorded below; the
[roadmap](../roadmap/current.md#boundary-cleanup-workstreams) tracks remaining work.

| Surface | Current status | Owning documentation |
| --- | --- | --- |
| implementation direction | adopted documentation; real-provider/fast-path integration prioritized before freeze | [design policy](../design/DESIGN_POLICY.md), [near-term direction](../design/NEAR_TERM_PLAN.md) |
| runtime architecture and layout | one reusable `avatarRuntime` target; further component split proposed | [overview](../architecture/OVERVIEW.md), [layout](../architecture/PROJECT_LAYOUT.md) |
| input/evaluated-state contracts | experimental revision-5 C headers: attributed scalars and typed gaze point/direction observations with explicit world/joint-local space, validity and clock mappings; host input revision and dense snapshots with retained layout ID/version, active capabilities and source-sample provenance (selected inputs plus provider-reported pose samples with mapped runtime seconds); scalar/vec2/vec3/vec4 material values and primary vertex UV values; configured owner clip-to-pose mapping with retained interpolated/held sample time and owned selected-motion scalar/world-gaze assembly implemented; live observation intake, multi-source composition, expression/gaze result records and other deformation channels pending | [input](../contracts/INPUT_FRAME.md), [state](../contracts/EVALUATED_STATE.md) |
| evaluator registration, phase execution and lifecycle | direct serial plan, dependencies/cycles/write validation, instance state, commit/abort/reset implemented; real-provider phase conformance and checkpoint restore pending | [evaluator](../contracts/EVALUATOR.md), [lifecycle](../architecture/FRAME_LIFECYCLE.md) |
| physics orchestration | Experimental optional C tables implement exclusive world membership, selective participant removal, existing-plan pre/Step/post boundaries, fixed-tick accumulation with retained bounded catch-up, pause, group reset and invalidation after Step begins; generated MMD/Jolt callbacks and two-avatar shared-world smoke pass. Automatic body/constraint teardown, display interpolation, checkpoint restore and shared-world VRM composition remain unimplemented | [evaluator](../contracts/EVALUATOR.md#5-shared-world-physics-orchestration), [lifecycle](../architecture/FRAME_LIFECYCLE.md#5-physics-clock-and-discontinuities) |
| Backend-free VRM session | Core native fixed-tick session and optional installed VRM/clip host implement seeding, bounded catch-up/backlog, pause, reset/seek, transactional retry and retained output; generated and two-version real-avatar owner/pose-transport checks pass. Resident renderer/Hydra parity and scaled spring rigs remain open | [VRM session](../architecture/VRM_SESSION.md) |
| Stage Runner host session | Optional installed `avatarStageRunnerSession` wraps a borrowed or owned runtime world registration with measured display time, play/pause, group reset, zero-elapsed simulation retry, retained last successful output, selective member removal and explicit/idempotent close; no second accumulator. Automatic stage/world construction, application wiring and body/constraint teardown remain open | [host session](../architecture/STAGE_RUNNER_SESSION.md) |
| MMD control/coupling registration | Optional installed `avatarMmdAdapter` invokes owner BeginPhysics -> path-keyed kinematic targets -> caller-owned world snapshot -> feedback -> ResumePhysics; explicit joint/layout/root placement, owner-resolved vertex weights, primary UV values, typed material overrides and visibility mapping; generated PMX/VMD/Jolt headless and installed-consumer tests pass. Material snapshot scope is recorded [below](#mmd-material-snapshot-evidence-2026-10-27); live/rendered material realization, generic retarget intake, automatic host/renderer binding and avatar L3 acceptance remain open | [MMD adapter](../architecture/MMD_ADAPTER.md) |
| versioned C ABI | revision-4/size validation, revision-1–3 rejection, scoped handles, retain/release and installed C provider-consumer tests; ABI freeze pending | [ABI](../contracts/ABI.md) |
| capability negotiation and diagnostics | exact-version provider/binding intersection, required support checks, ordered bounded diagnostics and provider provenance implemented; semantic vocabulary/output negotiation pending | [capabilities/diagnostics](../contracts/CAPABILITIES_AND_DIAGNOSTICS.md) |
| motion validation boundary | installed owner clip/retarget validators invoked before retargeter construction; selected timestamp/scalar/gaze validation delegated by MotionPoseInputBridge; runtime binding/clock/C-string checks retained; owned MotionValidationError and synchronous C sink forwarding preserve owner report identity/order and package version; old InputAssembler source names retained | [motion adapters](../architecture/MOTION_ADAPTER.md) |
| selected USD motion inputs | ReadStageClip returns the strict owner's MotionStageRead directly, forwarding MotionStageReadOptions; legacy StageClip delegates to it; tool attribute selection, clip-to-world rigid gaze placement and untouched world probes implemented; generated native VRMA expression/keyed/default gaze fixtures drive an actual avatar with owner parity; automatic native discovery and representative capture evidence pending | [USD motion binding](../architecture/MOTION_USD_BINDING.md) |
| motion/VRM/MMD/connector integration | optional installed owner motion/VRM adapters and MMD control/coupling registration; constructed motion -> assembled input -> VRM and gaze-space numeric parity tested; schema-derived Humanoid mapping plus seven real VRMA clips on one private avatar via owned USD clip/source rest, with all-joint parity and reset/retention; full avatar bindings, connectors and multi-source selection pending | [motion adapters](../architecture/MOTION_ADAPTER.md), [USD motion binding](../architecture/MOTION_USD_BINDING.md), [VRM adapter](../architecture/VRM_ADAPTER.md), [MMD adapter](../architecture/MMD_ADAPTER.md), [dependencies](../architecture/DEPENDENCIES.md) |
| USD skeleton binding | optional authored skeleton/rest extraction into owned baseline and owner `SkeletonDescriptor`/`RetargetMap`; explicit Humanoid roles, metre conversion, auxiliary joints and rigid root placement; separate optional owner schema Humanoid discovery with custom-role reporting; bounded affine roundoff; constructed USD -> motion -> VRM parity and one local real-avatar skeleton/Humanoid result; scoped expression/output extraction available separately | [USD binding](../architecture/USD_BINDING.md), [VRM USD binding](../architecture/VRM_USD_BINDING.md) |
| Hydra state overlay and direct consumer API | retained direct snapshot API implemented; optional motion-check host consumes installed Toon AvatarState for joint/morph/material probe transport, duplicate/reset/rebind/retention and late draw-value parity; actual resident-avatar binding/rendering, consumer-cost evidence and Hydra/direct parity pending; resident target matching assigned to a renderer-side host, no runtime-side bridge target | [output paths](../architecture/OUTPUT_PATHS.md) |
| USD LookAt binding | separate optional owner schema/raw range-map extraction, head/eye/rest bindings and owned adapter configuration; constructed bone quaternion/expression-weight parity; one private avatar's Expression-type rig with test gaze and actual morph outputs, also composed with seven real clips driving the working head; explicit native keyed/default VRMA fixture gaze intake validated; representative captured gaze pending | [VRM LookAt USD binding](../architecture/VRM_LOOKAT_USD_BINDING.md) |
| USD Expression binding | optional applied owner expression discovery, explicit mesh blend-shape token mapping, indexed/legacy material binds and owned canonical RGB/alpha baseline; all-slot constructed owner parity and one private avatar's 18 expressions/48 morph slots with test scalar/gaze LookAt composition; shared-mesh targets and real-avatar material evidence pending | [VRM Expression USD binding](../architecture/VRM_EXPRESSION_USD_BINDING.md) |
| OpenExec orchestration adapter | proposed; not implemented | [overview](../architecture/OVERVIEW.md) |
| capture, replay, inspection and benchmarks | opt-in local avatar/motion pose checker plus actual LookAt/Expression composition mode, independent owner pose/morph/material oracle and explicit probe/native input counters implemented; recording, comprehensive inspection/replay and benchmarks pending | [USD motion binding](../architecture/MOTION_USD_BINDING.md), [recording/replay](../design/RECORDING_AND_REPLAY.md) |
| multi-avatar execution and parallel scheduling | independent instances tested under serial execution; parallel scheduling pending | [overview](../architecture/OVERVIEW.md) |
| OST composition and published runtime packages | local CMake install/export tested; OST composition and publication not configured | [layout](../architecture/PROJECT_LAYOUT.md) |
| supported native/Web/WASM/XR targets | local Windows x64/MSVC direct prototype evidence only; no other target validation | [roadmap](../roadmap/current.md) |

Update a row only with implementation and validation evidence from this
repository. A sibling's callable library or published package is an integration
input, not proof that this runtime already composes it.

## Native MMD application session evidence (2026-10-27)

The optional installed `avatarMmdSession` (`mmd_session` component) shares the
usdview MMD/Jolt implementation with native hosts. It exposes retained final
joint, vertex/group morph and primary-UV snapshots with optional USD publication;
direct mode leaves the stage unchanged. Its generated snapshot test passes
720 ticks over three generations with exact replay, pause, tick identity, retained-state
lifetime, close/restart refusal and resource teardown. The existing eight
usdview native regressions pass after extraction. See the
[native session contract](../architecture/MMD_SESSION.md) and the renderer's
[live-viewport evidence](https://github.com/animu-sphere/hydra-toon/blob/main/docs/reports/renderer/107-2026-10-27-live-mmd-viewport.md).
Single-avatar wiring does not close general application/participant lifecycle,
representative L3, material output or private-avatar Hydra/direct parity.

## Validation evidence

Dense primary-UV validation now uses an ordered scan and an unordered-layout
fallback over borrowed identity views, retaining duplicate and finite-value
refusals. Copied states share immutable owned identity strings, with independent
numeric arrays/source samples. The configured 22 tests pass, including dense
ordered/reversed multi-mesh layouts, nonadjacent duplicate refusal and retained
snapshots across reset/destruction. The renderer's
[dense-preview report](https://github.com/animu-sphere/hydra-toon/blob/main/docs/reports/renderer/108-2026-10-27-dense-live-mmd.md)
records separate validation, owner and display timings; real-time private-avatar
acceptance remains open.

The direct Release build was checked on Windows x64 with MSVC 19.51 and CMake
4.4.3 on 2026-10-05. No OpenUSD, Hydra, OpenExec or sibling provider target is
linked. `ctest --test-dir build/direct --output-on-failure` runs:

| Test | Evidence |
| --- | --- |
| `contracts.c_provider_consumer` | separate runtime DLL, C11 provider DLL and C11 consumer; revision-4 table negotiation, revision-2/3 rejection, typed gaze transport including stale/unavailable and invalid-direction retry; provider-reported held pose sample and retained gaze sample times; capability discovery and copied layout/input/capability/sample metadata surviving runtime shutdown |
| `contracts.runtime` | C++20 headers; revision-1–3 and undersized (including revision-3-sized) view rejection; vec2 material layouts, writes and nonzero unused-component rejection; retained source samples in input-then-plan order with mapped runtime seconds, stale gaze keeping its time, runtime-stamped evaluator identity, no working-view exposure, prior-view visibility and every invalid/duplicate/undeclared-domain report failing the frame; deterministic plan order; missing/backward/cyclic dependencies; overlapping writers; capability mismatch; stateful failure/retry/reset; ignored/invalid writes; time/input/layout validation; independent instances; absent/zero and complete snapshot behavior; diagnostic provenance and overflow; retained metadata, stable layout across reset, explicit layout version change, prior input revision on failure and domain-independent evaluator metadata; gaze absence/origin, point/direction, world/bound-joint spaces, unchanged stale/unavailable transport, source/actor identity, invalid arrays/references/numerics/clocks/duplicates and failure before provider callbacks with same-frame retry |
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

This is immutable constructed-clip/rig composition evidence. Complete
real-avatar expression/LookAt bindings, live connector intake,
multi-source selection and retained source metadata remain unvalidated; milestones A/B/C and
Runtime Phase A stay open.

The optional USD/motion/VRM Release configuration was checked on the same
date/toolchain with installed `motionRetarget`/`motionSampling` 0.5.3,
`motionCore` 0.5.0, direction-capable `vrmRig` 0.10.0 and OpenUSD 26.08.
`ctest --test-dir build/usd-binding --output-on-failure` passes all nine
core/VRM/motion/USD tests, adding:

The USD-only Release configuration passes five tests, including its installed
consumer, without importing `motionSampling` or `vrmRig`. The default core-only
configuration still passes its three tests and resolves no owner dependency.

| Test | Evidence |
| --- | --- |
| `adapters.usd_skeleton` | authored default-time joint/rest extraction through the motion owner's `BuildSkeletonDescriptor`; explicit owner Humanoid mapping; nested rigid placement, centimetre-to-metre conversion, multiple roots, auxiliary/nonuniform-scale rest; invalid layout/path/mapping/rest/topology/matrix rejection; owned copies surviving stage edits/destruction and retained output surviving runtime destruction; with motion/VRM enabled, root-motion placement once per frame, empty clip returning to placed baseline, reset/repeated-frame behavior and bone LookAt numeric owner parity at `1e-6` |
| `adapters.usd_installed` | separately configured consumer requesting installed `usd` component, optionally `motion vrm`, repeating binding/composition checks without source-tree headers or libraries |

This generic binder test is constructed USD stage and explicit-role mapping
evidence. Separate schema-derived and local real-avatar evidence is scoped below.
Expression/material/deformation extraction, connectors,
rendering and all evidence milestones remain open.

The extended USD/motion/VRM Release configuration also consumes installed
`vrmSchema` 0.10.0, enabling `AVATAR_BUILD_VRM_USD_BINDING`. On the same
date/toolchain, all eleven tests pass, adding:

| Test | Evidence |
| --- | --- |
| `adapters.vrm_usd_humanoid` | all standard motion roles resolved through installed applied schema and owner vocabulary; partial/empty role maps and custom-role reporting; root-scoped discovery and explicit selection; reference remapping and forwarded skeleton relationships; malformed/schema/relationship/joint rejection; immutable copy/stage lifetime and retained state; constructed root movement/head rotation/all-joint TRS parity with the owner at `1e-6` when motion is enabled |
| `adapters.vrm_usd_installed` | separate installed `vrm_usd` consumer resolving the generic USD binding transitively and repeating discovery/ownership/parity checks |

A fresh schema-only Release configuration passes seven tests without importing
`motionSampling` or `vrmRig`. Its installed `usd` consumer also imports no
`vrmSchema`. The default core-only Release configuration still passes three
tests without importing any provider or USD target.

Generic USD tests now include homogeneous-column roundoff acceptance within
`1e-12`, rejection above that tolerance and unchanged authored matrices.
A private user-supplied VRM was opened through installed `usdVrmFileFormat`
0.10.0 without copying or authoring it: 128 joints, 51 mapped standard roles,
zero unsupported roles, constructed root/head motion numeric owner parity
at `1e-6`, and retained snapshot use after runtime destruction. Its path,
hash and local command provenance are kept in ignored build evidence.
This run establishes one real-avatar skeleton/Humanoid result using a
constructed clip. Real-avatar expression/LookAt effects, renderer output,
milestones and ABI freeze remain unvalidated.

The [USD motion clip binding](../architecture/MOTION_USD_BINDING.md) subsequently
adds real-motion pose evidence on this avatar. On the same date/toolchain with
installed local `motionUsd` 0.5.3 and `usdVrmaFileFormat` 0.10.0 resources,
the extended configuration passes all thirteen tests, adding:

| Test | Evidence |
| --- | --- |
| `adapters.motion_usd_clip` | owner stage reading plus source-rest connection; source height subtraction into target rest, source rotation/ancestry, seconds/rate and optional metadata, owned copy/stage lifetime, invalid units/axis/rate/placement/rest/animation and duplicate semantic-role rejection |
| `adapters.motion_usd_installed` | separately built installed `motion_usd` consumer with transitive generic USD binding; no sampling or VRM evaluator/schema import |

The opt-in `avatarMotionCheck` applies all seven supplied VRMA MotionPack clips
to 128 target joints using 51 schema roles, checking 4,129 imported keys and
their midpoints/holds plus resets (8,272 frames). Maximum TRS component error
is `1.403972313e-7`, below `1e-6`; all clips alter numeric pose on 51
joints and pass retained snapshot checks after reset/runtime destruction.
Owner diagnostics preserve unbound driven `upperChest` and the intentional
boundary holds; no error diagnostic occurs. Files are read in place, and paths,
hashes and command logs stay in ignored build evidence. No asset is copied,
authored, redistributed or added to default tests. This is real-motion/avatar
Humanoid pose composition evidence. Native VRMA expression/gaze intake,
live connectors, renderer output, milestones and freeze
remain open.

The [USD LookAt binding](../architecture/VRM_LOOKAT_USD_BINDING.md) was checked
on the same date/toolchain with installed owner packages above. The combined
Release configuration passes all fourteen tests, adding
`adapters.vrm_usd_lookat` for both raw formats, normalized type precedence,
explicit head/eye/rest extraction, malformed bindings, defaults/warnings,
forwarded/reference remapping, stage/copy lifetime and direct-owner quaternion
and expression-weight parity at `1e-6`. The existing
`adapters.vrm_usd_installed` now also builds/runs the explicit `vrm_lookat_usd`
consumer while preserving the `vrm_usd`-only evaluator isolation check.
The fresh LookAt/USD/VRM configuration with motion sampling/source-clip
adapters disabled passes ten tests. Its independent installed LookAt consumer
requests only `vrm_lookat_usd` and resolves no motion sampling/source reader.
Core-only and schema-only regression configurations still pass three and
seven tests respectively, preserving provider dependency separation.

The same private avatar's 128-joint Expression-type LookAt configuration was
extracted read-only with zero parser warnings and tested against owner weights
using test gaze and constructed expression output sinks. Reset and retained
snapshots pass after stage/runtime destruction. Paths, hashes, package versions
and command logs remain in ignored build evidence. Real-avatar bone-eye conformance, real-motion-to-LookAt
composition, connectors and renderer evidence remain open; no milestone or
ABI freeze is claimed.

The optional `AVATAR_MOTION_CHECK_TOON` integration was checked on 2026-10-07
with the same Windows x64/MSVC/OpenUSD Release environment and a separately
built/installed local `Toon::AvatarState` consumer. Its additive `BaseColorRgb`
binding supports canonical RGB plus separate alpha; a configure-time probe
requires that header/API. Only the opt-in host and regression executable link
the renderer. The runtime's full 16-test configuration passes, including
constructed bone/expression LookAt with placed roots, all six material slots,
selected native scalar/gaze inputs and deliberately perturbed transport values.

Seven private VRMA clips on the same 128-joint/48-morph avatar pass 8,286 frames
per mode, with and without host scalar/gaze probes. Three generated owner VRMA
expression/keyed/default-gaze fixtures add 26 frames without probes. All
16,598 frames reach the installed adapter and late draw list; the maximum
matrix/weight normalized error is `4.612073397e-7` against a `2e-6` bound, with
translations compared in metres. Existing owner/state maximum component error
remains `1.403972313e-7`. Duplicate arrays/revisions, reset without structural
rebinding, old-generation/layout rejection, independently retained snapshots
after producer release/destruction and baseline restoration pass. Asset hashes,
commands and logs remain in ignored local evidence.

The resident resources here are an explicit triangle probe, with reversed
palette/morph slots and centimetre units, not the avatar's actual renderer
resources. This avatar has no material binds; constructed fixtures supply
RGB/alpha evidence. The renderer's focused split-RGB/alpha and installed-consumer
tests pass in its owning repository, along with its existing runtime/GPU checks.
A separate Toon-disabled host regression and all three core-only tests pass;
an actual older same-version Toon install is refused by the header probe with
the expected configure diagnostic. Actual mesh/inverse-bind/material
extraction, GPU rendering of the avatar, visibility mapping, Hydra/direct
parity, consumer latency/copies/allocations, milestones and ABI freeze remain
open. See [the scoped transport contract](../architecture/OUTPUT_PATHS.md#scoped-toon-transport-check).

The selected USD input handoff was checked on 2026-10-07 with the same
Windows x64/MSVC/OpenUSD environment. `StageClip` forwards
`MotionStageReadOptions` to the additive strict owner overload; in-tree and
installed-consumer tests preserve owner errors, sparse input timing and owned
values after stage/options destruction. All sixteen runtime tests pass.
The tool's constructed tests cover native-style attributes, origin gaze,
translated/rotated clip placement and unchanged world probes for both LookAt
types, with separate known-coordinate placement assertions.

Three generated VRMA owner fixtures drive the private avatar without probes:
expression, keyed gaze and default gaze produce 26 evaluation frames with
maximum pose/morph error `7.058422424e-8`, observable morph effects, reset and
retained snapshots. Unknown custom expression input is reported as unmapped.
The seven private clips still pass 8,286 frames at `1.403972313e-7`, and their
imported-stage inventory contains no native expression/gaze attributes.
Direct source GLB JSON inspection confirms that all seven
`VRMC_vrm_animation` objects contain only `specVersion` and `humanoid`;
`expressions` and `lookAt` are absent in the source files themselves.
This closes explicit reader/adapter intake with generated-format evidence;
representative captured expression/gaze, native automatic discovery, real
material binds, connectors and renderer evidence remain open. Asset paths,
hashes, arguments and logs stay in ignored local evidence.

A fresh motion-USD-only Release configuration passes seven tests without
importing `motionSampling`, `vrmRig` or `vrmSchema`. The default core-only
configuration still passes three tests without importing USD/provider targets.

The [USD Expression binding](../architecture/VRM_EXPRESSION_USD_BINDING.md)
was checked with the same installed packages/toolchain on 2026-10-06. The
combined Release build passes fifteen tests, adding
`adapters.vrm_usd_expression`: all six owner material slots and alpha at `1e-6`,
override/morph accumulation, alias tokens, indexed/legacy arrays, LookAt
composition, forwarded/reference remapping, malformed/ambiguous/connected
output rejection, zero/absence, stage/copy lifetime, reset and retained snapshots.
The installed VRM USD test additionally configures/builds/runs the explicit
`vrm_expression_usd` consumer, checking that it imports neither LookAt binding
nor motion sampling/source reading before separately requesting LookAt.

The private avatar now has actual expression/output extraction evidence:
18 expressions and 48 morph slots, with no material colour binds. Test scalar/gaze
input through extracted LookAt/Expression bindings matches direct owner outputs
at `1e-6`; stage destruction, reset and retained snapshots pass. Asset identities
and commands remain in ignored build evidence. Real-avatar material binds,
native VRMA expression/gaze intake, connectors, renderer evidence and freeze stay open.

On 2026-10-06, the extended `avatarMotionCheck --vrm` was checked with the same
Windows x64/MSVC Release toolchain and installed owner versions. The combined
build passes sixteen tests, adding `tools.motion_vrm_check`: motion USD -> input
assembly -> actual USD Humanoid/LookAt/Expression bindings -> retained state,
with separate owner comparisons at `1e-6`. Constructed stages cover both LookAt
types, moving root/head pose, nonidentity eye rest, common semantic expression
keys between body keys, all six material slots/alpha, unmapped channels,
zero/absence, stale holds, reset and active/held retained snapshots. Malformed
probe options and deliberately perturbed morph/material outputs are rejected.

Seven private VRMA clips on the same 128-joint, 18-expression, 48-morph avatar
pass both native-only and explicit host scalar/gaze probe modes: 8,286 frames
per mode, maximum component error `1.403972313e-7`, no error diagnostics and
8,265 frames with changed morph output in probe mode. There are no real material
binds on this avatar. Native expression/gaze counters are zero because the
installed common reader does not convert VRMA-specific expression/gaze
attributes; this result does not establish that the original files omit them.
The checker reports the gap and probe identity rather than claiming complete
native input conformance. Paths, hashes, options, versions and command logs
remain in ignored local evidence. This proves real motion pose plus host test
input -> actual-avatar LookAt/Expression composition; native VRMA input mapping,
connectors, renderer output, all evidence milestones and ABI freeze remain open.
The fresh pose-check configuration with the VRM evaluator disabled also builds
and evaluates a constructed motion on the private avatar, rejects `--vrm` with
an explicit build-option diagnostic, and imports no `vrmRig` target.

On 2026-10-06 the motion-boundary cleanup was checked with Windows x64/MSVC
19.51 Release and separately built/installed local `motionCore`,
`motionSampling` and `motionRetarget` 0.5.3 with additive owner validation APIs.
The owner core/sampling/retarget suites pass 5/2/3 tests, including value,
hierarchy, partial-input and dependency-boundary coverage. The runtime's full
optional adapter/tool configuration passes all 16 tests; separate motion-only
and core-only configurations pass 5 and 3 tests. Installed C and C++ consumers
are included in each applicable configuration.

`adapters.motion_pose` and its separately built installed consumer compare
representative malformed clip/rig reports with direct owner validators,
including every code, subject, detail, severity and origin in order. The
owned exception retains the report and installed version after source
configuration destruction; later `Emit` reproduces the same records. The
new input-bridge name and old source aliases compile together; selected-field
validation agrees with the owner, corrected retry/zero/absence pass, and
unused pose-only fields do not become bridge preconditions. Existing
clock/ownership/rollback/reset/retained snapshot and motion -> VRM parity
tests remain green. Runtime binding diagnostics now have runtime origin,
while owner evaluation diagnostics retain motion origin.

The validation extension is local/unpublished; package numbers alone are
insufficient. CMake verifies installed headers and callable linked symbols for
the optional motion component. In-tree and separately configured installed
motion consumers reject pre-extension owner packages with an actionable
configuration diagnostic. Core-only import still resolves no provider or
USD dependency. USD skeleton/source-rest ownership cleanup, real native input,
renderer consumption, milestones and ABI freeze remain open. This change
establishes adapter delegation and forwarding, not independent validation of
motion algorithms or a new core ABI.

The scoped USD reader split was checked on 2026-10-06 with the same Windows
x64/MSVC 19.51/OpenUSD 26.08 Release environment and separately installed local
`motionUsd` 0.5.3 with unreleased strict-reading additions. The owner suite
passes all four tests, including `motionUsd_skeletonReader` and its unchanged
dependency boundary. The full runtime configuration passes all 16 tests;
after adding owner-result/diagnostic assertions, the four USD/source-clip and
installed-consumer tests pass again. These compare rest/parent/placement
mapping directly with owner results and retain refusal code/subject/detail
and owner version after stage destruction.

The optional `usd` component now resolves `motionUsd` for skeleton reading;
`usd`/schema-only consumers still import no source-clip adapter or sampler,
and core-only installed import still resolves no USD/provider target. A
configure-time header/link probe rejects a pre-extension same-version
`motionUsd` install with an actionable error.

The same seven private clips and 128-joint avatar pass both clip-only input
and explicit host scalar/world-gaze probe modes after migration: 8,286 frames
per mode, maximum component error `1.403972313e-7`, all-joint/morph owner parity,
reset and retained active/held snapshots. Asset hashes/commands/logs stay in
ignored local evidence; no asset is copied or authored. Native scalar/gaze
counters remain zero and real material binds remain absent. This preserves
the earlier integration evidence, without closing native intake or any
milestone. Typed owner descriptor/source-rest adoption is described below;
the subsequent direct owner-result host migration is recorded below. No package
release or ABI freeze is claimed.

Typed owner results were adopted and checked on 2026-10-07 with Windows
x64/MSVC 19.51/OpenUSD 26.08 Release and separately installed `motionUsd` 0.5.3
with unreleased typed-reader additions. `SkeletonBinding` consumes the generic
descriptor unchanged and marshals normalized baseline transforms; `StageClip`
retains `MotionStageRead::descriptor` and `sourceRest`, with `SourceRest()`
referencing that owned result. No raw-array descriptor/rest builders remain in
these runtime binders. Tests compare exact owner descriptors, all source-rest
arrays, joint/parent/placement mapping and copy/stage lifetime, and forward
semantic duplicate-bone diagnostics without replacing their subject/detail.

The full runtime configuration passes 16 tests, USD-only passes five, and
motion-USD-only passes seven, including separate installed consumers and
core-only imports without providers. Source and installed USD configurations
reject an older same-version strict-reader package with an actionable typed-API
diagnostic. The same seven private clips pass 8,286 frames per mode with and
without explicit scalar/world-gaze probes, maximum component error
`1.403972313e-7`, owner pose/morph parity, reset and retained snapshots. Asset
paths/hashes and logs remain ignored local evidence. The subsequent host
migration is recorded below; representative native capture, renderer evidence
and ABI freeze remain open.

The owner [v0.5.4 release](https://github.com/animu-sphere/usd-motion-plugins/releases/tag/v0.5.4)
was published on 2026-10-07 after all three OS release lanes passed. All 70
assets, 69 SHA-256 checksums and 33 package pins were checked, together with
tagged source identity and the public `motionUsd` retarget dependency. Runtime
motion/USD source and installed components now require 0.5.4 while retaining
header/link probes for incomplete installations. An older 0.5.3 `motionUsd`
package is rejected at version discovery in both source and installed consumers.

The runtime's Windows configuration was switched to published digest-pinned
`motionCore`, `motionSampling`, `motionRetarget` and `motionUsd` packages fetched
from OCI. Its build and all 16 tests pass, including installed consumers and
core-only lookup isolation. Both seven-clip real-avatar parity modes pass again,
8,286 frames per mode and maximum component error `1.403972313e-7`, with reset
and retained snapshots. Package digests, build/test logs and private-asset
evidence remain in ignored local storage. The experimental runtime C ABI and
remaining milestone gates are unchanged.

The StageClip host migration was checked on 2026-10-10 with Windows
x64/MSVC 19.51/OpenUSD 26.08 Release and the same published motion 0.5.4
packages. `ReadStageClip` now returns the strict owner's `MotionStageRead` by
value; `avatarMotionCheck` uses its clip/source rest directly for registration
and input assembly. The existing `StageClip` delegates to that entry point
and preserves immutable shared-copy/source-rest-alias behavior. No owner
conversion, motion validation or private result type is introduced.

Source and installed source-clip contracts compare the complete owner result,
including populated producer/provenance metadata and a nonempty future-contract
warning. They check independent mutable value copies, stage/options lifetime,
selected scalar/gaze results and preserved refusal code/subject/detail/version.
The full configuration with Toon transport enabled passes all 16 tests; a fresh
motion-USD-only Release build passes all seven, including installed consumers
without importing sampling/VRM and a provider-free core lookup. Compiler
configuration probes used Release after local MSVC Debug probes failed with
PDB version mismatch C1902; no repository toolchain setting was changed.

The same real avatar and seven clips pass both native-input and host-probe
modes through installed Toon transport: 8,286 frames per mode, maximum
pose/morph component error `1.403972313e-7`, Toon transport maximum relative
error `4.851291323e-7`, reset and retained snapshots. Asset hashes, commands
and logs remain in ignored local evidence.
This validates the scoped workstream B host migration while retaining the
legacy wrapper for source compatibility. Representative native expression/gaze,
real material binds/resident avatar rendering, milestones and ABI freeze
remain open.

Experimental ABI revision 4 was checked on 2026-10-10 with Windows x64/MSVC
19.51/CMake 4.4.3/OpenUSD 26.08 Release and the same published motion 0.5.4
packages. It adds `AR_VALUE_VEC2`, retained `ArSourceSample` records and
`ArStateWriter.report_sample`. The three core tests and the full 16-test
configuration pass without compiler warnings, including installed consumers.
`ClipPoseAdapter` reports interpolated samples at the requested clip time and
held samples at the boundary sample's own time.

The installed `Toon::AvatarState` consumer pins revision 3 at compile time, so
the configure probe now refuses that install with an explicit ABI diagnostic.
Toon evidence used a scratch export of `hydra-toon` 3bc1d34 with only that pin
changed to 4. Its 14 tests, including runtime lifetime against this runtime,
pass; a published renderer change is still required. With that consumer, the
seven private clips pass both modes: 8,286 frames per mode, maximum component
error `1.403972313e-7`, Toon maximum relative error `4.851291323e-7`. Every
snapshot carries one retained pose sample that matches owner `SampleClip`
status and time, plus one record per selected input; each clip holds five
boundary samples. Asset hashes, commands and logs remain in ignored local
evidence. Representative captured input ages, consumer latency evidence and
ABI freeze remain open.

## Scoped MMD/Jolt evidence (2026-10-21)

Windows x64, MSVC 19.51, Release, OpenUSD 26.08; runtime base `b59f452`
plus this change; installed MMD base `2806fc4` (0.2.0); installed physics
packages with descriptor contract 1 and Jolt available. These are local build
results, not hosted CI, published dependency or representative-avatar evidence.

The seven tests in `build/mmd-physics` pass, including four core contracts and:

| Test | Evidence |
| --- | --- |
| `adapters.mmd` | Owner pre/post calls in the same constraints phase with explicit dependency; two instances, reversed selection/input order, all prefixes -> one Step -> all suffixes; separate 60/80-degree feedback, parent-local translation retention and post append in final snapshots; rotated/translated root placement and unrelated runtime joint; zero/multiple ticks, pause, retained snapshots, three reset/replays, pre failure retry and post failure invalidation; owner diagnostic identity/subject/context; layout/parent refusals, explicit scalar morph/visibility output and unmapped-output refusal |
| `adapters.mmd_installed` | Separate installed `AvatarRuntime` `mmd` component configure/build/run repeating the constructed tests; core-only installed C lookup rejects imported MMD/physics/provider targets |
| `adapters.mmd_jolt` | Generated `coupling-modes.pmx` + `sample.vmd` -> real owner continuation/coupling -> installed Jolt body handles -> final CPU pose; two per-instance path maps in one solver, all kinematic targets before one Step, all body snapshots after it; 24 ticks at 120 Hz, three complete solver reset/replays; every final joint matches direct owner continuation/feedback, the associated mode-1 body now drives root position, final feedback and post append are nonzero; finite body/bone transforms and same-build exact pose replay; see the [corrected feedback evidence](#mmd-mode-1-feedback-correction-2026-10-25) |

The generated Jolt scene has no joints and deliberately excludes contact pairs.
It starts dynamic bodies at a declared 60-degree perturbation and applies a
small linear velocity to expose body state. It verifies coupling and scheduling,
not physical settling, contacts, spring/limit fidelity or L3 acceptance. The
consumer reads the final retained CPU snapshot; renderer/Hydra parity, resident
avatar resources, stage-runner host integration and real assets remain open.
No MMD control algorithm or generic solver logic moved into runtime core.

## Scoped MMD/Toon evidence (2026-10-21)

Runtime base `4d7279c` plus the transport tests; the same installed MMD/physics
inputs as the scoped MMD/Jolt evidence above. Windows x64, MSVC 19.51, Release,
OpenUSD 26.08 and a local installed `Toon::AvatarState` built for runtime ABI 4.
The fresh `build/mmd-toon` configuration passes nine tests: the seven existing
core/MMD/Jolt tests and the two additional consumer tests below. Installed MMD
component lookup also rejects renderer target imports. An older installed Toon
header pinning ABI 3 is refused at configure time by `AR_MMD_TOON_ABI`.

| Test | Evidence |
| --- | --- |
| `adapters.mmd_toon` | Two distinct constructed final poses, rotated/translated placement, unrelated leading runtime joint, reversed renderer palette, nonzero scalar morph and hidden/visible output; pause, multiple ticks, failed post-Step publication and reset; frame/layout identity, duplicate array reuse and retained first/current outputs after runtime destruction; deliberately corrupt palette rejected by the oracle |
| `adapters.mmd_jolt_toon` | Both instances' final generated PMX/VMD/Jolt snapshots reach the installed renderer adapter and extracted draws over 24 ticks at 120 Hz, three solver reset/replays; owner feedback/ResumePhysics parity remains asserted before transport; first/current snapshots remain usable after runtime destruction and obsolete generations are refused atomically |

Palette comparisons use an independent double-precision USD matrix oracle and
`abs(actual-expected) <= 2e-6 * max(1, abs(expected))`, with translation compared
in metres. Morph values equal the expected float conversion. The test uses
centimetre scene units, explicit identity inverse binds and generated triangle
resources; scalar morph/visibility changes have constructed evidence, while the
Jolt run validates final pose transport. Material/UV/impulse morph realization,
resident-avatar resources, stage-runner composition, rendering, Hydra parity and
representative L3 acceptance remain open. These are local tests, not published
package or hosted CI evidence. See the
[output boundary](../architecture/OUTPUT_PATHS.md#scoped-mmd-transport-check).

## Scoped Stage Runner clock evidence (2026-10-21)

Runtime base `3d772a5` plus the test-host change; installed Stage Runner
`runtimeCore` from `a81bb28`; installed MMD, physics and Toon inputs match the
MMD/Toon evidence above. Windows x64, MSVC 19.51, Release, OpenUSD 26.08.
The opt-in `build/mmd-stage-runner` configuration passes all ten tests, including
`adapters.mmd_stage_runner`; the clock owner's three standalone tests pass.
These are local installed-package results, without a production API/ABI change.

The injected installed `FrameClock` drives display requests with zero, one and
multiple runtime ticks at 120 Hz. Two avatars and one separate falling body
share one Jolt world. Both prefixes precede exactly one Step and both suffixes
follow it for every committed tick. A 100 ms request runs four of twelve due
ticks, retains eight and drains them with zero-elapsed requests. Ten paused
wall seconds neither step nor drain backlog; resume adds none of that time.
Every prefix receives the held input identity/value/source clock and revision,
including explicit zero and a later revised value; the host template's tick
identity/time fields remain unchanged. Each delivered final pose matches direct
owner feedback/ResumePhysics, then the installed Toon consumer's existing
palette/morph/visibility oracle.

Three 24-tick reset/replays reproduce final avatar poses and the separate body's
position/velocity exactly on this build. Reset clears a partial tick and
reconstructs all resources together. A failed suffix returns no snapshots and
blocks further advancement until group reset; a failed prefix retries accepted
time with zero additional elapsed. Retained snapshots survive reset and the
renderer retains first/current outputs after runtime destruction. Installed MMD
lookup also checks that no Stage Runner target is imported.

Only a test host composes the clock. `StageSession`, interactive applications,
reusable production world binding, resident avatar mesh/material resources,
contacts/joints, Hydra agreement and representative L3 acceptance remain open.
No renderer dependency enters the MMD adapter or runtime core, and the host
uses no second fixed-step accumulator. Build switches and the dependency
boundary belong to the [MMD adapter](../architecture/MMD_ADAPTER.md).

## Scoped Stage Runner session evidence (2026-10-21)

Runtime base `afbff13` plus the host-session implementation. The installed
clock, MMD, physics, Jolt and ABI-4 Toon inputs match the preceding clock
evidence. Windows x64, MSVC 19.51, Release, OpenUSD 26.08. The existing
`build/mmd-stage-runner` configuration, additionally enabling
`AVATAR_BUILD_STAGE_RUNNER_SESSION`, passes all twelve tests. A fresh
`build/stage-runner-session-only` configuration passes all six tests with
only installed `runtimeCore` and no format, OpenUSD, physics or renderer prefix.
These are local build/install results, without published or hosted claims.

`adapters.stage_runner_session` exercises the reusable target with an injected
clock and actual runtime barrier. Repeated play preserves accumulated active
wall time; pause discards ten wall seconds without draining backlog. Post-Step
failure and rejected/failed reset preserve the last displayable snapshot set;
further simulation remains invalid until successful group reset. Reset pauses,
clears output and restarts with the supplied increasing generation. A pre-Step
failure retries with zero new elapsed after ten wall seconds, completing only
the already accepted tick. Independently retained snapshots remain valid after
session and runtime destruction.

`adapters.stage_runner_installed` builds and executes the same contract in a
separate consumer using `COMPONENTS stage_runner`, rejecting accidental imports
of MMD, OpenUSD, physics and Toon targets. Core-only installed lookup rejects
both session and Stage Runner targets. The generated two-avatar/non-avatar
MMD/Jolt/Toon smoke now drives this production session target, preserving all
previous barrier, held-input, three-reset/replay, owner-pose and Toon matrix
checks. It additionally checks that a failed suffix preserves the session's
previous display output and retries a failed prefix through `RetryAdvance`.

The [session boundary](../architecture/STAGE_RUNNER_SESSION.md) owns public API,
lifetime and build instructions. World resources are still explicitly built
by the test host. Automatic world binding, `StageSession`, application/UI
composition, resident PMX resources, Hydra agreement and representative L3
acceptance remain open; the session target does not complete those gates.

## Scoped session world-lifetime evidence (2026-10-21)

Runtime base `62b869e` plus the owned-registration/close changes. The same local
Windows/MSVC Release configurations pass six session-only tests and twelve
MMD/Stage Runner/Jolt/Toon tests. `adapters.stage_runner_installed` also compiles
and executes the new constructor and Close API from installed headers/library
without provider, OpenUSD, physics or renderer targets.

The session contract test checks two-member registration, failed construction
without leaked membership, repeated close, closed-call refusal without clock or
callback activity, removal on destruction, explicit instance reset and new
session registration under the same world ID. Another world in the same runtime
continues advancing. Closing a borrowed registration leaves membership intact.
Retained snapshots keep the original instance/generation after member removal.

The generated MMD host now uses the owning constructor. After the existing
owner-pose, held-input, reset/replay and renderer transport checks, it closes
the session before destroying both instances and releasing all Jolt bodies and
the solver. No step/reset/provider callback occurs on close or a subsequent
display request. Independently retained poses remain readable, and installed
Toon output survives producer destruction. Physics resources and instance
discovery are still supplied explicitly by this test host.

This validates whole-world registration lifetime, not selective removal from a
running shared world. Automatic stage binding, application/UI wiring, resident
avatar rendering, Hydra agreement and representative L3 acceptance remain open.

## Scoped selective-unload evidence (2026-10-21)

Runtime base `8ee3170` plus the selective-membership/session changes. Local
Windows x64/MSVC 19.51 Release checks use the same installed clock, MMD, physics/Jolt and ABI-4 Toon
inputs as the world-lifetime evidence. The session-only configuration passes
six tests and the MMD/Stage Runner configuration passes twelve. The C11 core
installed consumer negotiates the new table; the separately compiled session
consumer exercises removal without OpenUSD, MMD, physics or renderer targets.

`contracts.physics` validates removal with pending partial time, untouched
survivor generation/tick/request history, reset-required detached reuse,
immediate instance destruction, retained output, exact remaining input coverage,
zero-avatar stepping and an unaffected second world. Invalid handles, another
world's member, repeated removal, callback reentry and poisoned-group removal
refuse without changing membership. The session contract checks no clock read
or playback change, survivor output ordering and retained handles, unchanged
advance metadata, reset after instance destruction and closed-call refusal.

The generated MMD host removes one participant through `RemoveParticipant`,
then destroys its three Jolt bodies and shapes, clears its command/result/input
rows and destroys the instance. The same solver, surviving body handles and
non-avatar position remain intact at removal. Three subsequent ticks have one
prefix, one shared Step and one suffix each. Final surviving pose exactly
matches owner feedback/ResumePhysics and reaches the installed Toon oracle;
the non-avatar body keeps falling. A group reset rebuilds only surviving body
maps, and retained removed-avatar CPU/Toon output survives world/runtime teardown.

This covers scheduler membership and explicit teardown in a contact-disabled,
joint-free generated host. Automatic body/constraint teardown, cross-avatar
constraint policy, provider unregister, stage discovery, interactive application
wiring, resident renderer resources and representative L3 acceptance remain
open. The [lifecycle](../architecture/FRAME_LIFECYCLE.md#experimental-fixed-tick-requests)
and [session](../architecture/STAGE_RUNNER_SESSION.md) own API/ownership rules.

## Scoped physics scene resource evidence (2026-10-21)

Runtime base `9cb59d9` plus the optional `avatarPhysicsUsdResources` changes.
Local Windows x64/MSVC 19.51 Release/OpenUSD 26.08 checks use installed physics
packages, with no sibling source compiled into runtime. The MMD/Stage Runner/
Jolt/Toon configuration passes fourteen tests; a physics-resource-only build
passes six, and the physics-free session-only build passes six. These are local
source/install checks without publication or hosted CI claims.

`adapters.physics_usd_resources` checks identical paths in independent resource
sets, exact endpoint/handle mapping, world anchors, fixed-joint local frames,
disabled joints, body policy and snapshot lifetime. All eight backend creation
positions fail by exception and by invalid handle in separate runs; rollback
leaves an existing resource set's handles/resources intact. Invalid paths,
duplicate identities, external endpoints, unsupported limits, malformed shapes,
missing capability and policy exceptions reject before any backend creation.
Constraint/body/shape teardown refusals stop before their dependencies and allow
explicit retry; successful repeated close and destruction leave no owned handles.
No step, state query, force or velocity operation occurs in this component.

`adapters.physics_usd_installed` separately builds and executes that contract
through `COMPONENTS physics_usd`, rejecting accidental MMD, backend, Stage Runner
or renderer imports. Core/session-only installed consumers additionally reject
the new resource target and physics USD targets.

The generated MMD host now constructs imported body snapshots through the
reusable resource target. A second selective-unload run authors a standard
world joint into the stage's in-memory session layer; the installed reader
discovers it on initial construction and reset. Removing one avatar closes its
constraint/body/shape set after membership removal. Existing final-pose owner
parity, Toon transport, retained output, shared non-avatar stepping and survivor
reset checks pass. After removal the survivor runs another 120 fixed ticks:
its constrained body remains within 0.27 metres of the world anchor's Y for
the declared +/-0.25 metre limit and moves downward by more than 0.1 metres.
Contacts remain disabled; these are generated joint-lifetime/bounded-execution
checks rather than representative avatar acceptance.

The [resource contract](../architecture/PHYSICS_USD_RESOURCES.md) owns preparation,
rollback and teardown rules. Automatic application/stage discovery, solver
selection, avatar configuration, cross-avatar constraints, resident rendering,
Hydra agreement and representative L3 acceptance remain open.

## Scoped stage-world binding evidence (2026-10-21)

Runtime base `7f8858b` plus the `StageWorld` changes, using the same installed
physics/MMD/Jolt/clock/ABI-4 Toon inputs as the resource-set evidence above.
Local Windows x64/MSVC 19.51 Release/OpenUSD 26.08 checks pass fourteen tests in
the composed configuration and six in the physics-resource-only configuration.
No sibling source is compiled into the consumer. These are source/install
checks without publication, hosted CI or representative L3 claims.

The source and separately built installed `physics_usd` contracts exercise the
whole-stage scope with a caller factory. Owner scene identity and gravity reach
the factory unchanged; absent scene settings stay absent. Invalid stages,
multiple scenes and unsupported units fail before factory invocation. Missing,
throwing and null-returning factories reject. Policy failure and all three
backend creation positions, by exception and invalid handle, destroy the new
world only after rolling back owned resources. A teardown refusal keeps the
world alive for explicit retry; successful repeated close and destructor cleanup
release resources before the world. Stage mutation/destruction leaves the
snapshot binding intact. A generated joint between different root bodies maps
both endpoints into the single whole-stage scope. No world step/query is made
by the adapter; mock operations abort on accidental calls.

`adapters.mmd_stage_runner` additionally reads a copied generated PMX stage with
an in-memory standard PhysicsScene and uses the reusable scope with the installed
Jolt factory. The declared gravity is passed to Jolt, all three source bodies are
discovered, and the host advances 120 fixed ticks. A selected dynamic body's
downward displacement is finite and between 0.5 and 1.5 metres with 2 m/s^2
gravity. Closing and reconstructing the complete scope reproduces that position
exactly on this build. The existing two-avatar clock/barrier/reset/unload and
Toon snapshot transport checks still pass; their explicit participant binding
is unchanged. This additional test establishes stage/factory/resource composition,
not automatic runtime participant discovery or full application wiring.

The [whole-stage contract](../architecture/PHYSICS_USD_RESOURCES.md#whole-stage-world-binding)
owns factory selection and lifetime. Backend policy, application events,
automatic avatar configuration, selective unload with cross-avatar constraints,
resident renderer resources, Hydra agreement and representative L3 remain open.

## Live UsdSkel and usdview MMD evidence (2026-10-21)

Runtime base `901fc3f` plus local changes; Windows x64, MSVC 19.51 Release,
Python 3.13.14 and OpenUSD 26.08 with Qt/usdview. Installed MMD 0.2,
physics/Jolt and Stage Runner clock packages are the preceding stage-world
inputs; USD binding uses installed motion owner 0.5.4. No sibling source enters
the build. The live-host source/install configuration passes fourteen CTests.
This is local evidence, without publication, hosted CI or representative L3 claims.

`adapters.usd_pose_output`, also built/run by the separate installed USD
consumer, validates pose-only publication with centimetre units, translated/
rotated placement, multiple roots and nonuniform child scale. A UsdSkel query
at an arbitrary timeline frame recovers the expected skeleton-local transforms
within `2e-5`. Malformed transforms, narrowing overflow, changed units/topology/
placement, layout mismatch, another instance, older output and a masking
session-root binding reject before changing the previous live layer. Close
restores the preceding animation, preserves foreign sublayers/offsets and leaves
persistent layer text unchanged. Retained CPU state outlives producer/output.
The [output contract](../architecture/USD_POSE_OUTPUT.md) owns those rules.

`hosts.usdview_mmd_native` uses the generated MMD `live-physics-pendulum.pmx`:
two bones/bodies, one pivot joint and a skinned four-vertex bar. Three 120-tick
live replays reproduce the final joint arrays exactly on this build. Gravity
produces a driving-bone quaternion Z magnitude over 0.1; UsdSkel skinning moves
a bound mesh vertex by over 0.1 metres. Pause admits no simulation time, reset
rebuilds the entire world/generation, and repeated close restores the session
and persistent layer text. Publication contains no authored time samples.

The opt-in `tools/usdviewMmdPhysics/test_usdview.py` opens an actual usdview
window, verifies its menu, drives real Qt-timer Play/Pause/Reset/resume/Stop,
and checks final physics-driven rotation through the composed UsdSkel query.
Stage replacement additionally stops its timer/session and cleans the old stage.
The [guide](../guides/live-mmd-physics.md) owns reproduction and UI behavior.

That initial check disabled contacts/body damping and reported preserved
filter/contact/damping/spring metadata as unapplied. The body-properties
follow-up below extends that scope. Output is joints
only, with unsupported owner morph/hidden channels refusing through the MMD
adapter. Representative PMX/VMD intake, spring semantics, multi-avatar
application binding, seek, full material/morph/visibility output, resident Toon
resources and Hydra/direct agreement remain open. The local application uses
usdview's default renderer; hydra-toon rendering/pixel parity is not established.

## Live MMD body properties evidence (2026-10-21)

Runtime base `80246f4` plus local changes consumes a separately installed MMD
owner package from base `c11f274` plus its body-properties API. The preceding
live-host environment is unchanged. All fifteen configured source/installed
CTests pass. `hosts.usdview_mmd_properties` isolates contact from joint collision
suppression using in-memory edits over the generated pendulum. It checks
acceptance-mask bits, rejected/admitted drops, restitution, inclined-floor
friction and independent linear/angular damping responses. It also checks
construction/Reset refusal with preserved owner code/path, retained display
after failure, repair/reset recovery, copied body-state lifetime and session
cleanup. The existing native three-replay test and actual-window usdview
Play/Pause/Reset/resume/Stop/stage-replacement check still pass.

The host delegates metadata interpretation and source damping conversion to
installed `mmdPhysicsAdapter::ReadBodyProperties`, maps optional values into
neutral descriptors and selects the owner's product pair policy. It introduces
no solver/provider dependency into core. The MMD owner's
[contract and report](https://github.com/animu-sphere/usd-mmd-plugins/blob/main/docs/reports/2026-10-21-phase8-live-body-properties.md)
record conversion domains, measured controls and approximation boundaries.
The [guide](../guides/live-mmd-physics.md) owns build/run steps and the package
feature-version requirement. The spring follow-up below extends that scope;
representative PMX/VMD, resident Toon and Hydra/direct parity gates remain open.
This is local generated evidence, without publication, hosted CI or L3 claims.

## Live MMD spring evidence (2026-10-21)

Runtime base `f757535` and MMD owner base `45b3827`, plus local changes, extend
the preceding live host using installed `mmdPhysicsAdapter::ReadJointSprings`.
The generic scene resource adapter accepts a validated neutral joint policy
before backend resource creation, with no source-format branch or owner
dependency. Source and installed tests check callback count, disabled omission,
endpoint remapping, descriptor validation and failure before mutation.

The application maps owned SI coefficients to independently default-OFF Hookean
translation and small-angle rotation approximations. Native generated checks
cover gravity-response differences, positive free/limited activation, signed,
locked, missing/other-type and disabled gaps, final UsdSkel/body agreement,
pause/reset replay, changed-stiffness reconstruction, refusal with owner
code/path and layer cleanup. Actual-window Qt playback with the rotation opt-in
also checks final pose agreement, reset/resume/stop and stage replacement.
The owner's [report](https://github.com/animu-sphere/usd-mmd-plugins/blob/main/docs/reports/2026-10-21-phase8-live-springs.md)
records measurements and the [guide](../guides/live-mmd-physics.md) owns commands.

Negative/locked/format-only spring meaning remains unsupported. Applied axes
remain explicitly approximate and do not establish native MMD or finite-angle
equivalence. Representative PMX/VMD, resident Toon and Hydra/direct parity
remain open. This is local generated evidence, without publication, hosted CI
or L3 completion claims.

## Live MMD keyed-input evidence (2026-10-24)

Runtime base `c990824` plus generated-test changes consumes installed packages
from the preceding live-host configuration. `hosts.usdview_mmd_motion` pairs
the generated MMD pendulum with an owner-generated VMD. Three 240-tick runs
check a 10 cm keyed pivot/return, finite body/bone state, a motion-free control,
joint separation below 2 cm, and composed UsdSkel skinning against concrete
solver rotation within `2e-5` metres. Reset reproduces body and joint arrays
exactly; Pause and Close preserve the existing lifetime guarantees. The
[guide](../guides/live-mmd-physics.md#validation) owns reproduction; measured
values and anonymous private-asset intake results are in the
[MMD report](https://github.com/animu-sphere/usd-mmd-plugins/blob/main/docs/reports/2026-10-24-phase8-live-keyed-input.md).

One private model passes a bounded rest-input finite-state/reset smoke. Its
full VMD input refuses with `MMD_ADAPTER_MORPH` because this pose-only host
has no non-bone morph output binding. A second private model refuses owner
damping validation with `MMD_PHYSICS_METADATA_INVALID`. No source opinion is
changed to bypass either refusal. Private inputs, filenames and paths are
excluded from tracked files. This is local numeric evidence; representative
L3, live material/morph output, resident Toon rendering and Hydra/direct
agreement remain open.

## Live MMD vertex morph evidence (2026-10-24)

Runtime base `038074d` plus this change consumes the preceding installed MMD,
motion, physics/Jolt and Stage Runner packages; no owner implementation or pin
changes. `SkeletonPoseOutput` optionally transports one skeleton-bound mesh's
runtime blend-shape weights. Source and installed USD consumer tests cover
identity-based reordering, unrelated channels, unclamped weights, missing,
duplicate, nonfinite and overflowing values, changed mesh binding and atomic
refusal preserving the preceding display. The existing pose-only path remains
covered. The [output contract](../architecture/USD_POSE_OUTPUT.md) owns the API.

`hosts.usdview_mmd_morph` combines generated direct vertex keys with a moving
kinematic pivot and dynamic feedback. Three 240-tick runs reproduce weights,
deformed/skinned points and body states exactly after Reset; pause holds a
nonzero morph weight and Close restores both layer texts. Composed UsdSkel
deformation agrees with the keyed vertex delta followed by concrete solver
rotation within `2e-5` m. The original check refused group and UV tracks;
the [resolved-group follow-up](#live-mmd-resolved-group-evidence-2026-10-24)
now admits vertex groups through the owner API. Material/impulse/visibility
remain outside this host. The [guide](../guides/live-mmd-physics.md#validation) owns reproduction;
the [MMD report](https://github.com/animu-sphere/usd-mmd-plugins/blob/main/docs/reports/2026-10-24-phase8-live-vertex-morph.md)
records generated measurements. This is native numeric evidence; representative
L3, actual-window morph pixels, resident Toon and Hydra/direct agreement remain open.

## Live MMD resolved group evidence (2026-10-24)

Runtime base `4d0d108` plus this change consumes installed MMD
`Evaluator::ResolveMorphChannels` after final control continuation. Raw channels
remain owner data; only resolved leaf weights reach deformation slots. Source
and installed MMD adapter tests cover nested groups plus direct contributions,
negative/above-one/zero/absent weights, bone effects applied once, retained
snapshots, pause/reset and refusal of unmapped UV/material/flip/impulse leaves
even at zero, as well as non-finite resolved weights. Refusal publishes no
partial snapshot and requires world reset after Step. Source and installed
component configuration probe owner headers and linked symbols; older owner
headers refuse with a rebuild instruction. Core-only consumers remain isolated.

The live morph check now runs direct, group-only and mixed direct/group VMDs,
each for three 240-tick replays. The mixed input combines a group weight of 2
with a direct vertex weight of -0.5 at its peak; the resolved result is 0.5,
without adding raw inputs again. It verifies the final UsdSkel deformation and
skinning against the concrete body transform, exact reset replay, paused output,
return to zero and layer restoration. UV still refuses. See the
[MMD integration report](https://github.com/animu-sphere/usd-mmd-plugins/blob/main/docs/reports/2026-10-24-phase8-live-resolved-morph.md)
for measurements and the [guide](../guides/live-mmd-physics.md#validation)
for commands. This generated numeric scope does not establish private full-motion,
UV/material/flip/impulse execution, representative L3, actual-window morph pixels,
resident Toon binding or Hydra/direct agreement.

## MMD mode-1 feedback correction (2026-10-25)

Runtime base `20e2e42` consumes the corrected installed MMD owner above the
unchanged adapter/ABI. The original no-feedback mode-1 expectation was incorrect.
Source/installed adapter checks now verify physics-driven root translation,
updated parent-relative rotation, post-physics append and bone-morph composition.
The generated Jolt check retains its three 24-tick shared-world reset replays.

`hosts.usdview_mmd_dynamic` overrides only the generated pendulum's composed
swing mode to `dynamic` and Y translation limits to +/-0.2 m. Three 240-tick
replays verify position/rotation against
the concrete body after rest-offset removal, nonzero local translation, pause,
exact reset replay and layer restoration, with both spring options OFF.
The existing mode-2 generated checks remain unchanged. The live-host
configuration passes all 19 CTests; the separate installed Jolt/Toon/Stage
Runner configuration passes all 14 tests, including final renderer transport
and shared-world host lifecycle checks.

An anonymous local character's 123 associated mode-1 bones all change local
rotation during the same bone/IK-only preview, including descendants of mode-2
roots. Three 600-tick replays reproduce published pose and body state exactly;
all exposed body values are finite and all mode-1 published transforms agree
with concrete body/rest-offset transforms within `2e-5` m and quaternion-dot
error `1e-5`. Original source/session layers are restored. This local preview
omits morph tracks in a separate derived VMD; it is not full-motion or L3
acceptance and no private source names, assets or screenshots are committed.
Measurements and ownership are in the
[MMD correction report](https://github.com/animu-sphere/usd-mmd-plugins/blob/main/docs/reports/2026-10-25-phase8-dynamic-bone-feedback.md).
Representative springs, contact/recovery behavior, full morph output, resident
Toon resources and Hydra/direct agreement remain open.

## MMD start-pose initialization (2026-10-25)

The live usdview host consumes the installed MMD owner's initial-pose query
before constructing the world and constraints. All associated modes start at
evaluated source frame zero; Reset recreates the same seed with fresh zero
velocity. Unassociated bodies retain authored placement. There is no hidden
Step, warm-up or clock shift; Pause/resume retains the current solver state.
The reusable provider/core ABI and solver interfaces are unchanged.

`hosts.usdview_mmd_startup` covers generated translation, root rotation and
child-pose starts, pre-Step world transforms and zero linear velocities,
translation-invariant trajectories, three exact 240-tick reset replays per
input, pause and persistent/session layer restoration. All 20 live-host CTests
pass. An anonymous bone/IK-only local preview retains exact 600-tick reset
replays and mode-1 output agreement; its derived motion omits morph tracks.
The [MMD report](https://github.com/animu-sphere/usd-mmd-plugins/blob/main/docs/reports/2026-10-25-phase8-start-pose-initialization.md)
owns measurements and environment, and the
[guide](../guides/live-mmd-physics.md#validation) owns reproduction.
Initial overlap/contact recovery, representative springs, arbitrary seeking,
full-motion and L3 acceptance remain open.

## Live primary UV evidence (2026-10-25)

Experimental ABI revision 5 retains absolute primary vertex UV values. The MMD
adapter consumes installed owner deltas against immutable bases, and the live
USD output publishes unindexed vertex `st` with joints/vertex weights in one
session layer. Generated direct and mixed group/direct scenarios each pass
three 240-tick replays, including signed/above-one weights, base restoration,
pause and layer cleanup. Source and installed adapter checks cover identity
mapping, malformed reached-zero offsets, additional-channel refusal and retained
snapshot lifetime. The live-host configuration's 21 CTests pass.

See the [state contract](../contracts/EVALUATED_STATE.md#primary-vertex-uvs),
[USD output](../architecture/USD_POSE_OUTPUT.md) and
[MMD evidence](https://github.com/animu-sphere/usd-mmd-plugins/blob/main/docs/reports/2026-10-25-phase8-live-primary-uv.md).
Additional UVs, materials, automatic resident Toon UV mapping and Hydra/direct
agreement remain open. Rebuild providers and consumers for ABI 5; older binaries
reject negotiation. See the later scoped direct-consumer evidence below.

## MMD primary UV direct-consumer evidence (2026-10-26)

The opt-in MMD/Toon host consumes the installed renderer's explicit primary UV
bindings alongside final pose/morph/visibility. Generated owner group/direct
resolution reaches reordered renderer UV slots for two instances over three
reset generations. Duplicate arrays/revisions and retained snapshots after
producer destruction agree. All nine CTests in this configuration pass.
The renderer's separate generated MMD/MToon Vulkan checks establish texture
response and zero restoration, not composed MMD-to-GPU or Hydra parity.
The [MMD report](https://github.com/animu-sphere/usd-mmd-plugins/blob/main/docs/reports/2026-10-26-phase8-toon-primary-uv.md)
records reproduction and limits. Automatic resident mapping, tangent
regeneration, additional UV/material output and representative L3 remain open.

## MMD material snapshot evidence (2026-10-27)

Runtime base `a76aaf4` plus this change calls installed owner
`EvaluateMaterialMorphs` on raw final pose channels and publishes complete
typed overrides through explicit field/input bindings. Configuration and
publication requirements belong to the
[MMD adapter](../architecture/MMD_ADAPTER.md#configuration-and-ownership).
No core ABI, phase, owner semantics or dependency pin changed.

Generated source and installed-consumer tests exercise all 28 components for
two materials and two instances, group-plus-direct weights, separate sampled
tints, all-material/explicit targets, reordered inputs, unrelated input
preservation, repeated-frame nonaccumulation, zero/absent weights and three
reset generations. Retained snapshots survive producer destruction. A final
float overflow after a successful tick publishes no new snapshot, preserves
the retained frame and recovers through whole-world reset. Missing/mistyped
outputs, unbound reached-zero materials, owner-invalid offsets and ambiguous
or incomplete bindings refuse. The material fixture uses caller-synthesized
completed body transforms, not a material-bearing Jolt/rendered asset.

The MMD-only configuration's six CTests and the extended configuration's nine
CTests pass, including the existing generated PMX/VMD/Jolt and Toon transport
regressions. Material rendering is not part of those Toon checks. See the
[MMD evidence report](https://github.com/animu-sphere/usd-mmd-plugins/blob/main/docs/reports/2026-10-27-phase8-material-snapshots.md)
for reproduction. Automatic material discovery, live USD material output,
renderer tint consumption, representative private-input parity and L3 remain open.

## Live MMD material evidence (2026-10-29)

The [native MMD session](../architecture/MMD_SESSION.md) now binds direct-PMX
stage-contract v2 materials and delivers six canonical fields plus six vec4
sampled-tint coefficients per material. The original resolved-tint adapter
configuration remains compatible; live consumers use independent multiply/add
inputs, preserving `sample*M+A` instead of evaluating it against a white texel.
No core C ABI, phase, owner evaluation rule or schema is changed.

Generated `hosts.mmd_session_materials` compares all 40 components of each of
two materials against direct installed-owner evaluation across group/direct
weights, all-material multiplication, explicit addition, negative weights and
zero restoration. Both direct-only and USD-enabled modes run three exact
240-tick reset generations (1440 Jolt ticks total), with paused Step counts,
retained output through destruction and source/session-layer restoration.
USD compares the six canonical inputs with float tolerance `1e-6`; the owner
defines no USD input for sampled tints. `adapters.usd_pose_output` verifies
typed numeric delivery, reordered slots, malformed/missing/duplicate inputs,
float overflow and masking session-root opinions without partial publication.
The complete enabled Windows Release configuration passes 24 CTests, including
installed core, adapter and USD consumers and the existing live regressions.

The renderer consumes the installed `mmd_session` package through automatic
resident matching. Its [material delivery report](https://github.com/animu-sphere/hydra-toon/blob/main/docs/reports/renderer/111-2026-10-29-live-mmd-materials.md)
owns six passing renderer checks, sampled-texture GPU evidence and 120-frame
generated viewport delivery. Representative private assets, broader discovery,
additional UV channels, full Hydra/direct numerical/pixel parity and L3 remain
open; this does not close physics milestone D or the ABI-freeze gates.

## VRM spring stage binding evidence (2026-10-28)

The optional [spring USD binding](../architecture/VRM_SPRING_USD_BINDING.md)
reads canonical applied spring/collider schemas into installed owner values,
using an explicit skeleton and layout. Windows Release tests cover generated
VRM 1.0 chains and VRM 0.x branching roots, sphere/capsule fields, parallel
parameters, center and optional collider groups, forwarded relationships and
reference remapping. Joint/group resolution failures, ambiguous declarations,
blocked/mismatched arrays, unknown collider shapes, unsupported units/scales,
contract-v1 input and owner-rejected topology/parameters fail explicitly.

`adapters.vrm_spring_usd` compares the bound rest/placement against independent
fixture values, including a rotated avatar placement, then compares 12 steps
of bound versus direct owner evaluation (`1e-5 m` tails, `1e-5` absolute
quaternion-dot difference). Copies and all returned configuration remain usable
after source prim removal and stage destruction. Owner refusal detail, subject,
package identity and version are retained by `SpringBoneOwnerError`.
Numerical reference trajectories remain the solver owner's evidence.

`adapters.vrm_spring_usd_installed` repeats these checks through installed
`AvatarRuntime::avatarVrmSpringUsdBinding`, first checking that core-only
package lookup imports no owner. It also checks that `vrm_spring_usd` imports
neither expression registration nor motion sampling. Both source and installed
configure refuse an older solver install without the feature API and an older
schema install without `rootJoints`, even when the package version matches.
The existing expression-only option still builds and passes its six tests
against that older solver install. Core and existing USD contracts also pass.

This is local Windows stage-binding evidence. Hosted runtime lanes, actual
`.vrm` intake, fixed-tick scheduling, live avatars, and Hydra/direct output
parity remain open. The following evaluator evidence adds stateful runtime
callbacks and rollback. No runtime phase, physics milestone or ABI freeze gate
is closed.

## VRM spring evaluator evidence (2026-10-28)

The optional [spring evaluator](../architecture/VRM_SPRING_ADAPTER.md) calls
installed `vrmRig` through core's existing stateful callbacks in the secondary
phase. Local Windows x86-64 Release source and installed-consumer tests pass
against the owner solver built from `14efdec`, schema contract v2, OpenUSD
26.08 and motion packages 0.5.4. It adds no core provider dependency or C ABI
change. Spring numerical/reference correctness remains with the owner.

`adapters.vrm_spring` compares 25 frames per generated rig against direct owner
Reset/Step calls: a VRM 1.0 chain, a branching VRM 0.x root subtree and a
simulated skeleton root. Working animation includes rotating roots/hair,
translated placement/root motion and a rotated avatar placement. Final rotation
agreement uses absolute quaternion-dot error below `1e-8`. Unsimulated joint
values, translations and other state channels retain their working values.

Failure injection covers initial seeding and a later step followed by a
downstream provider refusal, and a step followed by an ignored invalid writer
call. Retrying the same frame matches a clean independently advanced instance
exactly. Every original frame also matches increasing-generation reset/replay
exactly. Another instance starts from independent history, and retained
snapshots survive reset and runtime destruction. Layout/joint/parent mismatch,
scale, owner-rejected animated non-root translation, duplicate/omitted ticks,
time jumps and same-phase writer conflicts are refused. Owner diagnostic detail,
version, origin and evaluator identity remain observable.

`adapters.vrm_spring_installed` repeats the suite through installed
`AvatarRuntime::avatarVrmSpringAdapter`, checking that core-only lookup imports
no owner and the spring component selects no schema, USD binding, expression or
sampling adapter. With both spring options enabled, `adapters.vrm_spring_usd`
and its installed counterpart pass bound values to the evaluator after stage
destruction: both declaration shapes and sphere/capsule configurations compare
13 seed/tick frames against direct owner evaluation, with retained identity and
snapshot lifetime. The combined configuration's 11 CTests include the existing
core/physics, USD skeleton/output and installed-consumer checks.
Both source and installed configure reject an older owner lacking the spring
API with an explicit admission diagnostic. The existing expression-only build
still passes all six tests against that older owner.

This is generated fixed-instant evaluator evidence. The first request initializes
without stepping; later calls consume one explicit fixed tick. Subsequent native
clock and host evidence is recorded below. Hosted runtime lanes and renderer/Hydra
parity remain open. Neither slice closes physics milestone E, a Runtime Phase or
an ABI-freeze gate.

## VRM fixed-tick session and host evidence (2026-10-29)

Measured on Windows x86-64 / OpenUSD 26.08, using installed `vrmRig` and
contract-v2 `vrmSchema` 0.10.0 and published motion 0.5.4 packages.
`FixedTickSession` is a backend-free core C++ surface over the existing core
transaction and snapshot registry; the C ABI tables/phases are unchanged.
It excludes direct/world clock ownership of the same instance. A declared seed
at tick zero precedes due fixed ticks. The runtime retains excess time with the
same bounded catch-up/pause policy as its physics clock.

`contracts.fixed_tick_session` runs without owner dependencies. Expanded
`adapters.vrm_spring` and `adapters.vrm_spring_installed` exercise both declaration
shapes with zero/one/multiple ticks, pause/resume/backlog, malformed-input and
duplicate-request refusal before elapsed acceptance, failed seed, downstream
provider/writer failure and same-tick retry against an independent control.
A later failure in a multi-tick request preserves the earlier tick's commit,
display snapshot and failed-tick backlog. Reset replays identically, and retained
state survives reset, session close and runtime destruction.

`hosts.vrm_session` and `hosts.vrm_session_installed` compose generated Humanoid,
clip and root/chain spring fixtures with explicit enabled/disabled mode,
play/pause/reset/close, stage-independent lifetime, pose output and retained
unload. The separate installed consumer selects `vrm_session` and checks the
dependency boundary. The combined configuration passes all 20 CTests, including
existing C provider/consumer, physics and installed adapter regressions.

The installed `avatarVrmSessionCheck` also ran each real avatar below with
`VRMA_01.vrma`, at 60 Hz for 120 ticks, in both spring and motion-only modes.
Its comparison uses direct owner motion sampling/retargeting and a separate
owner spring solver, then reads delivered USD local joint TRS/identity. Both
enabled runs and both disabled runs pass exact reset replay and retained unload.

| Avatar | Joints | Motion-changed joints | Spring-changed joints | Max motion component error | Max spring quaternion error | Max USD translation/quaternion error |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| AliciaSolid (VRM 0.x) | 128 | 49 | 48 | `1.01596e-7` | `1.01743e-7` | `7.13532e-8` |
| VRM1 Constraint Twist Sample (VRM 1.0) | 154 | 51 | 57 | `1.02064e-7` | `1.02069e-7` | `7.92478e-8` |

Translation/quaternion tolerance is `1e-6`. USD half-scale quantization is
checked separately with relative `5e-4` tolerance (absolute near zero): maximum
scale errors are zero and `7.15256e-7`, respectively. The 1.0 importer emits its
existing `VRM110` conflicting-inverse-bind warning and uses its declared first
matrix policy; this host does not evaluate the sample's node constraints.
Spring-changed counts compare simulated output with the unsimulated clip pose;
motion-changed counts compare the clip pose with authored rest.

Asset SHA-256 identities, with no asset redistribution:

- AliciaSolid: `237bb02efadf8c13a114af91dd8e860173081457dee87017e51011c448d05dc2`.
- Constraint Twist Sample: `12c2b97e95e700783a6a550dc0eee2d7880aeedccef9ae67bc4c5a2f0f2631a2`.
- VRMA_01: `f45abc44800c647fa106537a2e31a84cb83b74cf3dd8a0ee71c6915f1555ac57`.

`tools/check-vrm-session.ps1` records rerunnable asset hashes/outcomes. The local
run's JSON/logs are under ignored `build/vrm-session-real-evidence.json` and
`build/vrm-session-real.log`. Importer binary/resources were refreshed to contract
v2 in a local staged prefix; the older staged importer was correctly refused by
`VRM_SPRING_BINDING_CONTRACT`. Owner libraries are installed packages, not sibling
source additions to the host build.

Seed-san is explicitly refused by `VRM_SPRING_BINDING_SCALE` at
`hips/spine/chest/bag_rope_L`. Its motion-only 120-tick run passes owner/pose
transport and exact reset/unload: motion error `1.12983e-7`, translation/quaternion
output error `1.01355e-7`, and half-scale quantization `2.35081e-4`. No scale is
discarded to admit springs. Renderer resident-resource delivery, Hydra/direct
agreement, pixels, interpolation, in-place mode changes, broader avatar/motion
coverage, hosted runtime lanes and VRM/shared-world composition remain open.

## Source damping runtime evidence (2026-10-31)

The optional native MMD session and shared usdview wrapper explicitly consume
installed owner `ReadSourceBodyProperties`, mapping independent finite rates or
pre-force prior-velocity clearing into the neutral descriptor. The owner
retains strict structural admission and original coefficients; core remains
provider-independent. Native `DampingNormalizations()` and Python copied
`dampingNormalizations` records expose original attribute paths/values and
effective coefficients, independently of unapplied spring metadata. Successful
Reset rereads declarations and replaces records; malformed source opinions
refuse with retained owner code/path and can be repaired/retried.

Local Windows/MSVC Release, OpenUSD 26.08, Python 3.13.14 and deterministic
Jolt 5.5.0/six-DOF verification uses rebuilt installed owner and physics
core/backend/USD bridge packages. Configuration and installed `mmd_session`
lookup require `mmdPhysicsAdapter_SOURCE_BODY_PROPERTIES_VERSION=1`,
`physicsCore_DAMPING_RESPONSE_VERSION=1` and
`physicsJolt_PRIOR_VELOCITY_CLEARING_AVAILABLE`; product versions alone do not
authorize mixing the descriptor layouts.

All 24 configured runtime/core/adapter/live CTests pass. The generated property
test checks surviving gravity at the unit endpoint against the 120 Hz analytic
response, and equality for normalized 2/unit and negative/zero controls. New
`hosts.usdview_mmd_damping` covers eight policies, three exact 120-tick replay
generations each, independent channels, finite coexistence, fresh keyed
constraint motion, unaffected kinematic commands, Pause, nonmutation and changed
source/failed-Reset repair. A separately compiled installed native-session
consumer exercises public normalization records and Reset without Python.
Neutral backend queued-force/contact evidence remains in the physics owner.

A bounded private PMX/VMD rerun uses no damping overrides or motion filtering,
with 58 normalization records and exact 120-tick replay after Reset. Raw intake
still refuses joints with no dynamic endpoint; the scoped rerun disables 10
inert kinematic/kinematic joints in an anonymous session layer. The MMD owner's
[dated report](https://github.com/animu-sphere/usd-mmd-plugins/blob/main/docs/reports/2026-10-31-phase8-source-damping-runtime.md)
records identities, measurements, compiler environment and limitations. This
is headless local evidence, not published packages, live renderer acceptance,
full-motion coverage, native MMD trajectories or representative L3 completion.
