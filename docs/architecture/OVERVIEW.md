---
status: proposed
owner: usd-avatar-runtime
---

# Runtime architecture

This is the target architecture, following
[design policy sections 1–4 and 17](../design/DESIGN_POLICY.md).
Implementation status is the [capability matrix](../reference/CAPABILITY_MATRIX.md).

The adopted [boundary cleanup policy](../design/BOUNDARY_POLICY.md) specifies
the kernel's ownership: lifecycle, scheduling, registration, capabilities,
common state and publication. Owner algorithms are invoked through thin
adapters. Motion validation and USD skeleton/rest interpretation are upstream
responsibilities; current adapter overlap is tracked as cleanup work.

The [near-term direction](../design/NEAR_TERM_PLAN.md) prioritizes real motion
-> VRM Humanoid/LookAt/Expression -> evaluated state -> `hydra-toon` fast-path,
then MMD under the same scheduler/state model. Runtime Phases A/B overlap to
validate and correct contracts before freeze. OpenExec, full Hydra publication
and physics are not prerequisites for that serial direct slice.

## 1. Composition and evaluation

```text
OST composition -> packages, plugins and host environment
USD composition -> authored avatar, bindings and static resources
connector bridge -> immutable AvatarInputFrame
registry + bindings -> ordered evaluator plan
frame execution -> EvaluatedAvatarState + diagnostics
publication -> Hydra | direct consumer | recording/bake
```

OST adoption and package composition configure a usable environment. Reusable
runtime libraries own frame execution independently of that configuration.
OST metadata is not a substitute for the runtime scheduler.

The composed stage remains the source of authored structure, format bindings
and static values. A binding adapter reads those declarations into evaluation
data. Format-specific interpretation stays with the format owner.
High-frequency resolved values live in runtime buffers and output overlays.
Only explicit recording/baking authoring writes those results back to USD.

The planned USD binding adapter builds instance configuration from avatar root,
skeleton/joint mapping, format identity, expression bindings, LookAt
configuration and material/deformation target identities. It preserves owner
semantics and validates layout/version identity and invalidation before frames.
It is distinct from high-frequency value publication. Revision 3 core accepts
caller-supplied layouts; optional [USD skeleton](USD_BINDING.md) and
[VRM USD](VRM_USD_BINDING.md) binders now supply scoped configuration. Generic
skeleton conversion in the former still needs the owner/runtime split described
by the boundary policy; no full format-independent binding system is claimed.

## 2. Instances and resources

An `AvatarRuntime` contains independent `AvatarInstance`s. Each instance owns
its format bindings, ordered evaluator plan, input/connector bindings,
mutable working state, prior-frame evaluator state and output bindings.
There is no global active avatar or singleton mutable evaluation state.

Evaluator code, immutable skeleton metadata, clips and material resources can
be shared. Sharing does not imply shared writable buffers or implicit time.
Handle scope, retained output ownership and invalidation are the
[ABI proposal](../contracts/ABI.md)'s.

## 3. Execution mechanisms

```text
format or generic evaluator core
       |                  |
direct invocation    OpenExec wrapper
       \                  /
          runtime phase plan
                 |
          common evaluated state
```

OpenExec is an optional mechanism. Nodes wrap evaluator/library calls;
they do not duplicate algorithms or define another update loop. Generic
motion and format nodes remain with their respective owners. `execAvatar`
is the optional orchestration integration, not a new VRM or MMD evaluator.

The direct path must remain usable without OpenExec. Whether its initial
packages also require OpenUSD value libraries is a dependency question, not
an assumption that the existing motion packages are USD-free. A compact C ABI
alone does not establish WASM portability.

## 4. Scheduling

The host supplies time and drives frames. Evaluation begins with deterministic
serial execution, using the
[evaluator phase proposal](../contracts/EVALUATOR.md). Dependencies and state
writes are explicit. Later scheduling can exploit independent read/write
sets, starting with independent avatars where resource ownership permits it.

The host/stage-runner boundary for shared physics stepping needs agreement;
the runtime owns ordering of avatar contributions, rather than silently
claiming a physics world or backend. Boundary handoff and avatar state versioning
belong here; solver, collision and fixed-step implementation stay outside runtime. See
[dependency questions](DEPENDENCIES.md#3-integration-questions).
