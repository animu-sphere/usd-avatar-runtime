---
status: accepted
owner: usd-avatar-runtime
---

# Implementation and boundary cleanup policy

Adopted from the boundary plan supplied on 2026-10-06. The source's 21 section
numbers are preserved. This policy refines the original
[design policy](DESIGN_POLICY.md) and [near-term direction](NEAR_TERM_PLAN.md)
for ownership and cleanup. It does not claim that the migrations, conceptual
API names or target layouts below are implemented. Current facts remain in
the [capability matrix](../reference/CAPABILITY_MATRIX.md); actionable work and
acceptance gates remain in the [roadmap](../roadmap/current.md#boundary-cleanup-workstreams).

## 1. Purpose

`usd-avatar-runtime` is the ecosystem's **format-independent avatar evaluation
kernel, lifecycle and orchestration layer**. It owns avatar instances, frame
lifecycle, evaluator scheduling, phase ordering, capability discovery, common
input, resolved runtime state and publication.

Motion mathematics, VRM/MMD semantics, physics solvers and renderer
implementation belong to their respective owners.

## 2. Core responsibilities

| Area | Runtime responsibilities |
| --- | --- |
| lifecycle | instance creation/destruction, frame begin/evaluate/commit/rollback, retained snapshots, generation/frame identity, state isolation and deterministic ordering |
| evaluation plan | evaluator registration, dependency ordering, phase assignment, read/write domain declarations, capability negotiation, provider discovery and cycle/error detection |
| common contracts | `AvatarInputFrame`, scalar/gaze observations, evaluated pose/morph/material values, `EvaluatedAvatarState`, diagnostics and publication identity |
| publication | direct consumer API, Hydra and renderer fast-path adapters, recording publication adapter, debug/inspect/replay hosts |

These are responsibilities, not a list of already available APIs or tools.

## 3. Responsibilities owned elsewhere

| Owner | Responsibilities |
| --- | --- |
| `usd-motion-plugins` | `MotionPose`, `MotionClip`, `MotionStream`, sampling, interpolation, filtering, blending, retargeting, source rest, generic skeleton representation/conversion/validation, recording/replay algorithms and USD motion read/write |
| `usd-vrm-plugins` | humanoid rules, expressions, LookAt, VRM material semantics and VRMA interpretation |
| `usd-mmd-plugins` | morph semantics, PMX control rules, VMD semantics, IK and MMD evaluation |
| `motion-connectors` | device/protocol handling, external source normalization, source clocks, actor tracking and network transport |
| renderer | GPU resources, shaders, late-latching implementation, draw submission and backend optimization |

Physics ownership is detailed in section 11. Canonical sibling references are
maintained in [dependencies](../architecture/DEPENDENCIES.md#1-owners).

## 4. Adapter principle

An adapter receives owner-library values, invokes the owner evaluator or
algorithm, and marshals its result into the common runtime contract:

```text
owner representation -> thin adapter -> runtime contract
```

Registration, runtime binding checks, lifetime and publication support this
bridge. Domain algorithms must not be reimplemented inside adapters.

## 5. ClipPoseAdapter cleanup

Retain `ClipPoseAdapter` as the motion evaluator registration bridge:

```text
MotionClip / MotionPose -> owner sampler / retargeter -> ArStateWriter
```

Keep evaluator registration, phase/capability declarations, runtime layout
identity checks, runtime joint lookup, owner diagnostic forwarding and
owner-result-to-writer mapping.

Move generic motion invariant validation to `usd-motion-plugins` validation
APIs: timestamps/interpolation intervals, quaternion correctness, source-rest
hierarchy, confidence ranges, root-motion modes, human-joint vocabulary and
channel invariants. Runtime contract and owner-to-runtime binding validation
remain here. Extend missing owner APIs before removing required checks.

The target evaluation shape is conceptual:

```cpp
auto sample = motion::SampleClip(clip, time);
auto pose = retargeter.Retarget(*sample.pose, &diagnostics);
PublishPoseToRuntime(pose, writer);
```

Empty/held samples, errors and runtime transaction handling still need their
declared behavior. See [motion adapters](../architecture/MOTION_ADAPTER.md).

## 6. InputAssembler cleanup

Retain the `MotionPose -> AvatarInputFrame` bridge in this repository. Clarify
its name when implementing the cleanup; candidates are `MotionPoseInputBridge`,
`MotionInputBridge` and `MotionObservationBridge`. At adoption no rename was
selected; the implemented choice and compatibility names are recorded in
[motion adapters](../architecture/MOTION_ADAPTER.md#host-input-assembly).

It owns runtime channel mapping, source/actor identity attribution, owner values
to `ArScalarInput`/`ArGazeInput`, runtime input lifetime and unmapped-field
diagnostics. It preserves supplied identities and clocks; external actor
tracking and clock/identity normalization remain with `motion-connectors`.

Device clock normalization, network clock synchronization, protocol decoding
and generic motion channel normalization do not belong in this bridge.

## 7. StageClip cleanup

Phase out the current `motion-usd/StageClip` form. Motion stage reading, source
skeleton interpretation, source-rest generation and generic USD motion
validation belong to `usd-motion-plugins::motionUsd`.

Retain only a thin runtime integration helper that obtains the owner read
result and registers motion evaluation with target runtime bindings. The
intended shape is an owner read supplying clip/source rest followed by runtime
registration; it does not define a new `ReadMotionStage` signature. Do not
reconstruct a private equivalent of `MotionStageRead` here. See
[USD motion binding](../architecture/MOTION_USD_BINDING.md).

## 8. SkeletonBinding split

| Move to motion owner | Retain in runtime binding |
| --- | --- |
| UsdSkel joints to `SkeletonDescriptor`, rest decomposition, generic topology validation, skeleton conversion and motion source/target-rest generation | avatar-root identity, `layoutId`/`layoutVersion`, runtime `skeletonId`, joint-ID mapping, baseline `ArStateView`, runtime root placement and common-state layout registration |

```text
UsdSkel -> motionUsd::ReadSkeleton() -> SkeletonDescriptor
        -> AvatarSkeletonBinding -> ArStateView baseline
```

`ReadSkeleton` and `AvatarSkeletonBinding` are target concepts, not new public
APIs. Generic conversion/validation belongs upstream even when the current
adapter delegates some decomposition already. See
[USD skeleton binding](../architecture/USD_BINDING.md).

## 9. Core dependency policy

Keep `avatarRuntime` free of external provider dependencies. Its C ABI hides
STL implementation details and exposes scheduler, lifecycle, state storage,
diagnostic and registration contracts.

Optional integrations are separate targets. Intended families include
`avatarMotionAdapter`, `avatarUsdBinding`, `avatarVrmAdapter`,
`avatarVrmUsdBinding`, `avatarMmdAdapter` and `avatarImaging`. Linking or
finding core alone must not pull in OpenUSD, VRM, MMD or Hydra. Current target
names and availability are recorded in
[project layout](../architecture/PROJECT_LAYOUT.md).

The supplied plan also listed `avatarHydraToonBridge`. That family was
withdrawn on 2026-10-10: runtime-to-resident target matching belongs to a host
on the renderer's side, and the renderer's own adapter consumes retained
snapshots directly
([resident target matching](../architecture/OUTPUT_PATHS.md#resident-target-matching)).

## 10. Shared phase model

The phase graph is a runtime-owned abstraction. A conceptual cleanup model is:

```text
INPUT -> MOTION_SAMPLE -> RETARGET -> FORMAT_PRE_PHYSICS
      -> PHYSICS_BOUNDARY -> FORMAT_POST_PHYSICS
      -> EXPRESSION / LOOKAT / CONTROL -> RESOLVE -> PUBLISH
```

Keep common phase names independent of format semantics. VRM and MMD register
owner evaluators into this graph; runtime does not know their algorithms.
Validate semantic dependencies and atomic owner calls before adopting a
sequence. This sketch neither changes current `AR_PHASE_*` values nor moves
MMD pre-physics control/IK after physics. The
[evaluator contract](../contracts/EVALUATOR.md) owns phase decisions (RT-O3).

## 11. Physics boundary

```text
avatar pre-physics evaluation -> shared physics world -> physics result
                             -> avatar post-physics evaluation
                             -> EvaluatedAvatarState
```

Runtime owns boundary scheduling, input/output handoff, avatar state versioning
and deterministic phase ordering. It does not own a solver. Backend/world
lifetime, collisions, solver and fixed-step implementation belong to the
physics owner. Exact shared-world host integration remains RT-O6 in
[dependencies](../architecture/DEPENDENCIES.md#4-physics-boundary).

## 12. Recording boundary

```text
EvaluatedAvatarState -> RecordingPublicationAdapter
                    -> usd-motion-plugins recorder
```

Runtime publishes state; it does not implement a generic recorder. Storage
formats, resampling, compression and motion clip construction belong to the
motion owner. Avatar-specific non-motion recording formats require a separate
design. Runtime evaluation replay hosts compose configuration, provenance and
owner algorithms rather than implementing generic motion replay. See
[recording and replay](RECORDING_AND_REPLAY.md).

## 13. Hydra and fast-path boundary

```text
                     +-> Hydra adapter
EvaluatedAvatarState +-> fast-path adapter
```

Both consume the same source of truth. Renderers do not recalculate LookAt or
expression arbitration; runtime does not manage renderer resources.

Fast-path views may provide zero/minimal-copy access, frame/generation identity,
dirty masks, late publication and GPU-friendly packed views. These are allowed
transport optimizations, not implemented guarantees; canonical semantics remain
in `EvaluatedAvatarState`. Late-latching implementation remains renderer-owned.
See [output paths](../architecture/OUTPUT_PATHS.md).

## 14. Common runtime state

Keep common state format-independent: skeleton/joint transforms, morph weights,
material parameter overrides, visibility, resolved gaze and extensible named
values. Extensible value representation is still a design question.

Do not add core fields such as `vrmExpressionHappy`, `mmdBoneMorph` or
`pmxMaterialMorph`. Format adapters convert owner results to common resolved
state. Namespaced format identities may be retained as provenance without
requiring consumers to interpret format algorithms. The
[state contract](../contracts/EVALUATED_STATE.md) owns representation.

## 15. Diagnostics

Runtime diagnostics cover evaluator order, missing dependencies, capability
mismatch, lifecycle misuse, layout mismatch, transaction/publication failures
and owner diagnostic forwarding.

Do not redefine motion/VRM/MMD diagnostic codes. Preserve owner name, version
and subject when forwarding. Current callback records and provider descriptor
version metadata are distinct; a structured retained owner-version envelope
requires contract work. See
[capabilities and diagnostics](../contracts/CAPABILITIES_AND_DIAGNOSTICS.md).

## 16. Testing

Core tests cover lifecycle, rollback, retained snapshots, isolation, dependency
graphs, evaluator/phase order, malformed registration, capability discovery
and ABI compatibility.

Adapter tests check registration, invocation, marshaling, diagnostics and
lifetime using known owner input and expected evaluated state. Generic
sampling/retarget numerical correctness tests belong in the motion repository;
runtime comparisons with owner calls establish adapter conformance.

Integration acceptance includes motion -> retarget -> VRM LookAt/Expression ->
state -> `hydra-toon` fast-path, and motion -> MMD evaluator -> shared phase
model -> state. MMD is the second semantics provider proving generality.

## 17. Project layout

Recommended responsibility layout:

```text
include/avatarRuntime/
libs/runtime/ registry/ diagnostics/
adapters/motion/ usd/ vrm/ vrm-usd/ mmd/ hydra/
tools/inspect/ replay/ benchmark/
tests/
```

This is a target sketch, not a directory rename requirement. Owner algorithm
copies accumulating in adapters indicate a boundary violation. Actual and
planned components are distinguished in
[project layout](../architecture/PROJECT_LAYOUT.md).

## 18. Prohibited implementations

Do not duplicate motion samplers, retarget algorithms, generic skeleton
conversion, VRM expression algorithms or MMD IK/control. Do not embed physics
solvers or renderer shader/backend logic, spread format-specific branches
into output consumers, or replace a missing owner API with a private algorithm.

When a required API is missing, extend the owning repository.

## 19. Implementation workstreams

The supplied cleanup sequence uses A–F. These workstreams are distinct from
the original Runtime Phases A–F and evidence milestones A/B/C:

| Workstream | Scope |
| --- | --- |
| A: motion boundary | owner validation API, thin `ClipPoseAdapter`, bridge naming/documentation |
| B: USD/motion ownership | move StageClip domain work to `motionUsd`, split SkeletonBinding, remove duplicate skeleton/rest conversion |
| C: state validation | full real-motion VRM path, LookAt/expression, morph/material outputs, retained snapshots and fast-path parity |
| D: MMD second provider | registration, bone/morph/control dependencies, common phases without new format branches in core |
| E: publication | Hydra, `hydra-toon` fast-path, recording publication adapter and inspect/replay hosts |
| F: ABI stabilization | C/provider/consumer ABI revision review, capability versions, installed-consumer tests and OST composition evidence |

The [roadmap](../roadmap/current.md#boundary-cleanup-workstreams) owns remaining
work and acceptance. Early fast-path feedback still informs contract correction;
freeze follows real VRM/MMD evidence.

## 20. Completion conditions

Core remains independent of OpenUSD, VRM, MMD and renderers. Runtime contains
no motion algorithms or `motionUsd` domain logic. VRM and MMD use one scheduler
and state contract; Hydra and fast-path consume the same resolved state.
Recording uses a publication adapter, owner diagnostics survive forwarding,
and adapters are limited to marshaling, owner invocation and publication with
runtime registration/binding/lifetime checks.

## 21. Final position

`usd-avatar-runtime` is the **format-independent avatar evaluation IR,
lifecycle and orchestration kernel**:

```text
motion-connectors -> runtime input bridge -> AvatarInputFrame
                                                |
                                                v
motion / VRM / MMD owners -> usd-avatar-runtime kernel
                            lifecycle / scheduler / registry
                            capabilities / resolution / publication
                                                |
                                                v
                                     EvaluatedAvatarState
                                      /        |        \
                                   Hydra   fast-path   recording
```

It owns which evaluator runs for which avatar and frame, in which order, and
which common resolved state is published.
