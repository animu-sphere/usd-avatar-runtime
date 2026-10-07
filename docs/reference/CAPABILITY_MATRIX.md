# Capability matrix

Checked against this repository on 2026-10-07. This is implementation status,
not a promise about sibling repositories or a release/support declaration.
The experimental direct runtime and CMake installation/export are implemented.
Runtime Phase A remains open; revision 3 is not a frozen ecosystem ABI.

The [boundary cleanup policy](../design/BOUNDARY_POLICY.md) is accepted intent.
The motion-boundary slice is implemented: `ClipPoseAdapter` delegates generic
invariants to installed owner validation APIs, and `MotionPoseInputBridge`
retains `InputAssembler` source aliases. Owned owner reports preserve
code/subject/detail and package identity and can be emitted to C sinks.
The scoped USD reader split is also implemented: `SkeletonBinding` delegates
generic matrix/token/topology/unit/placement checks to installed
`motionUsd::ReadMotionSkeleton` with the explicit `Generic` role, and `StageClip`
retains the coherent clip/descriptor/source-rest result of `ReadCanonicalMotionStage`.
Neither binding rebuilds the owner descriptor or source rest. Reading refusals
retain owner code/subject/detail and package identity/version in `MotionUsdReadError`.
WS-O4 is resolved upstream; final StageClip wrapper absorption remains on the
[roadmap](../roadmap/current.md#boundary-cleanup-workstreams).

| Surface | Current status | Owning documentation |
| --- | --- | --- |
| implementation direction | adopted documentation; real-provider/fast-path integration prioritized before freeze | [design policy](../design/DESIGN_POLICY.md), [near-term direction](../design/NEAR_TERM_PLAN.md) |
| runtime architecture and layout | one reusable `avatarRuntime` target; further component split proposed | [overview](../architecture/OVERVIEW.md), [layout](../architecture/PROJECT_LAYOUT.md) |
| input/evaluated-state contracts | experimental revision-3 C headers: attributed scalars and typed gaze point/direction observations with explicit world/joint-local space, validity and clock mappings; host input revision and dense snapshots with retained layout ID/version and active capabilities; configured owner clip-to-pose mapping and owned selected-motion scalar/world-gaze assembly implemented; live observation intake, multi-source composition, expression/gaze result records and other deformation channels pending | [input](../contracts/INPUT_FRAME.md), [state](../contracts/EVALUATED_STATE.md) |
| evaluator registration, phase execution and lifecycle | direct serial plan, dependencies/cycles/write validation, instance state, commit/abort/reset implemented; real-provider phase conformance and checkpoint restore pending | [evaluator](../contracts/EVALUATOR.md), [lifecycle](../architecture/FRAME_LIFECYCLE.md) |
| versioned C ABI | revision-3/size validation, revision-1/2 rejection, scoped handles, retain/release and installed C provider-consumer tests; ABI freeze pending | [ABI](../contracts/ABI.md) |
| capability negotiation and diagnostics | exact-version provider/binding intersection, required support checks, ordered bounded diagnostics and provider provenance implemented; semantic vocabulary/output negotiation pending | [capabilities/diagnostics](../contracts/CAPABILITIES_AND_DIAGNOSTICS.md) |
| motion validation boundary | installed owner clip/retarget validators invoked before retargeter construction; selected timestamp/scalar/gaze validation delegated by MotionPoseInputBridge; runtime binding/clock/C-string checks retained; owned MotionValidationError and synchronous C sink forwarding preserve owner report identity/order and package version; old InputAssembler source names retained | [motion adapters](../architecture/MOTION_ADAPTER.md) |
| selected USD motion inputs | StageClip forwards owner MotionStageReadOptions through the strict reader; tool attribute selection, clip-to-world rigid gaze placement and untouched world probes implemented; generated native VRMA expression/keyed/default gaze fixtures drive an actual avatar with owner parity; automatic native discovery and representative capture evidence pending | [USD motion binding](../architecture/MOTION_USD_BINDING.md) |
| motion/VRM/MMD/connector integration | optional installed owner clip sampling/retarget-to-pose adapter, host scalar/world-gaze input assembler and `vrmRig` expression + expression/bone LookAt adapter; explicit clock/input/layout/joint/output and eye/rest binding and diagnostics; constructed motion -> assembled input -> VRM and gaze-space numeric parity tested; schema-derived Humanoid mapping plus seven real VRMA clips on one private avatar via owned USD clip/source rest, with all-joint parity and reset/retention; full avatar bindings, connectors, multi-source selection and MMD pending | [motion adapters](../architecture/MOTION_ADAPTER.md), [USD motion binding](../architecture/MOTION_USD_BINDING.md), [VRM adapter](../architecture/VRM_ADAPTER.md), [VRM USD binding](../architecture/VRM_USD_BINDING.md), [dependencies](../architecture/DEPENDENCIES.md) |
| USD skeleton binding | optional authored skeleton/rest extraction into owned baseline and owner `SkeletonDescriptor`/`RetargetMap`; explicit Humanoid roles, metre conversion, auxiliary joints and rigid root placement; separate optional owner schema Humanoid discovery with custom-role reporting; bounded affine roundoff; constructed USD -> motion -> VRM parity and one local real-avatar skeleton/Humanoid result; scoped expression/output extraction available separately | [USD binding](../architecture/USD_BINDING.md), [VRM USD binding](../architecture/VRM_USD_BINDING.md) |
| Hydra state overlay and direct consumer API | retained direct snapshot API implemented; optional motion-check host consumes installed Toon AvatarState for joint/morph/material probe transport, duplicate/reset/rebind/retention and late draw-value parity; actual resident-avatar binding/rendering, consumer-cost evidence and Hydra/direct parity pending | [output paths](../architecture/OUTPUT_PATHS.md) |
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
final `StageClip` wrapper absorption remains workstream B work. No package
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
paths/hashes and logs remain ignored local evidence. Final wrapper absorption,
representative native capture, renderer evidence and ABI freeze remain open.
