---
status: accepted
owner: usd-avatar-runtime
---

> Adopted from the implementation policy supplied on 2026-10-05. The original
> 22 section numbers are preserved. This document owns architectural intent;
> it does not claim an implemented runtime or a frozen ABI. All code snippets,
> interface names, capability names and directory sketches are conceptual.
>
> The focused documents in [the documentation index](../README.md) develop
> these concepts. Their contract proposals remain `proposed` until the
> [Runtime Phase A acceptance criteria](../roadmap/current.md#runtime-phase-a--freeze-contracts)
> are met. In particular, the `timeCode` sketch in section 5 does not define
> the canonical motion clock; see [input time mapping](../contracts/INPUT_FRAME.md#2-time-and-coordinate-boundaries).
> Current implementation facts belong to the
> [capability matrix](../reference/CAPABILITY_MATRIX.md).

# usd-avatar-runtime — Implementation and Extension Policy

## 1. Purpose

`usd-avatar-runtime` is the runtime-composition and orchestration layer for avatar behavior across the animu-sphere ecosystem.

Its primary role is not to reimplement VRM, MMD, motion, rendering, or tracking semantics. Instead, it defines how those independently owned capabilities are composed into a coherent avatar runtime.

The long-term goal is for `usd-avatar-runtime` to become the practical specification space for avatar evaluation in the ecosystem:

```text
external world
    ↓
motion-connectors
    ↓
input intents / observations
    ↓
usd-avatar-runtime
    ↓
format-specific + generic evaluators
    ↓
evaluated avatar state
    ↓
Hydra path | fast-path | recording/export
```

---

## 2. Core Design Principle

The repository should own **evaluation orchestration**, not format semantics.

Responsibility split:

```text
motion-connectors
    external input → intent / observations

usd-motion-plugins
    time, pose, sampling, blending, retargeting, recording

usd-vrm-plugins
    VRM-specific semantics and evaluators

usd-mmd-plugins
    MMD-specific semantics and evaluators

usd-avatar-runtime
    ordering, composition, lifecycle, runtime state, scheduling

hydra-toon
    evaluated state → pixels
```

Key rule:

> Format plugins own meaning. `usd-motion-plugins` owns generic motion mathematics. `usd-avatar-runtime` owns evaluation order and orchestration. Renderers own realization.

---

## 3. Repository Boundary

### 3.1 `usd-avatar-runtime` should own

- OST runtime composition for avatar-related packages.
- Evaluator discovery and registration.
- Evaluation phase ordering.
- Runtime frame lifecycle.
- Common input and output contracts.
- Cross-plugin orchestration.
- Runtime state aggregation.
- Optional Hydra Scene Index output.
- Optional fast-path output contracts.
- Diagnostics, tracing, profiling, and deterministic replay hooks.
- Runtime feature/capability negotiation.

### 3.2 `usd-avatar-runtime` should not own

- VRM parsing.
- MMD parsing.
- VRM expression rules.
- VRM LookAt range maps.
- MMD morph semantics.
- MMD IK implementation details.
- Generic retarget math already owned by `usd-motion-plugins`.
- Device-specific MediaPipe/WebXR/OpenXR adapters.
- MToon or MMD shading logic.
- Vulkan/WebGPU backend details.

---

## 4. Canonical Runtime Data Flow

Recommended runtime architecture:

```text
                    external world
                         │
                         ▼
                 motion-connectors
                         │
                         ▼
                 AvatarInputFrame
                         │
                         ▼
                usd-avatar-runtime
                         │
          ┌──────────────┼──────────────┐
          ▼              ▼              ▼
 usd-motion-plugins usd-vrm-plugins usd-mmd-plugins
          │              │              │
          └──────────────┼──────────────┘
                         ▼
               EvaluatedAvatarState
                         │
          ┌──────────────┼──────────────┐
          ▼              ▼              ▼
      Hydra path      fast-path      recording
```

The runtime should treat the composed USD stage as authored state, while high-frequency evaluated state may remain runtime-only.

---

## 5. Common Input Contract

Introduce a renderer- and format-independent input layer.

Suggested concept:

```cpp
struct AvatarInputFrame {
    double timeCode;

    MotionInput motion;
    GazeInput gaze;
    ExpressionIntentState expressions;
    UserParameters parameters;
};
```

The exact ABI should remain compact and extensible.

### 5.1 Input categories

#### Motion input

- semantic pose
- root motion
- streamed body tracking
- clip playback state
- locomotion intent

#### Gaze input

Prefer high-level intent rather than VRM-specific values:

```text
world-space target
head-relative target
gaze direction
optional left/right eye observations
```

#### Expression intent

Use semantic intent where practical:

```text
blink
smile
mouthOpen
jawOpen
angry
happy
sad
surprised
custom channels
```

These are not VRM expression names or MMD morph names. Format adapters resolve them downstream.

---

## 6. Common Evaluated Output Contract

The most important shared boundary should be a renderer-neutral evaluated state.

Recommended logical decomposition:

```cpp
struct AvatarPoseState {
    MotionPose pose;
};

struct AvatarDeformationState {
    BlendShapeWeightsView blendShapes;
    DeformationChannelsView channels;
};

struct AvatarAppearanceState {
    MaterialOverrideView materials;
    VisibilityState visibility;
};

struct EvaluatedAvatarState {
    AvatarPoseState pose;
    AvatarDeformationState deformation;
    AvatarAppearanceState appearance;
};
```

The output should represent resolved effects rather than source-format semantics.

Examples:

```text
VRM Expression
    ↓
blend-shape weights + material overrides

MMD Morph
    ↓
blend-shape weights + bone deltas + material overrides

VRM LookAt
    ↓
eye rotations and/or expression weights
```

This is the convergence point for multiple avatar formats.

---

## 7. Evaluation Phase Order

The runtime should explicitly own the evaluation sequence.

Recommended initial phase model:

```text
0. Input collection
1. Timeline / clip sampling
2. Motion blending
3. Retargeting
4. Base pose construction
5. Constraints / IK
6. LookAt / gaze evaluation
7. Expression / morph evaluation
8. Secondary motion / physics
9. Final pose consolidation
10. Appearance/material overrides
11. Output publication
```

This ordering should be configurable only where necessary. Avoid allowing every plugin to invent its own independent update loop.

### Why ordering belongs here

LookAt, expressions, constraints, and physics may all modify overlapping state. A shared orchestrator avoids hidden dependency chains and nondeterministic execution.

---

## 8. Evaluator Interface

A small evaluator contract should be introduced.

Conceptually:

```cpp
class IAvatarEvaluator {
public:
    virtual EvaluatorPhase GetPhase() const = 0;
    virtual EvaluatorCapabilities GetCapabilities() const = 0;
    virtual void Evaluate(
        const AvatarEvaluationContext& context,
        AvatarMutableState& state) = 0;
};
```

Recommended properties:

- deterministic for identical inputs
- no renderer dependency
- no backend dependency
- explicit read/write state declarations where practical
- structured diagnostics
- optional profiling hooks

A data-oriented callback table or C ABI may be preferable to a C++ virtual ABI for cross-package stability.

---

## 9. C ABI / ABI Stability

Because the runtime will compose multiple separately versioned repositories, define a small stable boundary.

Recommended direction:

- C-compatible ABI for evaluator registration and frame execution.
- Versioned structs with `struct_size` / `version` fields.
- Opaque handles for runtime-owned objects.
- Span/view-style arrays rather than STL types across ABI boundaries.
- Stable token/string identifiers for semantic channels.

Example direction:

```cpp
struct AvatarRuntimeApiV1 {
    uint32_t version;
    uint32_t structSize;

    AvatarStatus (*registerEvaluator)(...);
    AvatarStatus (*evaluateFrame)(...);
};
```

C++ convenience wrappers can live above this ABI.

---

## 10. OpenExec Integration

OpenExec should be treated as an **evaluation execution mechanism**, not the only runtime architecture.

Recommended layering:

```text
evaluator core
   ├─ direct C++/C ABI call
   └─ OpenExec node wrapper
```

Examples:

```text
vrm.computeLookAt
vrm.applyLookAtToPose
vrm.resolveExpressions
motion.sampleClip
motion.retargetPose
```

The evaluator core should remain callable without OpenExec so that:

- fast-path runtimes can stay lightweight;
- OpenExec API changes do not destabilize the evaluator logic;
- Web/WASM targets can choose a smaller integration surface;
- deterministic unit testing remains simple.

---

## 11. Hydra Output Path

Hydra should be the interoperability-oriented delivery path.

Recommended model:

```text
UsdImagingStageSceneIndex
        ↓
AvatarEvaluationSceneIndex
        ↓
renderer
```

The avatar evaluation Scene Index may overlay runtime-evaluated values such as:

- skeleton transforms
- blend-shape weights
- material parameter overrides
- visibility
- other high-frequency runtime state

Do not require that all of these values be authored back to the USD stage every frame.

### Important distinction

```text
authored state → USD stage
runtime state  → runtime overlay / Scene Index
```

Recording or baking may explicitly write runtime state back to USD.

---

## 12. Fast-Path Output

The runtime should support a non-Hydra path for latency-sensitive applications.

```text
EvaluatedAvatarState
        ↓
fast consumer API
        ↓
hydra-toon fast adapter / other renderer
```

The fast-path should not bypass evaluation semantics. It should bypass only the Hydra transport layer.

Therefore:

```text
same evaluated state
   ├─ Hydra adapter
   └─ fast adapter
```

not:

```text
VRM evaluator inside renderer
MMD evaluator inside renderer
```

---

## 13. Recording and Replay

A runtime of this type benefits greatly from deterministic capture.

Add a frame capture format that records:

- input intents
- motion stream samples
- evaluator versions
- evaluated avatar state
- diagnostics
- timestamps

Use cases:

- regression tests
- offline debugging
- performance comparisons
- Hydra vs fast-path parity tests
- reproducible agent workflows

Where possible, reuse `usd-motion-plugins` recording primitives rather than creating a parallel motion format.

---

## 14. Capability Discovery

The runtime should be able to answer questions such as:

```text
Does this avatar support humanoid retargeting?
Does it support LookAt?
Does it expose expression channels?
Does it require MMD morph evaluation?
Does it provide secondary-motion semantics?
Can this renderer consume the evaluated appearance state?
```

Recommended approach:

```text
capability tokens + versions
```

rather than compile-time assumptions.

Example capability names:

```text
avatar.pose.humanoid
avatar.lookAt
avatar.expression
avatar.morph
avatar.secondaryMotion
avatar.appearance.materialOverride
```

---

## 15. Diagnostics and Observability

Make runtime evaluation easy to inspect.

Recommended diagnostics:

- phase timing
- evaluator timing
- missing semantic mappings
- dropped connector channels
- unresolved expression intent
- unsupported morph/material outputs
- dependency cycles
- non-finite transform detection
- per-frame state hashes

A debug dump should be able to show:

```text
input → evaluator → affected channels → final state
```

This will be especially valuable for coding-agent workflows.

---

## 16. Threading

Do not make evaluator execution implicitly parallel at first.

Start with deterministic ordered evaluation.

Later, add a scheduler using explicit state dependencies:

```text
read-set / write-set
```

Potential parallel areas:

- independent expression groups
- independent material overrides
- multiple avatars
- secondary motion chains
- deformation preparation

Multi-avatar parallelism is likely the safest first optimization.

---

## 17. Multi-Avatar Architecture

The runtime should avoid global singleton state.

Model:

```text
AvatarRuntime
   ├─ AvatarInstance A
   ├─ AvatarInstance B
   └─ AvatarInstance C
```

Each instance owns:

- format bindings
- evaluator graph
- state buffers
- connector bindings
- output bindings

Shared resources may include:

- evaluator code
- static skeleton metadata
- material resources
- motion clips

---

## 18. Repository Layout Proposal

```text
usd-avatar-runtime/
  cmake/
  docs/
    architecture/
    contracts/
    roadmap/
  include/
    avatarRuntime/
      api.h
      evaluator.h
      input.h
      state.h
      capabilities.h
      diagnostics.h
  libs/
    avatarCore/
    avatarRuntime/
    avatarRegistry/
    avatarDiagnostics/
  plugins/
    avatarImaging/          # optional Hydra Scene Index / adapter layer
    execAvatar/             # optional OpenExec integration
  tools/
    avatarInspect/
    avatarReplay/
    avatarBenchmark/
  tests/
    contracts/
    parity/
    replay/
```

Keep OST composition metadata separate from reusable runtime libraries.

---

## 19. Extension Policy

### Add to `usd-avatar-runtime` when

- the feature coordinates multiple avatar subsystems;
- it defines cross-format execution order;
- it defines a common runtime contract;
- it publishes evaluated state to consumers;
- it provides runtime-wide diagnostics or lifecycle management.

### Do not add when

- the behavior is specific to VRM, MMD, or another source format;
- it is generic motion math already suitable for `usd-motion-plugins`;
- it is an external device/input connector;
- it is a renderer-specific realization.

---

## 20. Recommended Milestones

### Phase A — Freeze contracts

- `AvatarInputFrame`
- `EvaluatedAvatarState`
- evaluator phase enum
- capability tokens
- diagnostics model

### Phase B — Integrate current repositories

- `usd-motion-plugins` evaluator adapter
- `usd-vrm-plugins` LookAt / expression adapter
- `usd-mmd-plugins` morph / IK adapter
- `motion-connectors` input bridge

### Phase C — Output paths

- Hydra Scene Index prototype
- direct fast-path API
- parity tests between both paths

### Phase D — OpenExec

- wrap stable evaluator cores in OpenExec nodes
- retain direct-call fallback

### Phase E — Runtime quality

- deterministic replay
- profiling
- multi-avatar scheduling
- structured capability negotiation

### Phase F — Web / XR / application runtime

- WASM-friendly ABI
- WebXR input composition
- WebGPU fast consumer
- reduced runtime profile

---

## 21. Non-Goals

Do not turn `usd-avatar-runtime` into:

- a renderer;
- a VRM implementation;
- an MMD implementation;
- a tracking SDK;
- a monolithic game engine;
- an alternative to OpenUSD composition.

It should remain the coordination layer that makes those systems work together.

---

## 22. Final Architectural Rule

The runtime should preserve this dependency direction:

```text
source formats / connectors
        ↓
semantic evaluators
        ↓
usd-avatar-runtime orchestration
        ↓
evaluated state
        ↓
Hydra | fast-path | recording
```

Never invert it so that renderers call back into format-specific behavior evaluators.

This boundary is what allows VRM, MMD, future avatar formats, XR tracking, AI agents, and multiple renderers to coexist without creating pairwise integrations between every repository.
