---
status: accepted
owner: usd-avatar-runtime
---

# Near-term implementation direction

Adopted from the near-term policy supplied on 2026-10-05. This document owns
the current priorities and refines the sequencing in
[design policy section 20](DESIGN_POLICY.md). The original policy remains the
architectural baseline; the [current roadmap](../roadmap/current.md) owns
actionable work and acceptance gates. Implementation evidence remains in the
[capability matrix](../reference/CAPABILITY_MATRIX.md).

## 1. Objective

Prove the existing runtime contract with real avatar evaluators and renderer
consumers before adding new abstractions. The runtime is the format-independent
avatar evaluation IR, lifecycle and orchestration layer for the ecosystem.
[Repository ownership](../architecture/DEPENDENCIES.md) remains unchanged:
format owners supply semantics, motion owns generic mathematics, connectors
own intake, renderers own GPU realization and `open-strata` owns composition.

## 2. Contract validation before freeze

Runtime Phases A and B overlap. Synthetic providers are necessary boundary
tests, but cannot establish a stable ecosystem ABI by themselves:

```text
contract design -> synthetic tests -> real provider integration
                                      -> contract correction -> ABI freeze
```

Runtime Phase A acceptance includes real motion, VRM and MMD provider evidence.
An early Runtime Phase C direct consumer also feeds contract correction;
full Hydra publication follows that evidence.

## 3. First vertical slice: VRM

```text
motion-connectors / test input
              |
       AvatarInputFrame
              |
     usd-motion-plugins
              |
     VRM Humanoid binding
              |
          VRM LookAt
              |
        VRM Expression
              |
     EvaluatedAvatarState
              |
     hydra-toon fast-path
```

Start with direct serial execution. OpenExec, a Hydra Scene Index and physics
are not prerequisites. Test input can bring up the path; milestone acceptance
requires real motion and a real VRM avatar.

## 4. Central evaluated-state IR

The minimum logical channels are rig pose, morph/deformation weights,
expression state, gaze/LookAt results, visibility, runtime appearance overrides
and diagnostics/provenance. The [state contract](../contracts/EVALUATED_STATE.md)
owns their representation and distinguishes target requirements from the
implemented experimental subset.

Contract hardening must settle joint and skeleton layout/version identity,
transform representation and spaces, complete/sparse/delta semantics, absence
versus identity, and binding invalidation. Gaze input and resolved output are
separate. Expressions retain semantic/native identity, weight, provenance,
arbitration outcome and availability without moving format rules into core.

Snapshots need frame/instance/layout identity, evaluation time, source/input
revision and active capabilities. Consumers must distinguish value updates
from structural rebinding. Do not assume configuration generation alone is
a layout version.

## 5. Validate evaluator dependencies

The first VRM plan validates:

```text
motion pose -> humanoid resolution -> LookAt -> expression arbitration
                                                  -> final evaluated state
```

MMD is the second evaluator family and a test of scheduler generality:

```text
base pose -> bone morph -> control / IK -> physics boundary
                                             -> final pose / morph state
```

These are semantic dependencies, not a requirement to split an owner's atomic
call. The [evaluator contract](../contracts/EVALUATOR.md) owns the phase mapping.
Correct the shared model using real evidence; do not optimize it only for VRM.

## 6. First renderer consumer

Connect retained snapshots to `hydra-toon`'s direct/fast-path before freezing
Hydra locators or Scene Index structure. This supplies an initial consumer for
correctness, latency, copy cost and allocation measurements.

The renderer consumes resolved state and realizes GPU buffers/draws. It does
not evaluate LookAt, expression arbitration, MMD morphs, IK or retargeting.
The runtime does not author high-frequency results to the stage every frame.

## 7. One state, multiple publication paths

```text
                     +-- fast/direct API -> hydra-toon
EvaluatedAvatarState +-- Hydra adapter --> Hydra render delegates
                     +-- recording/bake
```

All paths share frame/state identity and evaluated values. A Hydra adapter is
a publication mechanism, not a second semantic evaluator. The
[output contract](../architecture/OUTPUT_PATHS.md) owns parity requirements.

## 8. Immediate runtime additions

- Registration adapters for motion, VRM and MMD marshal owner values into the
  common contract without leaking provider-private types into the core ABI.
- A USD binding adapter builds instance configuration from avatar root,
  skeleton/joint mapping, format identity, expression bindings, LookAt
  configuration and material/deformation target identities.
- Snapshot metadata identifies layout/value changes and input provenance.
- Integration diagnostics preserve provider identity and explain missing
  joints, unsupported expressions, invalid gaze spaces, stale input, capability
  mismatch, layout mismatch and unsupported outputs before renderer consumption.

## 9. Deferred prerequisites

Do not make OpenExec, parallel evaluator scheduling, multi-avatar parallelism,
full physics, Web/WASM/XR packaging, full Hydra Scene Index publication,
comprehensive recording/replay tooling or a frozen stable ABI blockers for the
first vertical slice. Establish end-to-end correctness through the serial
direct runtime first.

## 10. Physics boundary

```text
avatar pre-physics evaluation
              |
shared physics world / usd-stage-runner / physics runtime
              |
        physics result
              |
avatar post-physics evaluation -> EvaluatedAvatarState
```

The runtime owns avatar ordering, not a physics implementation. Backend/world
lifetime, fixed timestep and shared-world stepping ownership remain RT-O6.
Keep MMD/VRM physics meaning with their owners. The MMD slice validates the
boundary without requiring a complete physics integration.

## 11. Optional OpenExec

Direct invocation and an OpenExec wrapper use the same evaluator implementation
and state contract. `AvatarRuntime` must remain fully functional without
OpenExec for direct renderer integration, small hosts and future Web/WASM use.

## 12. Validation tiers

Preserve synthetic ABI/lifetime/rollback/reset/isolation/capability/dependency
tests. Add real motion, VRM LookAt/Expression and MMD morph/control/IK evidence,
then input-to-fast-path pose/expression/gaze numeric parity. Later compare
resolved fast-path and Hydra outputs. Pixel parity is a separate renderer test.
The [roadmap](../roadmap/current.md#validation-tiers) owns the acceptance matrix.

## 13. Implementation sequence

1. Harden pose, gaze, expression and snapshot/layout contracts using provider needs.
2. Connect callable VRM Humanoid, LookAt and Expression boundaries with a real avatar.
3. Map owner motion values and connector input into the runtime without a competing vocabulary.
4. Add the `hydra-toon` retained-snapshot consumer and measure update costs.
5. Connect MMD bone morph/control/IK and validate scheduler and physics boundaries.
6. Establish a Runtime Phase A freeze candidate using VRM and MMD evidence.
7. Implement Hydra publication using the validated state semantics.

Steps 2 and 3 together complete the first evaluation slice; bringing up VRM
with test input does not replace motion integration evidence.

## 14. Evidence milestones

- Milestone A: a real VRM avatar receives real motion input, evaluates
  LookAt/Expression and produces `EvaluatedAvatarState`.
- Milestone B: `hydra-toon` consumes that same state through its fast-path and
  renders in real time.
- Milestone C: a real MMD evaluator operates on the same scheduler/state model.

These evidence milestones are distinct from Runtime Phases A–F. Together they
test the principal hypotheses of a format-independent evaluation runtime.

## 15. Success and placement rules

Success requires direct execution without OpenExec/Hydra, one evaluated state
across publication paths, real VRM and MMD conformance, no per-frame stage
authoring, no renderer callbacks into evaluation, and no new format branch in
core or renderer when adding a format. ABI freeze follows real-provider evidence.

Place orchestration/state/lifecycle work here; VRM/MMD/motion meaning in the
owning repository; rendering/GPU realization in the renderer; and device,
protocol and input acquisition in `motion-connectors`. Preserve this boundary
when choosing the next feature or abstraction.
